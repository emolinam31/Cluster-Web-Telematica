/*
 * =============================================================================
 * TWS (Telematics Web Server) - Punto de entrada
 * =============================================================================
 *
 * REQUISITOS QUE DEBE CUMPLIR ESTE ARCHIVO:
 *   1. Parsear argumentos CLI: ./server <HTTP_PORT> <LogFile> <DocumentRootFolder>
 *      - HTTP_PORT: puerto donde escucha el TWS (ej: 8080)
 *      - LogFile: ruta al archivo de log (ej: logs/tws.log)
 *      - DocumentRootFolder: carpeta raíz de recursos web (ej: ../../webapp)
 *   2. Validar que los 3 argumentos estén presentes, si no → error con uso correcto
 *   3. Inicializar el logger (llamar a logger_init con el LogFile)
 *   4. Llamar a server_start(port, doc_root) para iniciar el socket listener
 *
 * PASOS A TOMAR:
 *   - Paso 1: Validar argc == 4
 *   - Paso 2: Extraer port (atoi), log_file, doc_root de argv
 *   - Paso 3: Verificar que doc_root existe (opendir o stat)
 *   - Paso 4: Llamar logger_init(log_file)
 *   - Paso 5: Llamar server_start(port, doc_root)
 *
 * CLAVES PARA EL ÉXITO:
 *   - El programa se ejecuta así: $./server 8080 logs/tws.log ../../webapp
 *   - Si el puerto está ocupado, mostrar error claro
 *   - Si DocumentRootFolder no existe, abortar con mensaje descriptivo
 *   - Este archivo NO debe contener lógica de sockets ni HTTP,
 *     solo parseo de args e inicialización
 * =============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "server.h"
#include "logger.h"
