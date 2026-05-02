#include "file_handler.h"
#include "http_parser.h"
#include "http_response.h"
#include "logger.h"
#include "server.h"

#include <arpa/inet.h>
#include <errno.h>
#include <limits.h>
#include <netdb.h>
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
static int create_listener_socket(int port);
static void sockaddr_to_ip(const struct sockaddr *addr, char *out,
                           size_t out_size);

int server_start(int port, const char *doc_root) {
    int server_fd;

    server_fd = create_listener_socket(port);
    if (server_fd < 0) {
        return -1;
    }

    logger_log("[TWS] Escuchando en puerto %d", port);

    while (1) {
        struct sockaddr_storage client_addr;
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
        sockaddr_to_ip((struct sockaddr *)&client_addr,
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

static int create_listener_socket(int port) {
    struct addrinfo hints;
    struct addrinfo *servinfo = NULL;
    struct addrinfo *p;
    char port_str[16];
    int status;
    int listener = -1;
    int yes = 1;

    snprintf(port_str, sizeof(port_str), "%d", port);
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    status = getaddrinfo(NULL, port_str, &hints, &servinfo);
    if (status != 0) {
        logger_log("[TWS] getaddrinfo: %s", gai_strerror(status));
        return -1;
    }

    for (p = servinfo; p != NULL; p = p->ai_next) {
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listener < 0) {
            continue;
        }

        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

        if (bind(listener, p->ai_addr, p->ai_addrlen) == 0) {
            break;
        }

        close(listener);
        listener = -1;
    }

    freeaddrinfo(servinfo);

    if (listener < 0) {
        logger_log("[TWS] No se pudo crear/bindear socket en puerto %d", port);
        return -1;
    }

    if (listen(listener, BACKLOG) < 0) {
        logger_log("[TWS] Error en listen: %s", strerror(errno));
        close(listener);
        return -1;
    }

    return listener;
}

static void sockaddr_to_ip(const struct sockaddr *addr, char *out,
                           size_t out_size) {
    void *src = NULL;

    if (!addr || !out || out_size == 0) {
        return;
    }

    if (addr->sa_family == AF_INET) {
        src = &((struct sockaddr_in *)addr)->sin_addr;
    } else if (addr->sa_family == AF_INET6) {
        src = &((struct sockaddr_in6 *)addr)->sin6_addr;
    }

    if (!src || !inet_ntop(addr->sa_family, src, out, out_size)) {
        snprintf(out, out_size, "unknown");
    }
}
