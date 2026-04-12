/*
 * =============================================================================
 * TWS - server.c - Socket TCP listener + manejo de conexiones con threads
 * =============================================================================
 *
 * REQUISITOS QUE DEBE CUMPLIR ESTE ARCHIVO:
 *   1. Crear socket TCP: socket(AF_INET, SOCK_STREAM, 0)
 *   2. Configurar SO_REUSEADDR para evitar "Address already in use"
 *   3. bind() al puerto recibido, en INADDR_ANY (todas las interfaces)
 *   4. listen() con backlog de 128 conexiones
 *   5. Loop infinito: accept() → crear pthread por cada conexión
 *   6. Cada thread recibe un client_context_t con: client_fd, client_ip, doc_root
 *
 * PASOS A TOMAR:
 *   - Paso 1: Implementar server_start(port, doc_root)
 *   - Paso 2: En el loop, accept() → llenar client_context_t → pthread_create
 *   - Paso 3: pthread_detach() para no tener que hacer join
 *   - Paso 4: En handle_client():
 *       a. read() el request completo del cliente
 *       b. Llamar http_parse_request() para extraer método, URI, versión
 *       c. Si parseo falla → http_response_400()
 *       d. Si método es GET → buscar archivo → http_response_200() o 404
 *       e. Si método es HEAD → igual que GET pero sin body
 *       f. Si método es POST → leer body, procesar, responder 200
 *       g. Log de la petición y respuesta
 *       h. close(client_fd), free(ctx)
 *
 * CLAVES PARA EL ÉXITO:
 *   - TCP es un stream de bytes, read() puede no leer todo el request.
 *     Acumular en buffer hasta encontrar "\r\n\r\n" (fin de headers)
 *   - Para POST, después de "\r\n\r\n" viene el body, cuyo tamaño
 *     está en el header Content-Length
 *   - write() puede no enviar todo de una vez, usar loop de escritura
 *   - Siempre cerrar client_fd y liberar ctx al terminar el thread
 *   - Usar pthread_detach para evitar memory leaks de threads
 * =============================================================================
 */

#include "server.h"
#include "http_parser.h"
#include "http_response.h"
#include "file_handler.h"
#include "logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
