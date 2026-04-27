/*
 * =============================================================================
 * PIBL - balancer.c - Balanceador de carga con algoritmo Round Robin
 * =============================================================================
 *
 * REQUISITOS QUE DEBE CUMPLIR ESTE ARCHIVO:
 *   1. balancer_init(): Recibir array de backends y cantidad,
 *      guardarlos en variables estáticas
 *   2. balancer_next(): Retornar el siguiente backend en rotación
 *      usando Round Robin (índice circular: current++ % num_backends)
 *   3. Ser THREAD-SAFE: proteger el índice current con pthread_mutex_t
 *
 * PASOS A TOMAR:
 *   - Paso 1: Variables estáticas: backend_t *backends, int count, int current
 *   - Paso 2: pthread_mutex_t para proteger current
 *   - Paso 3: balancer_init() → copiar backends, init mutex, current = 0
 *   - Paso 4: balancer_next():
 *       a. lock()
 *       b. idx = current % count
 *       c. current++
 *       d. unlock()
 *       e. return &backends[idx]
 *
 * CLAVES PARA EL ÉXITO:
 *   - SIN MUTEX → race condition: dos threads podrían obtener el mismo backend
 *   - Round Robin es simple pero efectivo para distribuir carga
 *   - El PDF dice mínimo 3 backends: backend_1, backend_2, backend_3
 *   - Si current llega a INT_MAX → podría overflow, resetear periódicamente
 *     o usar unsigned int que hace wrap-around natural
 *   - Considerar agregar un mecanismo de "backend activo/inactivo" para
 *     failover: si proxy_forward falla, marcar backend como inactivo
 *     y no seleccionarlo en futuras rotaciones
 * =============================================================================
 */

/* Autor: Camila Martinez
 * Función: Dar información sobre los archivos en disco.
 */

#include "balancer.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

static backend_t *backends_list = NULL;

static int backends_count = 0;

static unsigned int current_index = 0;

static pthread_mutex_t balancer_mutex;

int balancer_init(backend_t *backends, int num_backends) {
    if (backends == NULL || num_backends < 1) {
        fprintf(stderr, "Error: backends nulos o parametros inválidosn");

        return -1;
    }
    
    backends_list = malloc(sizeof(backend_t) * num_backends);

    if (backends_list == NULL) {
        fprintf(stderr, "Error: No se pudo asignar memoria para backendsn, error al reservar memoria\n");

        return -1;
    }

    memcpy(backends_list, backends, sizeof(backend_t) * num_backends);

    backends_count = num_backends;

    current_index = 0;

    pthread_mutex_init(&balancer_mutex, NULL);

    return 0;
}

backend_t *balancer_next(void) {
    if (backends_list == NULL) {
        fprintf(stderr, "Error: Balancer no inicializado, backends_list es nulo\n");

        return NULL;
    }

    pthread_mutex_lock(&balancer_mutex);

    int idx = (int)(current_index % (unsigned int)backends_count);

    current_index++;

    pthread_mutex_unlock(&balancer_mutex);

    return &backends_list[idx];
}