/*
 * =============================================================================
 * TWS - http_response.c - Construcción y envío de respuestas HTTP
 * =============================================================================
 *
 * REQUISITOS QUE DEBE CUMPLIR ESTE ARCHIVO:
 *   1. http_response_200():
 *      - Construir headers: "HTTP/1.1 200 OK\r\n"
 *      - Header Content-Type según extensión del archivo
 *      - Header Content-Length con el tamaño del archivo
 *      - Enviar headers con write()
 *      - Si method es "GET" o "POST" → enviar body (contenido del archivo)
 *      - Si method es "HEAD" → NO enviar body, solo headers
 *   2. http_response_400():
 *      - Enviar "HTTP/1.1 400 Bad Request\r\n"
 *      - Incluir un body HTML simple explicando el error
 *   3. http_response_404():
 *      - Enviar "HTTP/1.1 404 Not Found\r\n"
 *      - Incluir un body HTML simple indicando que el recurso no existe
 *
 * PASOS A TOMAR:
 *   - Paso 1: Implementar función auxiliar send_all(fd, buf, len)
 *     para garantizar que se envíe todo el buffer
 *   - Paso 2: Implementar http_response_200 leyendo el archivo con
 *     file_handler y enviando en bloques
 *   - Paso 3: Implementar las funciones de error (400, 404) con HTML simple
 *
 * CLAVES PARA EL ÉXITO:
 *   - write() puede no enviar todos los bytes → loop con send_all()
 *   - Para archivos grandes (~1MB), leer y enviar en bloques de 4096 bytes
 *   - Los headers DEBEN terminar con "\r\n" cada línea
 *   - Después de todos los headers, enviar "\r\n" vacío (separador)
 *   - HEAD debe retornar los MISMOS headers que GET (incluyendo
 *     Content-Length) pero sin body
 *   - Usar file_handler para abrir/leer archivos, NO hacerlo aquí
 * =============================================================================
 */

#include "http_response.h"
#include "file_handler.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
