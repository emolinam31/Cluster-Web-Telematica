/*
 * =============================================================================
 * PIBL - config.h - Interfaz para lectura del archivo de configuración
 * =============================================================================
 *
 * REQUISITOS:
 *   1. Declarar config_load(config_file, backends[], max, &count) → 0 OK, -1 error
 *   2. Leer el archivo pibl.conf y extraer la lista de backends
 *
 * NOTAS:
 *   - Formato de pibl.conf:
 *       backend 192.168.1.10 8080
 *       backend 192.168.1.11 8080
 *       backend 192.168.1.12 8080
 *   - Mínimo 3 backends configurados
 * =============================================================================
 */

#ifndef PIBL_CONFIG_H
#define PIBL_CONFIG_H

#include "balancer.h"

int config_load(const char *config_file, backend_t *backends,
                int max_backends, int *out_count);

#endif
