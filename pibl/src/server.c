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
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>



#define BACKLOG 128

int server_start(int port){
    int server_fd;
    int opt = 1;
    struct sockaddr_in server_addr;

    /* Crear socket TCP */
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0){
        perror("Error creando socket");
        return -1;
    }

    /* Permitir reutilizar el puerto */
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0){
        perror("Error configurando SO_REUSEADDR");
        close(server_fd);
        return -1;
    }

    /* Configurar IP y puerto del servidor */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    /* Asociar socket al puerto */
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0){
        if (errno == EADDRINUSE){
            fprintf(stderr, "Error: puerto %d en uso\n", port);
        }
        else{
            perror("Error en bind");
        }
        
        close(server_fd);
        return -1;
    }

    /* Escuchar conexiones */
    if (listen(server_fd, BACKLOG) < 0){
        perror("Error en el listen");
        close(server_fd);
        return -1;
    }

    printf("[PIBL] Servidor escuchando en puerto %d\n", port);

    /* Aceptar clientes indefinidamente */
    while(1){
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        /* Esperar una conexion TCP */
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);

        if (client_fd < 0){
            perror("Error en accept");
            continue;
        }

        printf("[PIBL] Conexion TCP aceptada\n");

        /* Cerrar cliente por ahora */
        close(client_fd);

    }

    close(server_fd);
    return 0;

}
