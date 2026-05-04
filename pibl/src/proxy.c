/*
 * =============================================================================
 * PIBL - proxy.c - Reenvio de peticiones hacia backends (proxy forwarding)
 * =============================================================================
 *
 * Este archivo cubre RF-03 (crear socket cliente al backend) y la mitad
 * "leer del backend" del RF-04 (esperar respuesta del backend).
 * El reenvio al cliente original (la otra mitad del RF-04) ocurre en server.c.
 *
 * Flujo de proxy_forward():
 *   1. socket() + setsockopt(SO_RCVTIMEO/SO_SNDTIMEO=5s)
 *   2. connect() al backend (si falla -> -1, el caller probara otro)
 *   3. write() loop: enviar el request integro al backend
 *   4. read() loop: acumular la respuesta hasta:
 *        a) Content-Length completo  (RF-04: "Detecta fin por Content-Length")
 *        b) o cierre de conexion EOF (RF-04: "...o cierre de conexion")
 *        c) o buffer lleno
 *   5. close(backend_fd) y devolver bytes en el buffer
 * =============================================================================
 */

#define _GNU_SOURCE

#include "proxy.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#define PROXY_TIMEOUT_SEC 5

/*
 * Helper: busca "Content-Length:" dentro de la zona de headers del buffer.
 * Devuelve el valor del header o -1 si no aparece o es invalido.
 * Es case-insensitive (RFC 2616).
 */
static long find_content_length(const char *buf, size_t headers_len) {
    const char *p = buf;
    const char *end = buf + headers_len;
    static const char NEEDLE[] = "Content-Length:";
    const size_t NEEDLE_LEN = sizeof(NEEDLE) - 1;

    while (p < end) {
        const char *line_end = memchr(p, '\r', (size_t)(end - p));
        if (!line_end || line_end + 1 >= end || line_end[1] != '\n') {
            break;
        }
        size_t line_len = (size_t)(line_end - p);
        if (line_len >= NEEDLE_LEN && strncasecmp(p, NEEDLE, NEEDLE_LEN) == 0) {
            const char *v = p + NEEDLE_LEN;
            while (v < line_end && (*v == ' ' || *v == '\t')) v++;
            return atol(v);
        }
        p = line_end + 2;
    }
    return -1;
}

static int connect_to_backend(const char *backend_ip, int backend_port) {
    struct addrinfo hints;
    struct addrinfo *servinfo = NULL;
    struct addrinfo *p;
    char port_str[16];
    int status;
    int backend_fd = -1;
    struct timeval tv = { .tv_sec = PROXY_TIMEOUT_SEC, .tv_usec = 0 };

    snprintf(port_str, sizeof(port_str), "%d", backend_port);
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    status = getaddrinfo(backend_ip, port_str, &hints, &servinfo);
    if (status != 0) {
        fprintf(stderr, "[PIBL] getaddrinfo %s:%d fallo: %s\n",
                backend_ip, backend_port, gai_strerror(status));
        return -1;
    }

    for (p = servinfo; p != NULL; p = p->ai_next) {
        backend_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (backend_fd < 0) {
            continue;
        }

        setsockopt(backend_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(backend_fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

        if (connect(backend_fd, p->ai_addr, p->ai_addrlen) == 0) {
            break;
        }

        close(backend_fd);
        backend_fd = -1;
    }

    freeaddrinfo(servinfo);

    if (backend_fd < 0) {
        fprintf(stderr, "[PIBL] connect %s:%d fallo: %s\n",
                backend_ip, backend_port, strerror(errno));
    }

    return backend_fd;
}

int proxy_forward(const char *backend_ip, int backend_port,
                  const char *request, size_t req_len,
                  char *response, size_t *resp_len, size_t resp_max) {
    if (!backend_ip || !request || !response || !resp_len || resp_max == 0) {
        return -1;
    }
    *resp_len = 0;

    /* Paso 1: getaddrinfo() + socket() + connect(), siguiendo el patron Beej */
    int backend_fd = connect_to_backend(backend_ip, backend_port);
    if (backend_fd < 0) {
        return -1;
    }

    /* Paso 5: reenviar la peticion integra (write puede ser parcial) */
    size_t sent = 0;
    while (sent < req_len) {
        ssize_t n = write(backend_fd, request + sent, req_len - sent);
        if (n < 0) {
            if (errno == EINTR) continue;
            fprintf(stderr, "[PIBL] write a backend %s:%d fallo: %s\n",
                    backend_ip, backend_port, strerror(errno));
            close(backend_fd);
            return -1;
        }
        if (n == 0) break;
        sent += (size_t)n;
    }

    /*
     * Paso 6: leer respuesta completa con dos criterios de fin:
     *   - Content-Length conocido => leer headers_len + content_length bytes
     *   - Sin Content-Length      => leer hasta EOF (read() == 0) o buffer lleno
     */
    size_t total = 0;
    size_t expected = 0;       /* total esperado cuando se conozca CL */
    int    have_expected = 0;  /* 1 cuando ya parseamos Content-Length */

    while (total < resp_max) {
        ssize_t n = read(backend_fd, response + total, resp_max - total);
        if (n > 0) {
            total += (size_t)n;

            /* Intentar identificar el final de los headers para parsear CL */
            if (!have_expected) {
                /* memmem busca el separador headers/body sin requerir \0 */
                void *hdr_end = memmem(response, total, "\r\n\r\n", 4);
                if (hdr_end) {
                    size_t headers_len =
                        (size_t)((char *)hdr_end - response) + 4;
                    long cl = find_content_length(response, headers_len);
                    if (cl >= 0) {
                        expected = headers_len + (size_t)cl;
                        have_expected = 1;
                    } else {
                        /* Sin Content-Length: caemos al modo EOF */
                        have_expected = -1; /* "ya buscamos, no hay" */
                    }
                }
            }

            if (have_expected == 1 && total >= expected) {
                break;
            }
            continue;
        }
        if (n == 0) {
            /* Backend cerro la conexion: fin natural */
            break;
        }
        if (errno == EINTR) continue;
        fprintf(stderr, "[PIBL] read de backend %s:%d fallo: %s\n",
                backend_ip, backend_port, strerror(errno));
        close(backend_fd);
        *resp_len = total;
        return -1;
    }

    /* Paso 7: cerrar y devolver */
    close(backend_fd);
    *resp_len = total;
    return 0;
}
