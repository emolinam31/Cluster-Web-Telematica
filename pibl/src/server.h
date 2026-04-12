/*
 * =============================================================================
 * PIBL - server.h - Interfaz del servidor TCP del proxy
 * =============================================================================
 *
 * REQUISITOS:
 *   1. Declarar server_start(int port)
 *   2. La estructura de contexto para cada thread de cliente
 *
 * NOTAS:
 *   - El server del PIBL funciona igual que el del TWS en cuanto a
 *     socket/bind/listen/accept/pthread
 *   - La diferencia es que handle_client() NO sirve archivos, sino que
 *     reenvía la petición a un backend (proxy_forward)
 * =============================================================================
 */

#ifndef PIBL_SERVER_H
#define PIBL_SERVER_H

int server_start(int port);

#endif
