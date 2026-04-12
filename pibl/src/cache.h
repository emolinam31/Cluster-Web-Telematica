/*
 * =============================================================================
 * PIBL - cache.h - Interfaz del sistema de caché en disco
 * =============================================================================
 *
 * REQUISITOS:
 *   1. Declarar cache_init(ttl_seconds) → configurar TTL global
 *   2. Declarar cache_lookup(uri, response, resp_len) → buscar en disco
 *      Retorna 1 si encontrado y válido, 0 si no
 *   3. Declarar cache_store(uri, response, resp_len) → guardar en disco
 *   4. Declarar cache_is_valid(cache_file) → verificar si TTL no ha expirado
 *
 * NOTAS:
 *   - Caché basado en DISCO (carpeta cache/)
 *   - TTL se configura desde CLI al iniciar el PIBL
 *   - Si TTL == 0 → caché deshabilitado (bypass completo)
 * =============================================================================
 */

#ifndef PIBL_CACHE_H
#define PIBL_CACHE_H

#include <stddef.h>

int  cache_init(int ttl_seconds);
int  cache_lookup(const char *uri, char *response, size_t *resp_len,
                  size_t resp_max);
int  cache_store(const char *uri, const char *response, size_t resp_len);
int  cache_is_valid(const char *cache_path);

#endif
