/*
 * =============================================================================
 * TWS - http_parser.h - Interfaz del parser HTTP
 * =============================================================================
 *
 * REQUISITOS:
 *   1. Definir la estructura http_request_t con campos:
 *      - method: "GET", "HEAD" o "POST"
 *      - uri: ruta del recurso (ej: "/index.html", "/gallery.html")
 *      - version: "HTTP/1.1" o "HTTP/1.0"
 *      - host: valor del header Host (obligatorio en HTTP/1.1)
 *      - content_length: valor numérico del header Content-Length (para POST)
 *      - body: puntero al cuerpo del request (para POST)
 *   2. Declarar http_parse_request(buffer, request) → 0 éxito, -1 error
 *
 * NOTAS:
 *   - Un request HTTP tiene la forma:
 *     GET /index.html HTTP/1.1\r\n
 *     Host: 192.168.1.1:8080\r\n
 *     Content-Length: 0\r\n
 *     \r\n
 *     [body si POST]
 * =============================================================================
 */

#ifndef TWS_HTTP_PARSER_H
#define TWS_HTTP_PARSER_H

typedef struct {
    char method[8];
    char uri[2048];
    char version[16];
    char host[256];
    int  content_length;
    char *body;
} http_request_t;

int http_parse_request(const char *buffer, http_request_t *req);

#endif
