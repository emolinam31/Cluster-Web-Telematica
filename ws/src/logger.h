/*
 * =============================================================================
 * TWS - logger.h - Interfaz del sistema de logging
 * =============================================================================
 *
 * REQUISITOS:
 *   1. Declarar logger_init(log_file) → abrir archivo de log
 *   2. Declarar logger_log(format, ...) → log dual (stdout + archivo)
 *   3. Declarar logger_close() → cerrar archivo de log
 *
 * NOTAS:
 *   - El PDF exige logging a AMBOS: salida estándar Y archivo
 *   - Debe ser thread-safe (múltiples threads escriben al log)
 * =============================================================================
 */

/* Autor: Camila Martinez 
   Función: Declarar las funciones del sistema de logger.c
*/

#ifndef TWS_LOGGER_H
#define TWS_LOGGER_H

int  logger_init(const char *log_file);
void logger_log(const char *format, ...);
void logger_close(void);

#endif