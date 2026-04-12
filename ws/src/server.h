/*
 * =============================================================================
 * TWS - server.h - Interfaz del servidor TCP
 * =============================================================================
 *
 * REQUISITOS:
 *   1. Declarar server_start(int port, const char *doc_root)
 *   2. Declarar la estructura client_context_t que se pasa a cada thread
 *
 * NOTAS:
 *   - server.c es quien hace socket(), bind(), listen(), accept()
 *   - Cada conexión se maneja en un thread separado (pthread)
 * =============================================================================
 */

#ifndef TWS_SERVER_H
#define TWS_SERVER_H

int server_start(int port, const char *doc_root);

#endif
