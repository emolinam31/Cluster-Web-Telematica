/*
 * =============================================================================
 * TWS - http_parser.c - Parser de peticiones HTTP/1.1
 * =============================================================================
 *
 * REQUISITOS QUE DEBE CUMPLIR ESTE ARCHIVO:
 *   1. Parsear la línea de request (Request-Line): "METHOD URI VERSION\r\n"
 *      - Extraer method, uri, version con sscanf o strtok
 *   2. Parsear los headers línea por línea buscando:
 *      - Host: (obligatorio en HTTP/1.1)
 *      - Content-Length: (necesario para POST)
 *      - Content-Type: (informativo, guardar si se necesita)
 *      - Connection: (keep-alive / close)
 *   3. Validar que method sea GET, HEAD o POST → si no, retornar -1
 *   4. Validar que version sea HTTP/1.0 o HTTP/1.1 → si no, retornar -1
 *   5. El body del POST empieza después de "\r\n\r\n"
 *
 * PASOS A TOMAR:
 *   - Paso 1: Copiar buffer a una copia temporal (no modificar original)
 *   - Paso 2: Tokenizar primera línea → method, uri, version
 *   - Paso 3: Loop sobre líneas restantes → buscar headers relevantes
 *   - Paso 4: Si es POST, guardar puntero al body (offset desde "\r\n\r\n")
 *   - Paso 5: Retornar 0 si todo OK, -1 si request malformado
 *
 * CLAVES PARA EL ÉXITO:
 *   - Las líneas HTTP terminan en "\r\n" (CRLF), NO solo "\n"
 *   - Los headers son case-insensitive: "Host:" == "host:" == "HOST:"
 *     → usar strcasecmp o convertir a minúsculas
 *   - No confiar en que el request tenga formato perfecto,
 *     si algo falta retornar -1 (→ genera 400 Bad Request)
 *   - El URI puede contener query strings: /page?id=5
 *     → solo usar la parte antes de '?' para buscar archivos
 *   - Sanitizar URI: evitar path traversal ("..") por seguridad
 * =============================================================================
 */

#include "http_parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
