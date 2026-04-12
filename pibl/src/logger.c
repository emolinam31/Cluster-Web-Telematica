/*
 * =============================================================================
 * PIBL - logger.c - Sistema de logging dual (stdout + archivo)
 * =============================================================================
 *
 * REQUISITOS QUE DEBE CUMPLIR ESTE ARCHIVO:
 *   1. Idéntico al logger.c del TWS:
 *      - logger_init() → fopen append + mutex init
 *      - logger_log() → timestamp + print a stdout + archivo, con mutex
 *      - logger_close() → fclose + mutex destroy
 *   2. Thread-safe con pthread_mutex_t
 *   3. Log dual (stdout + archivo) con flush después de cada escritura
 *
 * PASOS A TOMAR:
 *   - Paso 1: Copiar la implementación del TWS logger.c
 *   - Paso 2: Es exactamente la misma lógica
 *   - (NOTA: en un futuro se podría extraer a una librería compartida)
 *
 * CLAVES PARA EL ÉXITO:
 *   - El log del PIBL debe registrar:
 *     [TIMESTAMP] CLIENT_IP → METHOD URI → BACKEND_IP:PORT → STATUS → cache HIT/MISS
 *   - Ejemplo de línea de log:
 *     [2026-03-31 14:30:45] 192.168.1.5 → GET /index.html → 10.0.1.10:8080 → 200 OK → MISS
 *   - fflush() después de cada escritura es obligatorio
 * =============================================================================
 */

#include "logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <pthread.h>
