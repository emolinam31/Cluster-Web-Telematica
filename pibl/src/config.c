/*
 * =============================================================================
 * PIBL - config.c - Lectura y parseo del archivo de configuración (RF-11)
 * =============================================================================
 * Lee un archivo de texto plano (pibl.conf) línea por línea y extrae la lista
 * de backends en formato:
 *     backend <IP> <PUERTO>
 *
 * Ignora:
 *   - Líneas que empiezan con '#' (comentarios)
 *   - Líneas vacías
 *
 * Avisa (warning) si:
 *   - Una línea no respeta el formato esperado
 *   - El puerto está fuera del rango [1, 65535]
 *   - Se cargaron menos de 3 backends (mínimo recomendado por el PDF)
 *
 * Retorna -1 si:
 *   - El archivo no se puede abrir
 *   - No se cargó ningún backend válido
 * =============================================================================
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 
Tamaño máximo de cada línea del archivo de configuración.
*/
#define MAX_LINEA 256

int config_load(const char *config_file, backend_t *backends,
                int max_backends, int *out_count) {
                
    /*
        Paso 0: Validar los parámetros de entrada.
    */
    if (config_file == NULL || backends == NULL || out_count == NULL) {
        fprintf(stderr, "[CONFIG] Error: parametros invalidos (NULL)\n");
        return -1;
    }
    /*
        Paso 1: abrir el archivo en modo lectura ("r")
        si no se puede abrir, retornar -1
    */
    FILE *fp = fopen(config_file, "r");
    if (fp == NULL) {
        fprintf(stderr, "[CONFIG] Error: no se pudo abrir '%s'\n", config_file);
        return -1;
    }

    char linea[MAX_LINEA];   /* buffer para cada línea leída */
    int  contador  = 0;      /* cuántos backends válidos llevamos cargados */
    int  num_linea = 0;      /* número de línea actual (para mensajes de error) */

    /* 
        Paso 2: leer el archivo línea por línea con fgets.
        fgets devuelve NULL al llegar al fin de archivo (EOF).
    */
    while (fgets(linea, sizeof(linea), fp) != NULL) {
        num_linea++;

        /* 2.1 - Saltar líneas de comentarios */
        if (linea[0] == '#') {
            continue;
        }

        /* 
            2.1 - Saltar líneas vacías (solo contienen '\n', '\r' o nada).
            Esto cubre el caso de líneas en blanco entre bloques.
        */
        if (linea[0] == '\n' || linea[0] == '\r' || linea[0] == '\0') {
            continue;
        }

        /* 
            2.2 - Cortar si ya llenamos el array (defensa contra archivos con demasiados backends para el tamaño que reservó main.c).
         */
        if (contador >= max_backends) {
            fprintf(stderr,
                    "[CONFIG] Warning: se alcanzo el max de backends (%d), "
                    "se ignoran las lineas restantes\n", max_backends);
            break;
        }

        /* 
            Paso 3: parsear el formato "backend <IP> <PUERTO>" con sscanf.         
        */
        char etiqueta[16];   
        char ip[64];         
        int  puerto;        

        int campos_leidos = sscanf(linea, "%15s %63s %d",
                                   etiqueta, ip, &puerto);

        /* 
            3.1 - La línea debe tener exactamente 3 campos 
        */
        if (campos_leidos != 3) {
            fprintf(stderr,
                    "[CONFIG] Warning: linea %d malformada, ignorada\n",
                    num_linea);
            continue;
        }

        /* 
            3.2 - El primer campo debe ser literalmente "backend".
        */
        if (strcmp(etiqueta, "backend") != 0) {
            fprintf(stderr,
                    "[CONFIG] Warning: linea %d no empieza con 'backend', "
                    "ignorada\n", num_linea);
            continue;
        }

        /* 
            3.3 - El puerto debe estar en el rango válido de TCP/IP 
        */
        if (puerto < 1 || puerto > 65535) {
            fprintf(stderr,"[CONFIG] Warning: puerto %d invalido en linea %d, "
                    "ignorada\n", puerto, num_linea);
            continue;
        }

        /*
            Paso 4: copiar la IP al array de backends.
        */
        strncpy(backends[contador].ip, ip, sizeof(backends[contador].ip) - 1);
        backends[contador].ip[sizeof(backends[contador].ip) - 1] = '\0';
        backends[contador].port = puerto;

        
        printf("[CONFIG] Backend %d cargado: %s:%d\n", contador + 1, backends[contador].ip, backends[contador].port);

        contador++;
    }

    /*
      Paso 5: cerrar el archivo (siempre, no dejarlo abierto)
    */
    fclose(fp);

    /*
      Paso 6: validaciones finales
    */

    /* Sin ningún backend válido -> error fatal: el PIBL no puede operar */
    if (contador == 0) {
        fprintf(stderr,
                "[CONFIG] Error: no se cargo ningun backend valido de '%s'\n",
                config_file);
        return -1;
    }

    /* Menos de 3 backends -> warning pero seguimos (el PDF solo lo recomienda) */
    if (contador < 3) {
        fprintf(stderr,
                "[CONFIG] Warning: solo se cargaron %d backends "
                "(el PDF recomienda minimo 3)\n", contador);
    }

    /* Devolver la cantidad por puntero de salida */
    *out_count = contador;
    return 0;
}