/*
 * =============================================================================
 * PIBL - proxy.h - Interfaz del módulo de proxy forwarding
 * =============================================================================
 *
 * REQUISITOS:
 *   1. Declarar proxy_forward(backend_ip, backend_port, request, response)
 *      → conectar al backend, enviar request, recibir response
 *   2. Retornar 0 si OK, -1 si el backend no responde
 *
 * NOTAS:
 *   - Esta función crea un socket NUEVO hacia el backend por cada petición
 *   - El request se envía tal cual al backend (transparente)
 *   - La respuesta se recibe completa y se devuelve al caller
 * =============================================================================
 */

#ifndef PIBL_PROXY_H
#define PIBL_PROXY_H

#include <stddef.h>

int proxy_forward(const char *backend_ip, int backend_port,
                  const char *request, size_t req_len,
                  char *response, size_t *resp_len, size_t resp_max);

#endif
