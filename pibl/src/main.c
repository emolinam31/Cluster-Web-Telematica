/*
 * =============================================================================
 * PIBL (Proxy Inverso + Balanceador de Carga) - Punto de entrada
 * =============================================================================
 *
 * REQUISITOS QUE DEBE CUMPLIR ESTE ARCHIVO:
 *   1. Parsear argumentos CLI:
 *      ./pibl <HTTP_PORT> <ConfigFile> <LogFile> <CacheTTL>
 *      - HTTP_PORT: puerto donde escucha el proxy (ej: 80)
 *      - ConfigFile: ruta al archivo de configuración (ej: pibl.conf)
 *      - LogFile: ruta al archivo de log (ej: logs/pibl.log)
 *      - CacheTTL: tiempo de vida del caché en segundos (ej: 300)
 *   2. Validar que los 4 argumentos estén presentes
 *   3. Inicializar subsistemas en orden:
 *      a. logger_init(log_file)
 *      b. config_load(config_file) → cargar lista de backends
 *      c. balancer_init(backends, num_backends)
 *      d. cache_init(cache_ttl) (si TTL > 0, caché habilitado)
 *      e. server_start(port)
 *
 * PASOS A TOMAR:
 *   - Paso 1: Validar argc == 5
 *   - Paso 2: Extraer port, config_file, log_file, cache_ttl de argv
 *   - Paso 3: Llamar a cada _init() en orden
 *   - Paso 4: Llamar a server_start() (no retorna, loop infinito)
 *
 * CLAVES PARA EL ÉXITO:
 *   - Si CacheTTL == 0 → caché deshabilitado (bypass)
 *   - Si config_load falla (archivo no existe) → abortar con error
 *   - Mínimo 3 backends definidos en el archivo de configuración
 *   - Este archivo NO contiene lógica de red ni HTTP, solo parseo y arranque
 * =============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "server.h"
#include "config.h"
#include "balancer.h"
#include "cache.h"
#include "logger.h"
