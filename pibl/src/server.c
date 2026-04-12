/*
 * =============================================================================
 * PIBL - server.c - Socket TCP listener del proxy inverso
 * =============================================================================
 *
 * REQUISITOS QUE DEBE CUMPLIR ESTE ARCHIVO:
 *   1. Crear socket TCP, bind, listen (idéntico al TWS)
 *   2. accept() en loop → crear pthread por conexión
 *   3. handle_client() flujo:
 *      a. read() la petición del cliente
 *      b. Parsear con http_parse_request()
 *      c. Si parseo falla → enviar 400 Bad Request directo al cliente
 *      d. Verificar si la respuesta está en caché (cache_lookup)
 *         - Si está en caché y es válida → servir desde caché + header Age
 *         - Si no → continuar al paso e
 *      e. Obtener backend con balancer_next()
 *      f. Reenviar petición al backend con proxy_forward()
 *      g. Recibir respuesta del backend
 *      h. Guardar respuesta en caché (cache_store) si aplica
 *      i. Agregar header "Via: PIBL/1.0" a la respuesta
 *      j. Enviar respuesta al cliente
 *      k. Log de toda la transacción
 *      l. close(), free()
 *
 * PASOS A TOMAR:
 *   - Paso 1: Reusar la estructura de socket del TWS
 *   - Paso 2: handle_client con el flujo proxy descrito arriba
 *   - Paso 3: Si un backend falla, intentar con el siguiente (failover)
 *
 * CLAVES PARA EL ÉXITO:
 *   - El proxy NO interpreta el body de la respuesta, solo lo reenvía
 *   - El header "Via" identifica que la respuesta pasó por el proxy
 *   - El header "Age" indica cuántos segundos lleva en caché
 *   - Si TODOS los backends fallan → responder 502 Bad Gateway
 *   - Mismo cuidado con read/write parciales que en el TWS
 * =============================================================================
 */

#include "server.h"
#include "http_parser.h"
#include "proxy.h"
#include "balancer.h"
#include "cache.h"
#include "logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
