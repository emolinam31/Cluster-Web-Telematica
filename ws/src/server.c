#include "file_handler.h"
#include "http_parser.h"
#include "http_response.h"
#include "logger.h"
#include "server.h"

#include <arpa/inet.h>
#include <errno.h>
#include <limits.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define REQUEST_MAX 65536
#define BACKLOG 128

typedef struct {
    int client_fd;
    char client_ip[INET_ADDRSTRLEN];
    const char *doc_root;
} client_context_t;

static void *handle_client(void *arg);
static ssize_t read_http_request(int fd, char *buffer, size_t max);
static int headers_complete(const char *buffer);

int server_start(int port, const char *doc_root) {
    int server_fd;
    int opt = 1;
    struct sockaddr_in addr;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        logger_log("[TWS] Error creando socket: %s", strerror(errno));
        return -1;
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR,
                   &opt, sizeof(opt)) < 0) {
        logger_log("[TWS] Error en setsockopt: %s", strerror(errno));
        close(server_fd);
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((uint16_t)port);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        logger_log("[TWS] Error en bind puerto %d: %s", port, strerror(errno));
        close(server_fd);
        return -1;
    }

    if (listen(server_fd, BACKLOG) < 0) {
        logger_log("[TWS] Error en listen: %s", strerror(errno));
        close(server_fd);
        return -1;
    }

    logger_log("[TWS] Escuchando en puerto %d", port);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr,
                               &client_len);
        client_context_t *ctx;
        pthread_t thread;

        if (client_fd < 0) {
            if (errno == EINTR) {
                continue;
            }
            logger_log("[TWS] Error en accept: %s", strerror(errno));
            continue;
        }

        ctx = malloc(sizeof(*ctx));
        if (!ctx) {
            close(client_fd);
            continue;
        }

        ctx->client_fd = client_fd;
        ctx->doc_root = doc_root;
        inet_ntop(AF_INET, &client_addr.sin_addr,
                  ctx->client_ip, sizeof(ctx->client_ip));

        if (pthread_create(&thread, NULL, handle_client, ctx) != 0) {
            logger_log("[TWS] Error creando thread para %s", ctx->client_ip);
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
    client_context_t *ctx = (client_context_t *)arg;
    char *buffer = malloc(REQUEST_MAX + 1);
    http_request_t req;
    char file_path[PATH_MAX];
    const char *content_type;
    size_t file_size;
    ssize_t n;

    if (!buffer) {
        close(ctx->client_fd);
        free(ctx);
        return NULL;
    }

    n = read_http_request(ctx->client_fd, buffer, REQUEST_MAX);
    if (n <= 0 || http_parse_request(buffer, &req) < 0) {
        logger_log("[TWS] %s <- 400 Bad Request", ctx->client_ip);
        http_response_400(ctx->client_fd);
        goto cleanup;
    }

    logger_log("[TWS] %s -> %s %s %s",
               ctx->client_ip, req.method, req.uri, req.version);

    if (strcmp(req.method, "POST") == 0 && strcmp(req.uri, "/submit") == 0) {
        static const char body[] =
            "<html><body><h1>POST recibido</h1>"
            "<p>El servidor proceso el cuerpo de la peticion.</p></body></html>";
        char header[256];
        int header_len = snprintf(header, sizeof(header),
                                  "HTTP/1.1 200 OK\r\n"
                                  "Content-Type: text/html\r\n"
                                  "Content-Length: %zu\r\n"
                                  "Connection: close\r\n\r\n",
                                  strlen(body));
        write(ctx->client_fd, header, (size_t)header_len);
        if (strcmp(req.method, "HEAD") != 0) {
            write(ctx->client_fd, body, strlen(body));
        }
        logger_log("[TWS] %s <- 200 OK %s (%d body bytes recibidos)",
                   ctx->client_ip, req.uri, req.content_length);
        goto cleanup;
    }

    if (file_build_path(ctx->doc_root, req.uri, file_path,
                        sizeof(file_path)) < 0) {
        logger_log("[TWS] %s <- 400 Bad Request %s",
                   ctx->client_ip, req.uri);
        http_response_400(ctx->client_fd);
        goto cleanup;
    }

    if (!file_exists(ctx->doc_root, req.uri)) {
        logger_log("[TWS] %s <- 404 Not Found %s", ctx->client_ip, req.uri);
        if (strcmp(req.method, "HEAD") == 0) {
            http_response_404_head(ctx->client_fd);
        } else {
            http_response_404(ctx->client_fd);
        }
        goto cleanup;
    }

    file_size = file_get_size(file_path);
    content_type = file_get_content_type(file_path);
    http_response_200(ctx->client_fd, file_path, req.method,
                      content_type, file_size);
    logger_log("[TWS] %s <- 200 OK %s (%zu bytes)",
               ctx->client_ip, req.uri, file_size);

cleanup:
    free(buffer);
    close(ctx->client_fd);
    free(ctx);
    return NULL;
}

static ssize_t read_http_request(int fd, char *buffer, size_t max) {
    size_t total = 0;
    int content_length = -1;

    while (total < max) {
        ssize_t n = read(fd, buffer + total, max - total);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (n == 0) {
            break;
        }

        total += (size_t)n;
        buffer[total] = '\0';

        if (headers_complete(buffer)) {
            http_request_t req;
            char *body;
            if (http_parse_request(buffer, &req) == 0) {
                content_length = req.content_length;
                body = strstr(buffer, "\r\n\r\n");
                if (body) {
                    size_t body_bytes = total - (size_t)((body + 4) - buffer);
                    if (content_length <= 0 ||
                        body_bytes >= (size_t)content_length) {
                        return (ssize_t)total;
                    }
                }
            } else {
                return (ssize_t)total;
            }
        }
    }

    buffer[total] = '\0';
    return (ssize_t)total;
}

static int headers_complete(const char *buffer) {
    return strstr(buffer, "\r\n\r\n") != NULL;
}
