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


static FILE *log_file = NULL;
static pthread_mutex_t log_mutex;

int logger_init(const char *path){
    log_file = fopen(path, "a");
    if (log_file == NULL){
        return -1;
    }

    if (pthread_mutex_init(&log_mutex, NULL) != 0){
        fclose(log_file);
        log_file = NULL;
        return -1;
    }

    return 0;
}

void logger_log(const char *format, ...){
    if (log_file == NULL){
        return;
    }

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);

    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);

    va_list ap;
    va_start(ap, format);

    pthread_mutex_lock(&log_mutex);

    fprintf(stdout, "[%s] ", timestamp);
    va_list ap_out;
    va_copy(ap_out, ap);
    vfprintf(stdout, format, ap_out);
    va_end(ap_out);

    fprintf(log_file, "[%s] ", timestamp);
    va_copy(ap_out, ap);
    vfprintf(log_file, format, ap_out);
    va_end(ap_out);

    fprintf(stdout, "\n");
    fprintf(log_file, "\n");

    fflush(stdout);
    fflush(log_file);

    pthread_mutex_unlock(&log_mutex);

    va_end(ap);
}

void logger_close(void){
    pthread_mutex_lock(&log_mutex);
    if (log_file != NULL){
        fflush(log_file);
        fclose(log_file);
        log_file = NULL;
    }
    pthread_mutex_unlock(&log_mutex);

    pthread_mutex_destroy(&log_mutex);
}


