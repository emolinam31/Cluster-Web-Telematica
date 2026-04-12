/*
 * =============================================================================
 * PIBL - config.c - Lectura y parseo del archivo de configuración
 * =============================================================================
 *
 * REQUISITOS QUE DEBE CUMPLIR ESTE ARCHIVO:
 *   1. config_load():
 *      a. Abrir el archivo de configuración con fopen()
 *      b. Leer línea por línea con fgets()
 *      c. Parsear líneas con formato: "backend <IP> <PORT>"
 *         usando sscanf(line, "backend %s %d", ip, &port)
 *      d. Llenar el array de backends con IP y puerto
 *      e. Retornar la cantidad de backends leídos en *out_count
 *   2. Ignorar líneas vacías y comentarios (que empiecen con #)
 *   3. Validar que se leyeron al menos 3 backends
 *
 * PASOS A TOMAR:
 *   - Paso 1: fopen(config_file, "r") → si falla, retornar -1
 *   - Paso 2: Loop con fgets(line, sizeof(line), fp)
 *   - Paso 3: Saltar líneas vacías (\n) y comentarios (#)
 *   - Paso 4: sscanf para extraer "backend", ip, port
 *   - Paso 5: Copiar a backends[count++] con strncpy para ip, asignar port
 *   - Paso 6: fclose(fp), *out_count = count, retornar 0
 *
 * CLAVES PARA EL ÉXITO:
 *   - pibl.conf es un archivo de texto plano, cada línea = un backend
 *   - No hardcodear IPs → deben venir del archivo de configuración
 *   - Validar que el puerto sea un número válido (1-65535)
 *   - Si hay menos de 3 backends → log warning pero continuar
 *   - Usar strncpy (no strcpy) para evitar buffer overflow al copiar IPs
 *   - El archivo de configuración se lee UNA vez al inicio, no se recarga
 *   - Ejemplo de pibl.conf:
 *       # Backends del cluster TWS
 *       backend 10.0.1.10 8080
 *       backend 10.0.1.11 8080
 *       backend 10.0.1.12 8080
 * =============================================================================
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
