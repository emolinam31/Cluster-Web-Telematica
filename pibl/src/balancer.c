/*
 * =============================================================================
 * PIBL - balancer.c - Balanceador de carga con algoritmo Round Robin (RF-10)
 * =============================================================================
 *
 * Implementacion thread-safe:
 *   - g_backends[] guarda la lista de backends (copia local)
 *   - g_current es el indice del proximo backend a entregar
 *   - g_lock serializa el acceso a g_current y g_count
 *
 * Dependencia directa del RF-04: server.c -> balancer_next() -> proxy_forward().
 * =============================================================================
 */

#include "balancer.h"

#include <stdio.h>
#include <string.h>
#include <pthread.h>

/* Mismo limite que MAX_BACKENDS en config.h: protege el array estatico */
#define BALANCER_MAX 16

static backend_t       g_backends[BALANCER_MAX];
static int             g_count   = 0;
static int             g_current = 0;
static pthread_mutex_t g_lock    = PTHREAD_MUTEX_INITIALIZER;

int balancer_init(backend_t *backends, int num_backends) {
    if (backends == NULL || num_backends <= 0) {
        fprintf(stderr, "[BALANCER] init: parametros invalidos\n");
        return -1;
    }
    if (num_backends > BALANCER_MAX) {
        fprintf(stderr,
                "[BALANCER] init: %d backends excede el max (%d), recortando\n",
                num_backends, BALANCER_MAX);
        num_backends = BALANCER_MAX;
    }

    pthread_mutex_lock(&g_lock);
    for (int i = 0; i < num_backends; i++) {
        g_backends[i] = backends[i];
    }
    g_count   = num_backends;
    g_current = 0;
    pthread_mutex_unlock(&g_lock);

    printf("[BALANCER] inicializado con %d backends\n", num_backends);
    return 0;
}

backend_t *balancer_next(void) {
    backend_t *out = NULL;
    pthread_mutex_lock(&g_lock);
    if (g_count > 0) {
        out = &g_backends[g_current];
        g_current = (g_current + 1) % g_count;
    }
    pthread_mutex_unlock(&g_lock);
    return out;
}

int balancer_count(void) {
    int c;
    pthread_mutex_lock(&g_lock);
    c = g_count;
    pthread_mutex_unlock(&g_lock);
    return c;
}
