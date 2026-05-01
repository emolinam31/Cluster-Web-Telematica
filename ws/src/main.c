#include "logger.h"
#include "server.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int parse_port(const char *value, int *out_port) {
    char *end = NULL;
    long port;

    errno = 0;
    port = strtol(value, &end, 10);
    if (errno != 0 || end == value || *end != '\0' ||
        port < 1 || port > 65535) {
        return -1;
    }

    *out_port = (int)port;
    return 0;
}

static int is_directory(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

int main(int argc, char *argv[]) {
    int port;
    const char *log_file;
    const char *doc_root;

    if (argc != 4) {
        fprintf(stderr, "Uso: %s <HTTP_PORT> <LogFile> <DocumentRootFolder>\n",
                argv[0]);
        return 1;
    }

    if (parse_port(argv[1], &port) < 0) {
        fprintf(stderr, "Error: puerto invalido '%s'\n", argv[1]);
        return 1;
    }

    log_file = argv[2];
    doc_root = argv[3];

    if (!is_directory(doc_root)) {
        fprintf(stderr, "Error: DocumentRootFolder no existe o no es directorio: %s\n",
                doc_root);
        return 1;
    }

    if (logger_init(log_file) < 0) {
        fprintf(stderr, "Error: no se pudo abrir el log: %s\n", log_file);
        return 1;
    }

    logger_log("[TWS] Iniciando servidor");
    logger_log("[TWS] Puerto: %d", port);
    logger_log("[TWS] LogFile: %s", log_file);
    logger_log("[TWS] DocumentRoot: %s", doc_root);

    if (server_start(port, doc_root) < 0) {
        logger_close();
        return 1;
    }

    logger_close();
    return 0;
}
