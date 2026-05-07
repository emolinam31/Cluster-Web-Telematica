#include "http_response.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define FILE_CHUNK 4096

static int send_all(int fd, const void *buf, size_t len) {
    const char *p = (const char *)buf;
    size_t sent = 0;

    while (sent < len) {
        ssize_t n = write(fd, p + sent, len - sent);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (n == 0) {
            return -1;
        }
        sent += (size_t)n;
    }

    return 0;
}

static void send_error(int client_fd, const char *status,
                       const char *body, int include_body) {
    char header[512];
    size_t body_len = strlen(body);
    int header_len = snprintf(header, sizeof(header),
                              "HTTP/1.1 %s\r\n"
                              "Content-Type: text/html\r\n"
                              "Content-Length: %zu\r\n"
                              "Connection: close\r\n"
                              "\r\n",
                              status, body_len);

    if (header_len <= 0) {
        return;
    }

    send_all(client_fd, header, (size_t)header_len);
    if (include_body) {
        send_all(client_fd, body, body_len);
    }
}

void http_response_200(int client_fd, const char *file_path,
                       const char *method, const char *content_type,
                       size_t file_size) {
    char header[512];
    char buffer[FILE_CHUNK];
    FILE *file;
    int header_len;

    header_len = snprintf(header, sizeof(header),
                          "HTTP/1.1 200 OK\r\n"
                          "Content-Type: %s\r\n"
                          "Content-Length: %zu\r\n"
                          "Connection: close\r\n"
                          "\r\n",
                          content_type, file_size);
    if (header_len <= 0) {
        return;
    }

    send_all(client_fd, header, (size_t)header_len);

    if (strcmp(method, "HEAD") == 0) {
        return;
    }

    file = fopen(file_path, "rb");
    if (!file) {
        return;
    }

    while (!feof(file)) {
        size_t n = fread(buffer, 1, sizeof(buffer), file);
        if (n > 0 && send_all(client_fd, buffer, n) < 0) {
            break;
        }
        if (ferror(file)) {
            break;
        }
    }

    fclose(file);
}

void http_response_400(int client_fd) {
    static const char body[] =
        "<html><body><h1>400 Bad Request</h1>"
        "<p>La peticion no pudo ser procesada.</p></body></html>";
    send_error(client_fd, "400 Bad Request", body, 1);
}

void http_response_404(int client_fd) {
    static const char body[] =
        "<html><body><h1>404 - Page/File Not Found</h1>"
        "<p>El recurso solicitado no existe.</p></body></html>";
    send_error(client_fd, "404 Not Found", body, 1);
}

void http_response_404_head(int client_fd) {
    static const char body[] =
        "<html><body><h1>404 - Page/File Not Found</h1>"
        "<p>El recurso solicitado no existe.</p></body></html>";
    send_error(client_fd, "404 Not Found", body, 0);
}
