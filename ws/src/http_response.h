/*
 * =============================================================================
 * TWS - http_response.h - Interfaz para construir respuestas HTTP
 * =============================================================================
 *
 * REQUISITOS:
 *   1. Declarar funciones para enviar respuestas al cliente:
 *      - http_response_200(fd, file_path, method, content_type, file_size)
 *      - http_response_400(fd)
 *      - http_response_404(fd)
 *   2. Cada respuesta incluye headers HTTP completos + body (si aplica)
 *
 * NOTAS:
 *   - Las respuestas deben seguir el formato HTTP/1.1:
 *     HTTP/1.1 200 OK\r\n
 *     Content-Type: text/html\r\n
 *     Content-Length: 1234\r\n
 *     \r\n
 *     [body]
 * =============================================================================
 */

#ifndef TWS_HTTP_RESPONSE_H
#define TWS_HTTP_RESPONSE_H

#include <stddef.h>

void http_response_200(int client_fd, const char *file_path,
                       const char *method, const char *content_type,
                       size_t file_size);
void http_response_400(int client_fd);
void http_response_404(int client_fd);

#endif
