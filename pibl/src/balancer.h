/*
 * =============================================================================
 * PIBL - balancer.h - Interfaz del balanceador de carga (Round Robin)
 * =============================================================================
 *
 * REQUISITOS:
 *   1. Definir estructura backend_t: { char ip[64]; int port; }
 *   2. Declarar balancer_init(backends[], num_backends)
 *   3. Declarar balancer_next() → retorna puntero al próximo backend
 *
 * NOTAS:
 *   - Round Robin: rota entre backends 0, 1, 2, 0, 1, 2, ...
 *   - Debe ser thread-safe (múltiples threads llaman balancer_next)
 *   - Mínimo 3 backends (según PDF)
 * =============================================================================
 */

#ifndef PIBL_BALANCER_H
#define PIBL_BALANCER_H

typedef struct {
    char ip[64];
    int  port;
} backend_t;

int        balancer_init(backend_t *backends, int num_backends);
backend_t *balancer_next(void);

#endif
