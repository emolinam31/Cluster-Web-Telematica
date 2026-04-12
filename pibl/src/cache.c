/*
 * =============================================================================
 * PIBL - cache.c - Sistema de caché en disco con TTL configurable
 * =============================================================================
 *
 * REQUISITOS QUE DEBE CUMPLIR ESTE ARCHIVO:
 *   1. cache_init(ttl): Guardar TTL global, si TTL == 0 → caché desactivado
 *   2. cache_lookup(uri, response, resp_len):
 *      a. Convertir URI a ruta de archivo: uri_to_cache_path()
 *         Ej: "/index.html" → "cache/index.html"
 *      b. Verificar si el archivo existe con stat()
 *      c. Verificar si es válido con cache_is_valid()
 *      d. Si válido → leer contenido del archivo al buffer response
 *      e. Retornar 1 si encontrado y válido, 0 si no
 *   3. cache_store(uri, response, resp_len):
 *      a. Convertir URI a ruta de archivo
 *      b. Crear directorios intermedios si es necesario (mkdir -p)
 *      c. Escribir response al archivo con fwrite()
 *   4. cache_is_valid(cache_path):
 *      a. Obtener tiempo de modificación del archivo (stat → st_mtime)
 *      b. Comparar con time(NULL) - ttl
 *      c. Si (now - mtime) > ttl → expirado, retornar 0
 *      d. Si (now - mtime) <= ttl → válido, retornar 1
 *
 * PASOS A TOMAR:
 *   - Paso 1: Variable estática global: int g_ttl
 *   - Paso 2: Implementar uri_to_cache_path() helper
 *   - Paso 3: cache_is_valid() con stat() + time()
 *   - Paso 4: cache_lookup() con fopen/fread
 *   - Paso 5: cache_store() con fopen/fwrite
 *
 * CLAVES PARA EL ÉXITO:
 *   - Si g_ttl == 0, cache_lookup SIEMPRE retorna 0 (bypass)
 *   - El URI "/gallery.html" se cachea como archivo "cache/gallery.html"
 *   - Para URIs con subdirectorios "/css/style.css" → crear "cache/css/"
 *   - Sanitizar el URI antes de usarlo como nombre de archivo
 *   - Thread-safety: múltiples threads leen/escriben el mismo archivo
 *     → usar mutex o file locking (flock)
 *   - Al calcular Age header: age = time(NULL) - st_mtime del archivo
 * =============================================================================
 */

#include "cache.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>
