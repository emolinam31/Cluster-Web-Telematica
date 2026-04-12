/*
 * =============================================================================
 * PIBL - http_parser.h - Interfaz del parser HTTP para el proxy
 * =============================================================================
 *
 * REQUISITOS:
 *   1. Definir http_request_t: method, uri, version, host, content_length, body
 *   2. Declarar http_parse_request(buffer, request) → 0 OK, -1 error
 *   3. Declarar http_find_header(buffer, header_name) → valor del header
 *   4. Declarar http_get_content_length(buffer) → tamaño del body
 *
 * NOTAS:
 *   - El parser del PIBL es MUY similar al del TWS
 *   - La diferencia es que el PIBL también necesita parsear RESPUESTAS del
 *     backend (para extraer Content-Length y saber cuándo terminó la respuesta)
 *   - Considerar crear también http_parse_response() si se necesita
 * =============================================================================
 */

#ifndef PIBL_HTTP_PARSER_H
#define PIBL_HTTP_PARSER_H

typedef struct {
    char method[8];
    char uri[2048];
    char version[16];
    char host[256];
    int  content_length;
    char *body;
} http_request_t;

int  http_parse_request(const char *buffer, http_request_t *req);
const char *http_find_header(const char *buffer, const char *header_name);
int  http_get_content_length(const char *buffer);

#endif
