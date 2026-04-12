/*
 * =============================================================================
 * PIBL - logger.h - Interfaz del sistema de logging (idéntico al TWS)
 * =============================================================================
 *
 * REQUISITOS:
 *   1. Declarar logger_init(log_file) → abrir archivo de log
 *   2. Declarar logger_log(format, ...) → log dual (stdout + archivo)
 *   3. Declarar logger_close() → cerrar archivo de log
 *
 * NOTAS:
 *   - Funcionalidad idéntica al logger del TWS
 *   - Log dual: stdout Y archivo
 *   - Thread-safe con mutex
 * =============================================================================
 */

#ifndef PIBL_LOGGER_H
#define PIBL_LOGGER_H

int  logger_init(const char *log_file);
void logger_log(const char *format, ...);
void logger_close(void);

#endif
