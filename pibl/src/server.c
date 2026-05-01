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
 *      backend responda; si todos caen -> 502 Bad Gateway al cliente.          [RF-03/RF-10]
 *   4. Insertar el header "Via: PIBL/1.0" en la respuesta (proxy transparente
 *      excepto Via, segun PDF seccion 5.6).                                    [RF-04]
 *   5. write() loop al cliente en bloques de 4096 bytes hasta vaciar el
 *      buffer de respuesta (RF-20 bigfile ~1 MB).                             [RF-04]
 *   6. Log de transaccion con logger_log() (dual stdout + archivo).            [RF-07]
 *   7. close()/free().
 *
 * RF-08 (cache): marcadores TODO cache_lookup/cache_store antes y despues del
 *                backend hasta integrar cache.h en este flujo.
 * =============================================================================
 */

#define _GNU_SOURCE

#include "server.h"
#include "http_parser.h"
#include "proxy.h"
#include "balancer.h"
#include "logger.h"

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
 *   - WRITE_CHUNK: 4 KB exacto, criterio del RF-04.
 *   - BACKLOG: cola de conexiones pendientes para listen() (RF-02).
 */
#define CLIENT_BUF_SIZE    8192
#define BACKEND_BUF_SIZE   (2 * 1024 * 1024)
#define WRITE_CHUNK        4096
#define BACKLOG            128

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

int server_start(int port) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        logger_log("[PIBL] Error creando socket");
        return -1;
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR,
                   &opt, sizeof(opt)) < 0) {
        logger_log("[PIBL] Error configurando SO_REUSEADDR");
        close(server_fd);
        return -1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port        = htons((uint16_t)port);

    if (bind(server_fd, (struct sockaddr *)&server_addr,
             sizeof(server_addr)) < 0) {
        if (errno == EADDRINUSE) {
            logger_log("[PIBL] Error: puerto %d en uso", port);
        } else {
            logger_log("[PIBL] Error en bind");
        }
        close(server_fd);
        return -1;
    }

    if (listen(server_fd, BACKLOG) < 0) {
        logger_log("[PIBL] Error en listen");
        close(server_fd);
        return -1;
    }

    logger_log("[PIBL] Proxy escuchando en puerto %d", port);
    logger_log("[PIBL] Backends configurados: %d", balancer_count());

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t          client_len = sizeof(client_addr);

        int client_fd = accept(server_fd,
                               (struct sockaddr *)&client_addr,
                               &client_len);
        if (client_fd < 0) {
            if (errno == EINTR) {
                continue;
            }
            logger_log("[PIBL] Error en accept");
            continue;
        }

        pibl_ctx_t *ctx = malloc(sizeof(pibl_ctx_t));
        if (!ctx) {
            logger_log("[PIBL] Error reservando memoria para cliente");
            close(client_fd);
            continue;
        }

        ctx->client_fd = client_fd;
        inet_ntop(AF_INET, &client_addr.sin_addr,
                  ctx->client_ip, sizeof(ctx->client_ip));

        pthread_t thread;
        if (pthread_create(&thread, NULL, handle_client, ctx) != 0) {
            logger_log("[PIBL] Error creando thread para cliente %s",
                       ctx->client_ip);
            close(client_fd);
            free(ctx);
            continue;
        }

        pthread_detach(thread);
    }

    close(server_fd);
    return 0;
}

static void *handle_client(void *arg) {
    pibl_ctx_t *ctx = (pibl_ctx_t *)arg;

    /*
     * Buffers grandes en heap (no en la pila del thread): 2 MB romperia
     * un stack pequeno.
     */
    char *req_buf  = malloc(CLIENT_BUF_SIZE);
    char *resp_buf = malloc(BACKEND_BUF_SIZE);

    if (!req_buf || !resp_buf) {
        logger_log("[PIBL] %s -> 500 Internal Server Error -> MISS",
                   ctx->client_ip);
        send_simple_response(ctx->client_fd, "500 Internal Server Error",
                             "<html><body>500 Internal Server Error</body></html>");
        goto cleanup;
    }

    ssize_t r = read(ctx->client_fd, req_buf, CLIENT_BUF_SIZE - 1);
    if (r <= 0) {
        goto cleanup;
    }

    req_buf[r] = '\0';

    http_request_t req;
    if (http_parse_request(req_buf, &req) < 0) {
        send_simple_response(ctx->client_fd, "400 Bad Request",
                             "<html><body>400 Bad Request</body></html>");
        logger_log("[PIBL] %s -> 400 Bad Request -> MISS", ctx->client_ip);
        goto cleanup;
    }

    logger_log("[PIBL] %s -> %s %s",
               ctx->client_ip, req.method, req.uri);

    /* TODO RF-08: aqui ira cache_lookup() antes de tocar backends. */

    size_t resp_len = 0;
    int    rc       = -1;
    int    n        = balancer_count();
    if (n < 1) {
        n = 1;
    }

    backend_t *be_used = NULL;

    for (int i = 0; i < n && rc < 0; i++) {
        backend_t *be = balancer_next();
        if (!be) {
            break;
        }

        rc = proxy_forward(be->ip, be->port,
                           req_buf, (size_t)r,
                           resp_buf, &resp_len, BACKEND_BUF_SIZE);

        if (rc == 0) {
            be_used = be;
        } else {
            logger_log("[PIBL] backend %s:%d fallo, intentando siguiente",
                       be->ip, be->port);
        }
    }

    if (rc < 0 || resp_len == 0) {
        send_simple_response(ctx->client_fd, "502 Bad Gateway",
                             "<html><body>502 Bad Gateway</body></html>");
        logger_log("[PIBL] %s -> %s %s -> 502 Bad Gateway -> MISS",
                   ctx->client_ip, req.method, req.uri);
        goto cleanup;
    }

    resp_len = insert_via_header(resp_buf, resp_len, BACKEND_BUF_SIZE);

    /* TODO RF-08: aqui ira cache_store() con la respuesta si corresponde. */

    size_t sent = 0;
    while (sent < resp_len) {
        size_t remaining = resp_len - sent;
        size_t chunk = remaining > WRITE_CHUNK ? WRITE_CHUNK : remaining;

        ssize_t w = write(ctx->client_fd, resp_buf + sent, chunk);
        if (w < 0) {
            if (errno == EINTR) {
                continue;
            }
            logger_log("[PIBL] Error enviando respuesta al cliente %s",
                       ctx->client_ip);
            break;
        }
        if (w == 0) {
            break;
        }
        sent += (size_t)w;
    }

    if (be_used) {
        /* MISS hasta implementar RF-08; no se inventa codigo HTTP del backend */
        logger_log("[PIBL] %s -> %s %s -> %s:%d -> MISS -> %zu bytes "
                   "reenviados",
                   ctx->client_ip,
                   req.method,
                   req.uri,
                   be_used->ip,
                   be_used->port,
                   sent);
    }

cleanup:
    free(req_buf);
    free(resp_buf);
    close(ctx->client_fd);
    free(ctx);
    return NULL;
}

static void send_simple_response(int client_fd, const char *status_line,
                                 const char *body) {
    char   header[256];
    size_t body_len = strlen(body);

    int hdr_len = snprintf(header, sizeof(header),
                           "HTTP/1.1 %s\r\n"
                           "Content-Type: text/html\r\n"
                           "Content-Length: %zu\r\n"
                           "Via: PIBL/1.0\r\n"
                           "Connection: close\r\n"
                           "\r\n",
                           status_line, body_len);

    if (hdr_len <= 0) {
        return;
    }

    write_all(client_fd, header, (size_t)hdr_len);
    write_all(client_fd, body, body_len);
}

static int write_all(int fd, const char *buf, size_t len) {
    size_t sent = 0;

    while (sent < len) {
        ssize_t w = write(fd, buf + sent, len - sent);

        if (w < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }

        if (w == 0) {
            return -1;
        }

        sent += (size_t)w;
    }

    return 0;
}

static size_t insert_via_header(char *buf, size_t len, size_t max) {
    if (!buf || len == 0) {
        return len;
    }

    void *first_crlf = memmem(buf, len, "\r\n", 2);
    if (!first_crlf) {
        return len;
    }

    size_t insert_at = (size_t)((char *)first_crlf - buf) + 2;

    if (len + VIA_HEADER_LEN > max) {
        logger_log("[PIBL] insert_via: sin espacio para agregar header Via");
        return len;
    }

    memmove(buf + insert_at + VIA_HEADER_LEN,
            buf + insert_at,
            len - insert_at);
    memcpy(buf + insert_at, VIA_HEADER, VIA_HEADER_LEN);

    return len + VIA_HEADER_LEN;
}
