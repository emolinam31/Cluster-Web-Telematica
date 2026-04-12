#
# =============================================================================
# TWS (Telematics Web Server) - Makefile
# =============================================================================
#
# REQUISITOS QUE DEBE CUMPLIR ESTE ARCHIVO:
#   1. Compilar todos los .c de src/ en un ejecutable llamado "tws"
#   2. Usar gcc con flags: -Wall -Wextra -pthread
#      - -Wall -Wextra: mostrar todas las advertencias (buena práctica)
#      - -pthread: enlazar con la librería de threads (obligatorio)
#   3. Targets requeridos:
#      - all: compilar el proyecto
#      - clean: borrar ejecutable y archivos .o
#      - run: ejecutar con argumentos por defecto (para pruebas rápidas)
#
# PASOS A TOMAR:
#   - Paso 1: Definir CC=gcc, CFLAGS=-Wall -Wextra -pthread
#   - Paso 2: Listar todos los SRCS (archivos .c)
#   - Paso 3: Generar OBJS (cambiar .c por .o)
#   - Paso 4: Regla all: $(CC) $(CFLAGS) -o tws $(SRCS)
#   - Paso 5: Regla clean: rm -f tws *.o src/*.o
#   - Paso 6: Regla run: ./tws 8080 logs/tws.log ../../webapp
#
# CLAVES PARA EL ÉXITO:
#   - El ejecutable resultante se llama "tws"
#   - Se ejecuta así: ./tws <puerto> <log_file> <document_root>
#   - Ejemplo: ./tws 8080 logs/tws.log ../../webapp
#   - Usar tabs (NO espacios) para la indentación de las reglas
#   - Si falta -pthread → errores de undefined reference a pthread_create
# =============================================================================

CC = gcc
CFLAGS = -Wall -Wextra -pthread

SRCS = src/main.c src/server.c src/http_parser.c src/http_response.c \
       src/file_handler.c src/logger.c

TARGET = tws

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS)

clean:
	rm -f $(TARGET)

run: $(TARGET)
	./$(TARGET) 8080 logs/tws.log ../../webapp

.PHONY: all clean run
