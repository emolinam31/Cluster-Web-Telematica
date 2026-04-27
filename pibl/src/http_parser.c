/*
 * =============================================================================
 * PIBL - http_parser.c - Parser HTTP para requests y responses
 * =============================================================================
 *
 * REQUISITOS QUE DEBE CUMPLIR ESTE ARCHIVO:
 *   1. http_parse_request(): Idéntico al TWS - parsear Request-Line + headers
 *   2. http_find_header(): Buscar un header específico en el buffer
 *      - Ej: http_find_header(buf, "Content-Length") → "1234"
 *      - Case-insensitive
 *   3. http_get_content_length(): Wrapper sobre http_find_header
 *      que retorna el valor como int (atoi)
 *
 * PASOS A TOMAR:
 *   - Paso 1: Copiar la implementación base del TWS http_parser.c
 *   - Paso 2: Agregar http_find_header():
 *       a. Buscar "header_name:" en el buffer (case-insensitive)
 *       b. Saltar espacios después de ":"
 *       c. Retornar puntero al inicio del valor
 *   - Paso 3: http_get_content_length():
 *       a. Llamar http_find_header(buf, "Content-Length")
 *       b. Si encontrado → atoi(valor)
 *       c. Si no → retornar -1
 *
 * CLAVES PARA EL ÉXITO:
 *   - El PIBL necesita parsear tanto requests como responses HTTP
 *   - Para responses, la Status-Line es: "HTTP/1.1 200 OK\r\n"
 *     → NO usar http_parse_request para responses
 *   - Content-Length es CRUCIAL para saber cuándo la respuesta del
 *     backend está completa → sin esto, read() se bloquea esperando más datos
 *   - Headers pueden tener espacios variables: "Content-Length: 1234"
 *     o "Content-Length:1234" → manejar ambos casos
 * =============================================================================
 */

#include "http_parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
