/*
 * =============================================================================
 * WS - http_parser.h - Interfaz del parser HTTP
 * =============================================================================
 *
 * REQUISITOS:
 *   1. Definir la estructura http_request_t con campos:
 *      - method: "GET", "HEAD" o "POST"
 *      - uri: ruta del recurso (ej: "/index.html", "/gallery.html")
 *      - version: "HTTP/1.1"
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


#ifndef WS_HTTP_PARSER_H
#define WS_HTTP_PARSER_H

/*
Esta va a ser la estructura con los datos parseados de una peticion HTTP1.1.
*/
typedef struct {
    char method[8]; /* Metodo HTTP: "GET", "HEAD" o "POST" */
    char uri[2048]; /* Ruta del recurso solicitado, ej: "/index.html" */
    char version[16]; /* Version del protocolo: "HTTP/1.1" */
    char host[256]; /* Valor del header Host, obligatorio en HTTP/1.1 */
    int content_length; /* Valor del header Content-Length (0 si no viene) */
    char *body; /* Puntero al inicio del body dentro del buffer */
} http_request_t;

int http_parse_request(const char *buffer, http_request_t *req);

#endif
