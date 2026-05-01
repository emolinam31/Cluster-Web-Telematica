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

#include <sys/socket.h>

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

    void *handle_client(void *arg) {
        client_context_t *ctx = (client_context_t *)arg;
        char buffer[BUFFER_SIZE];
    
        ssize_t n = read(ctx->client_fd, buffer, sizeof(buffer) - 1);
        if (n <= 0) {
            close(ctx->client_fd);
            free(ctx);
            return NULL;
        }
        buffer[n] = '\0';
    
        /* Parsear Request-Line: METHOD URI VERSION */
        char method[8], uri[2048], version[16];
        if (sscanf(buffer, "%7s %2047s %15s", method, uri, version) != 3) {
            char *resp = "HTTP/1.1 400 Bad Request\r\nContent-Length: 11\r\n\r\nBad Request";
            write(ctx->client_fd, resp, strlen(resp));
            close(ctx->client_fd);
            free(ctx);
            return NULL;
        }
    
        /* Log a stdout */
        printf("[WS] %s -> %s %s %s\n", ctx->client_ip, method, uri, version);
    
        /* Construir ruta completa del archivo */
        char filepath[4096];
        if (strcmp(uri, "/") == 0)
            snprintf(filepath, sizeof(filepath), "%s/index.html", ctx->doc_root);
        else
            snprintf(filepath, sizeof(filepath), "%s%s", ctx->doc_root, uri);
    
        /* Verificar si el archivo existe */
        FILE *file = fopen(filepath, "rb");
        if (!file) {
            char *resp = "HTTP/1.1 404 Not Found\r\n"
                         "Content-Type: text/html\r\n"
                         "Content-Length: 44\r\n"
                         "\r\n"
                         "<html><body>404 - Not Found</body></html>";
            write(ctx->client_fd, resp, strlen(resp));
            printf("[WS] %s <- 404 Not Found (%s)\n", ctx->client_ip, uri);
            close(ctx->client_fd);
            free(ctx);
            return NULL;
        }
    
        /* Obtener tamaño del archivo */
        fseek(file, 0, SEEK_END);
        long file_size = ftell(file);
        fseek(file, 0, SEEK_SET);
    
        /* Determinar Content-Type básico */
        const char *content_type = "application/octet-stream";
        if (strstr(uri, ".html")) content_type = "text/html";
        else if (strstr(uri, ".css")) content_type = "text/css";
        else if (strstr(uri, ".js")) content_type = "application/javascript";
        else if (strstr(uri, ".png")) content_type = "image/png";
        else if (strstr(uri, ".jpg")) content_type = "image/jpeg";
        else if (strstr(uri, ".gif")) content_type = "image/gif";
    
        /* Enviar headers */
        char header[512];
        int header_len = snprintf(header, sizeof(header),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: %s\r\n"
            "Content-Length: %ld\r\n"
            "\r\n",
            content_type, file_size);
    
        write(ctx->client_fd, header, header_len);
    
        /* GET: enviar body | HEAD: solo headers (ya enviados) */
        if (strcmp(method, "GET") == 0 || strcmp(method, "POST") == 0) {
            char file_buf[4096];
            size_t bytes;
            while ((bytes = fread(file_buf, 1, sizeof(file_buf), file)) > 0) {
                write(ctx->client_fd, file_buf, bytes);
            }
        }
        /* HEAD: no envía body, solo los headers de arriba */
    
        printf("[WS] %s <- 200 OK %s (%ld bytes)\n", ctx->client_ip, uri, file_size);
    
        fclose(file);
        close(ctx->client_fd);
        free(ctx);
        return NULL;
    }
}
