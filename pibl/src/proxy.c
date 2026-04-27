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
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#define PROXY_TIMEOUT_SEC 5

int proxy_forward(const char *backend_ip, int backend_port,
                  const char *request, size_t req_len,
                  char *response, size_t *resp_len, size_t resp_max) {
    if (!backend_ip || !request || !response || !resp_len || resp_max == 0) {
        return -1;
    }
    *resp_len = 0;

    /* Paso 1: crear socket TCP nuevo hacia el backend */
    int backend_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (backend_fd < 0) {
        perror("[PIBL] socket(backend)");
        return -1;
    }

    /* Paso 2: timeouts de envío y recepción para no colgarse si el backend muere */
    struct timeval tv = { .tv_sec = PROXY_TIMEOUT_SEC, .tv_usec = 0 };
    if (setsockopt(backend_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        perror("[PIBL] setsockopt(SO_RCVTIMEO)");
    }
    if (setsockopt(backend_fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
        perror("[PIBL] setsockopt(SO_SNDTIMEO)");
    }

    /* Paso 3: armar dirección del backend */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)backend_port);
    if (inet_pton(AF_INET, backend_ip, &addr.sin_addr) != 1) {
        fprintf(stderr, "[PIBL] IP de backend invalida: %s\n", backend_ip);
        close(backend_fd);
        return -1;
    }

    /* Paso 4: connect() — si falla, el caller probará otro backend */
    if (connect(backend_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "[PIBL] connect %s:%d fallo: %s\n",
                backend_ip, backend_port, strerror(errno));
        close(backend_fd);
        return -1;
    }

    /* Paso 5: reenviar la petición íntegra (write puede ser parcial) */
    size_t sent = 0;
    while (sent < req_len) {
        ssize_t n = write(backend_fd, request + sent, req_len - sent);
        if (n < 0) {
            if (errno == EINTR) continue;
            fprintf(stderr, "[PIBL] write a backend %s:%d fallo: %s\n",
                    backend_ip, backend_port, strerror(errno));
            close(backend_fd);
            return -1;
        }
        if (n == 0) break;
        sent += (size_t)n;
    }

    /* Paso 6: leer la respuesta completa hasta EOF o llenar el buffer */
    size_t total = 0;
    while (total < resp_max) {
        ssize_t n = read(backend_fd, response + total, resp_max - total);
        if (n > 0) {
            total += (size_t)n;
            continue;
        }
        if (n == 0) {
            break;
        }
        if (errno == EINTR) continue;
        fprintf(stderr, "[PIBL] read de backend %s:%d fallo: %s\n",
                backend_ip, backend_port, strerror(errno));
        close(backend_fd);
        *resp_len = total;
        return -1;
    }

    /* Paso 7: cerrar y devolver */
    close(backend_fd);
    *resp_len = total;
    return 0;
}
