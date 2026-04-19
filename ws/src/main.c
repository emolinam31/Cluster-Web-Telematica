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

#define BUFFER_SIZE 8192

typedef struct {
    int client_fd;
    char client_ip[INET_ADDRSTRLEN];
    char *doc_root;
    char *log_file;
} client_context_t;

void *handle_client(void *arg);

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Uso: %s <HTTP_PORT> <LogFile> <DocumentRootFolder>\n", argv[0]);
        return 1;
    }
    
    int port = atoid(argv[1]);
    char *log_file = argv[2];
    char *doc_root = argv[3];

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    //devolcemos error si el socket no se crea
    if (server_fd < 0){
        perror("Error creando socket")
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET,SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sinaddr.s_addr = INADDR_ANY,
        .sin_port = htons(port)
    };

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 128) < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }
    printf("[WS] Servidor escuchando en puerto %d\n", port);
    printf("[WS] DocumentRoot: %s\n", doc_root);
    printf("[WS] LogFile: %s\n", log_file);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }

        client_context_t *ctx = malloc(sizeof(client_context_t));
        ctx->client_fd = client_fd;
        ctx->doc_root = doc_root;
        ctx->log_file = log_file;
        inet_ntop(AF_INET, &client_addr.sin_addr, ctx->client_ip, sizeof(ctx->client_ip));

        pthread_t thread;
        pthread_create(&thread, NULL, handle_client, ctx);
        pthread_detach(thread);
    }

    close(server_fd);
    return 0;
}
