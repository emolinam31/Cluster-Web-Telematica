#
# =============================================================================
# PIBL (Proxy Inverso + Balanceador de Carga) - Makefile
# =============================================================================
#
# REQUISITOS QUE DEBE CUMPLIR ESTE ARCHIVO:
#   1. Compilar todos los .c de src/ en un ejecutable llamado "pibl"
#   2. Usar gcc con flags: -Wall -Wextra -pthread
#   3. Targets requeridos:
#      - all: compilar el proyecto
#      - clean: borrar ejecutable y archivos .o
#      - run: ejecutar con argumentos por defecto (para pruebas rápidas)
#
# PASOS A TOMAR:
#   - Paso 1: Definir CC=gcc, CFLAGS=-Wall -Wextra -pthread
#   - Paso 2: Listar todos los SRCS
#   - Paso 3: Regla all: compilar
#   - Paso 4: Regla clean: limpiar
#   - Paso 5: Regla run: ejecutar con parámetros de prueba
#
# CLAVES PARA EL ÉXITO:
#   - El ejecutable resultante se llama "pibl"
#   - Se ejecuta así: ./pibl <puerto> <config_file> <log_file> <cache_ttl>
#   - Ejemplo: ./pibl 80 pibl.conf logs/pibl.log 300
#   - CacheTTL=0 desactiva el caché
#   - CacheTTL=300 → caché válido por 5 minutos
#   - Usar tabs (NO espacios) para la indentación de las reglas
# =============================================================================

CC = gcc
CFLAGS = -Wall -Wextra -pthread

SRCS = src/main.c src/server.c src/proxy.c src/balancer.c \
       src/cache.c src/http_parser.c src/config.c src/logger.c

TARGET = pibl

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS)

clean:
	rm -f $(TARGET)

run: $(TARGET)
	./$(TARGET) 80 pibl.conf logs/pibl.log 300

.PHONY: all clean run
