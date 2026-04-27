/*
 * =============================================================================
 * TWS - logger.c - Sistema de logging dual (stdout + archivo)
 * =============================================================================
 *
 * REQUISITOS QUE DEBE CUMPLIR ESTE ARCHIVO:
 *   1. logger_init(): Abrir el archivo de log en modo append ("a")
 *      - Si no se puede abrir → retornar -1
 *   2. logger_log(): Escribir mensaje con timestamp a:
 *      - stdout (printf/fprintf a stdout)
 *      - archivo de log (fprintf al FILE*)
 *      - Flush ambos después de cada escritura
 *   3. logger_close(): Cerrar el FILE* del log
 *   4. Ser THREAD-SAFE: proteger escrituras con pthread_mutex_t
 *
 * PASOS A TOMAR:
 *   - Paso 1: Variables globales estáticas: FILE *log_fp, pthread_mutex_t mutex
 *   - Paso 2: logger_init() → fopen + pthread_mutex_init
 *   - Paso 3: logger_log() con va_list (variadic):
 *       a. pthread_mutex_lock()
 *       b. Obtener timestamp con time() + localtime() + strftime()
 *       c. Imprimir "[TIMESTAMP] mensaje" a stdout y al archivo
 *       d. fflush(stdout) y fflush(log_fp)
 *       e. pthread_mutex_unlock()
 *   - Paso 4: logger_close() → fclose + pthread_mutex_destroy
 *
 * CLAVES PARA EL ÉXITO:
 *   - SIN MUTEX = corrupcción de datos cuando múltiples threads logean
 *   - Usar va_start / va_end para manejar argumentos variables
 *   - fflush() es crucial: sin él, los logs se pierden si el programa crashea
 *   - Formato del timestamp sugerido: "[2026-03-31 14:30:45]"
 *   - No olvidar el "\n" al final de cada línea de log
 * =============================================================================
 */

 /* Autor: Camila Martinez 
   Función: Registrar mensajes con timestamp en la terminal y un archivo de texto
*/

#include "logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <pthread.h>

static FILE *log_fp = NULL;

static pthread_mutex_t log_mutex;

int logger_init(const char *log_file) {
    log_fp = fopen(log_file, "a");

    if (log_fp == NULL) 
        return -1;
    
    pthread_mutex_init(&log_mutex, NULL);

    return 0;
}

void logger_log(const char *format, ...) {
    pthread_mutex_lock(&log_mutex);

    time_t now = time(NULL);

    struct tm *t = localtime(&now);

    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "[%Y-%m-%d %H:%M:%S]", t);

    va_list args;

    va_start(args, format);

    printf("%s ", timestamp);

    vprintf(format, args);

    printf("\n");

    va_end(args);

    if (log_fp != NULL) {
        va_start(args, format);

        fprintf(log_fp, "%s", timestamp);

        vfprintf(log_fp, format, args);

        fprintf(log_fp, "\n");

        va_end(args);
    }

    fflush(stdout);
    fflush(log_fp);

    pthread_mutex_unlock(&log_mutex);
}

void logger_close(void) {
    if (log_fp != NULL) {
        fclose(log_fp);

        log_fp = NULL;
    }

    pthread_mutex_destroy(&log_mutex);
}