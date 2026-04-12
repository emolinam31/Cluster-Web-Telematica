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

#include "balancer.h"

#include <stdio.h>
#include <string.h>
#include <pthread.h>
