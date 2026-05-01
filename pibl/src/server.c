/*
 * =============================================================================
 * PIBL - server.c - Socket listener + handle_client (RF-02, RF-04, RF-05)
 * =============================================================================
 *
 * RESPONSABILIDAD PRINCIPAL DE ESTE ARCHIVO PARA EL RF-04:
 *   "PIBL espera respuesta del backend y reenvia al cliente"
 *
 * Flujo de handle_client():
 *   1. read() la peticion del cliente.                                        [RF-02]
 *   2. http_parse_request(): si falla -> 400 Bad Request al cliente.          [RF-06]
 *   3. Failover loop: balancer_next() -> proxy_forward() hasta que un
 *      backend responda; si todos caen -> 502 Bad Gateway al cliente.         [RF-03/RF-10]
 *   4. Insertar el header "Via: PIBL/1.0" en la respuesta (proxy transparente
 *      excepto Via, segun PDF seccion 5.6).                                   [RF-04]
 *   5. write() loop al cliente en bloques de 4096 bytes hasta vaciar el
 *      buffer de respuesta. Esto cumple el criterio del RF-04:
 *        "Para archivos grandes (~1MB), la respuesta se reenvia en bloques
 *         de 4096 bytes sin cortar"                                           [RF-04]
 *   6. Log de la transaccion (stdout; cuando se conecte logger_log() del
 *      RF-07, basta con cambiar las llamadas printf por logger_log).
 *   7. close()/free().
 *
 * NOTA: el cache (RF-08) y el logger dual (RF-07) son requisitos posteriores.
 * Este archivo deja "ganchos" claros para conectarlos sin reescribir nada.
 * =============================================================================
 */

#define _GNU_SOURCE

#include "server.h"
#include "http_parser.h"
#include "proxy.h"
#include "balancer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/*
 * Tamanos de buffer:
 *   - CLIENT_BUF_SIZE: tipico para un request HTTP (8 KB es estandar)
 *   - BACKEND_BUF_SIZE: 2 MB para soportar el caso de archivo de ~1 MB
 *     del RF-20 (bigfile.html) con margen para headers.
 *   - WRITE_CHUNK: 4 KB exacto, exigido por el criterio del RF-04.
 *   - BACKLOG: cola de conexiones pendientes para listen() (RF-02).
 */
#define CLIENT_BUF_SIZE    8192
#define BACKEND_BUF_SIZE   (2 * 1024 * 1024)
#define WRITE_CHUNK        4096
#define BACKLOG            128

/* Header agregado por el proxy (RFC 7230 5.7.1 / PDF seccion 5.6) */
#define VIA_HEADER         "Via: PIBL/1.0\r\n"
#define VIA_HEADER_LEN     (sizeof(VIA_HEADER) - 1)

typedef struct {
    int  client_fd;
    char client_ip[INET_ADDRSTRLEN];
} pibl_ctx_t;

static void *handle_client(void *arg);
static void  send_simple_response(int client_fd, const char *status_line,
                                  const char *body);
static int   write_all(int fd, const char *buf, size_t len);
static size_t insert_via_header(char *buf, size_t len, size_t max);

/* ------------------------------------------------------------------ */
/*  server_start: socket + bind + listen + accept loop con threads    */
/*  Cumple RF-02 (puerto configurable) y RF-05 (un thread por cliente) */
/* ------------------------------------------------------------------ */
int server_start(int port) {
    /* Crear socket TCP */
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("[PIBL] Error creando socket");
        return -1;
    }

    /* Permitir reutilizar el puerto (evita "Address already in use" rapido) */
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR,
                   &opt, sizeof(opt)) < 0) {
        perror("[PIBL] Error configurando SO_REUSEADDR");
        close(server_fd);
        return -1;
    }

    /* Configurar IP y puerto del servidor (INADDR_ANY = todas las interfaces) */
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port        = htons((uint16_t)port);

    /* Asociar socket al puerto (con mensaje claro si esta en uso, RF-02) */
    if (bind(server_fd, (struct sockaddr *)&server_addr,
             sizeof(server_addr)) < 0) {
        if (errno == EADDRINUSE) {
            fprintf(stderr, "[PIBL] Error: puerto %d en uso\n", port);
        } else {
            perror("[PIBL] Error en bind");
        }
        close(server_fd);
        return -1;
    }

    /* Escuchar conexiones */
    if (listen(server_fd, BACKLOG) < 0) {
        perror("[PIBL] Error en el listen");
        close(server_fd);
        return -1;
    }

    printf("[PIBL] Proxy escuchando en puerto %d\n", port);
    printf("[PIBL] Backends configurados: %d\n", balancer_count());

    /* Loop de accept(): un thread por conexion (RF-05) */
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t          client_len = sizeof(client_addr);

        int client_fd = accept(server_fd,
                               (struct sockaddr *)&client_addr,
                               &client_len);
        if (client_fd < 0) {
            if (errno == EINTR) continue;
            perror("[PIBL] Error en accept");
            continue;
        }

        pibl_ctx_t *ctx = malloc(sizeof(pibl_ctx_t));
        if (!ctx) {
            perror("[PIBL] malloc(ctx)");
            close(client_fd);
            continue;
        }
        ctx->client_fd = client_fd;
        inet_ntop(AF_INET, &client_addr.sin_addr,
                  ctx->client_ip, sizeof(ctx->client_ip));

        pthread_t thread;
        if (pthread_create(&thread, NULL, handle_client, ctx) != 0) {
            perror("[PIBL] pthread_create");
            close(client_fd);
            free(ctx);
            continue;
        }
        pthread_detach(thread);
    }

    close(server_fd);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  handle_client: aqui es donde "vive" el RF-04                       */
/* ------------------------------------------------------------------ */
static void *handle_client(void *arg) {
    pibl_ctx_t *ctx = (pibl_ctx_t *)arg;

    /*
     * Los buffers grandes NO se ponen en el stack: 2 MB explotaria la pila
     * por defecto del thread (8 MB en Linux pero con margen mucho menor).
     */
    char *req_buf  = malloc(CLIENT_BUF_SIZE);
    char *resp_buf = malloc(BACKEND_BUF_SIZE);
    if (!req_buf || !resp_buf) {
        perror("[PIBL] malloc(buffers)");
        send_simple_response(ctx->client_fd, "500 Internal Server Error",
                             "<html><body>500</body></html>");
        goto cleanup;
    }

    /* --- Paso 1: leer la peticion del cliente -------------------- */
    ssize_t r = read(ctx->client_fd, req_buf, CLIENT_BUF_SIZE - 1);
    if (r <= 0) {
        goto cleanup;
    }
    req_buf[r] = '\0';

    /* --- Paso 2: parsear (RF-06) y validar metodo/version --------- */
    http_request_t req;
    if (http_parse_request(req_buf, &req) < 0) {
        send_simple_response(ctx->client_fd, "400 Bad Request",
                             "<html><body>400 Bad Request</body></html>");
        printf("[PIBL] %s -> 400 Bad Request (parse error)\n", ctx->client_ip);
        goto cleanup;
    }

    /* TODO RF-08: aqui ira cache_lookup() antes de tocar el backend. */

    /* --- Paso 3: failover Round Robin -> proxy_forward() ---------- */
    size_t resp_len = 0;
    int    rc       = -1;
    int    n        = balancer_count();
    if (n < 1) n = 1;

    backend_t *be_used = NULL;
    for (int i = 0; i < n && rc < 0; i++) {
        backend_t *be = balancer_next();
        if (!be) break;
        rc = proxy_forward(be->ip, be->port,
                           req_buf, (size_t)r,
                           resp_buf, &resp_len, BACKEND_BUF_SIZE);
        if (rc == 0) {
            be_used = be;
        } else {
            fprintf(stderr,
                    "[PIBL] backend %s:%d fallo, intentando siguiente...\n",
                    be->ip, be->port);
        }
    }

    if (rc < 0 || resp_len == 0) {
        send_simple_response(ctx->client_fd, "502 Bad Gateway",
                             "<html><body>502 Bad Gateway</body></html>");
        printf("[PIBL] %s -> 502 Bad Gateway (todos los backends fallaron)\n",
               ctx->client_ip);
        goto cleanup;
    }

    /* --- Paso 4: insertar Via header (RF-04, proxy transparente) --- */
    resp_len = insert_via_header(resp_buf, resp_len, BACKEND_BUF_SIZE);

    /* TODO RF-08: aqui ira cache_store() con la respuesta original. */

    /* --- Paso 5: write loop al cliente en bloques de 4096 (RF-04) -- */
    size_t sent = 0;
    while (sent < resp_len) {
        size_t remaining = resp_len - sent;
        size_t chunk = remaining > WRITE_CHUNK ? WRITE_CHUNK : remaining;
        ssize_t w = write(ctx->client_fd, resp_buf + sent, chunk);
        if (w < 0) {
            if (errno == EINTR) continue;
            perror("[PIBL] write a cliente");
            break;
        }
        if (w == 0) break;
        sent += (size_t)w;
    }

    /* --- Paso 6: log de la transaccion (stdout) ------------------- */
    if (be_used) {
        printf("[PIBL] %s -> %s %s -> %s:%d -> %zu bytes reenviados\n",
               ctx->client_ip, req.method, req.uri,
               be_used->ip, be_used->port, sent);
    }

cleanup:
    free(req_buf);
    free(resp_buf);
    close(ctx->client_fd);
    free(ctx);
    return NULL;
}

/* ------------------------------------------------------------------ */
/*  Helpers                                                            */
/* ------------------------------------------------------------------ */

/*
 * Envia una respuesta minima HTTP/1.1 con un cuerpo HTML.
 * Se usa para 400 Bad Request y 502 Bad Gateway cuando el flujo proxy falla.
 */
static void send_simple_response(int client_fd, const char *status_line,
                                 const char *body) {
    char header[256];
    size_t body_len = strlen(body);
    int hdr_len = snprintf(header, sizeof(header),
        "HTTP/1.1 %s\r\n"
        "Content-Type: text/html\r\n"
        "Content-Length: %zu\r\n"
        "Via: PIBL/1.0\r\n"
        "Connection: close\r\n"
        "\r\n",
        status_line, body_len);
    if (hdr_len <= 0) return;
    write_all(client_fd, header, (size_t)hdr_len);
    write_all(client_fd, body,   body_len);
}

/*
 * write() puede ser parcial: este helper repite hasta enviar todo o fallar.
 * Tambien tolera EINTR.
 */
static int write_all(int fd, const char *buf, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t w = write(fd, buf + sent, len - sent);
        if (w < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (w == 0) return -1;
        sent += (size_t)w;
    }
    return 0;
}

/*
 * Inserta "Via: PIBL/1.0\r\n" inmediatamente despues de la Status-Line
 * (que termina en el primer \r\n del buffer de respuesta).
 *
 * El body NO se toca: el Content-Length original sigue siendo valido porque
 * Via es un header, no parte del cuerpo.
 *
 * Devuelve la nueva longitud del buffer; si no cabe el header o no se
 * encuentra el separador, devuelve la longitud original sin modificar nada.
 */
static size_t insert_via_header(char *buf, size_t len, size_t max) {
    if (!buf || len == 0) return len;

    /* Buscar el primer \r\n: marca el fin de la Status-Line */
    void *first_crlf = memmem(buf, len, "\r\n", 2);
    if (!first_crlf) return len;

    size_t insert_at = (size_t)((char *)first_crlf - buf) + 2;
    if (len + VIA_HEADER_LEN > max) {
        fprintf(stderr,
                "[PIBL] insert_via: sin espacio (%zu+%zu>%zu)\n",
                len, VIA_HEADER_LEN, max);
        return len;
    }

    /* Desplazar el resto del buffer y copiar el header */
    memmove(buf + insert_at + VIA_HEADER_LEN,
            buf + insert_at,
            len - insert_at);
    memcpy(buf + insert_at, VIA_HEADER, VIA_HEADER_LEN);
    return len + VIA_HEADER_LEN;
}
