/*
 * =============================================================================
 * PIBL - balancer.h - Interfaz del balanceador de carga (Round Robin)
 * =============================================================================
 *
 * Cubre RF-10 (Round Robin) y es dependencia directa del RF-04: server.c
 * pide el siguiente backend con balancer_next() para reenviar cada peticion.
 *
 * API:
 *   balancer_init(backends, n) -> copia los backends y resetea el indice.
 *   balancer_next()            -> devuelve el siguiente backend (rota 0..n-1).
 *   balancer_count()           -> cuantos backends hay (sirve para failover:
 *                                 server.c reintenta hasta count() backends).
 *
 * Es THREAD-SAFE: el indice esta protegido con pthread_mutex_t porque
 * varios threads (uno por cliente) llaman balancer_next() en paralelo.
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
int        balancer_count(void);

#endif
