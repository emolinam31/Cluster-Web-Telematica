/*
 * =============================================================================
 * PIBL - proxy.c - Reenvío de peticiones hacia backends (proxy forwarding)
 * =============================================================================
 *
 * REQUISITOS QUE DEBE CUMPLIR ESTE ARCHIVO:
 *   1. proxy_forward(): Implementar el reenvío transparente:
 *      a. Crear socket TCP: socket(AF_INET, SOCK_STREAM, 0)
 *      b. Configurar timeout de conexión (ej: 5 segundos) con setsockopt
 *      c. connect() al backend (ip:port)
 *      d. Si connect() falla → retornar -1 (backend caído)
 *      e. write() el request completo al backend
 *      f. read() la respuesta completa del backend
 *      g. close() el socket al backend
 *      h. Retornar 0 con la respuesta en el buffer
 *
 * PASOS A TOMAR:
 *   - Paso 1: Crear socket + struct sockaddr_in con inet_pton(backend_ip)
 *   - Paso 2: Configurar SO_RCVTIMEO y SO_SNDTIMEO (5s) para evitar bloqueos
 *   - Paso 3: connect() → si falla, log + return -1
 *   - Paso 4: Loop de write() hasta enviar todo el request
 *   - Paso 5: Loop de read() hasta recibir toda la respuesta
 *     (detectar fin: Content-Length o conexión cerrada)
 *   - Paso 6: close(socket), retornar 0
 *
 * CLAVES PARA EL ÉXITO:
 *   - TIMEOUT es esencial: sin él, el proxy se cuelga si un backend muere
 *   - connect() puede tardar hasta 75s por defecto → poner timeout corto
 *   - La respuesta HTTP puede venir en múltiples read() → acumular en buffer
 *   - Saber cuándo terminó la respuesta:
 *     a. Buscar Content-Length en headers → leer exactamente esos bytes
 *     b. Si no hay Content-Length → leer hasta que el backend cierre conexión
 *   - NO modificar el request (el PIBL es transparente), excepto:
 *     Opcionalmente ajustar el header Host al backend
 * =============================================================================
 */

#include "proxy.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
