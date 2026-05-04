# Cambios Realizados

Este documento resume los cambios aplicados para completar la implementacion del proyecto PIBL-WS segun el PDF del proyecto y la guia de requisitos funcionales.

## Resumen General

Se completo la implementacion principal del TWS, se implemento el cache persistente del PIBL, se integro el cache al flujo del proxy, se ajusto la configuracion local de backends y se actualizo el README con instrucciones completas de compilacion, ejecucion, pruebas y despliegue AWS.

Tambien se verifico la compilacion en entorno Linux/WSL usando `make` y `gcc`.

## Archivos Modificados

### Adaptacion de sockets basada en Beej

Se ajusto la creacion de sockets para quedar mas alineada con la guia de Beej:

Referencia:

- https://beej.us/guide/bgnet/html/split/system-calls-or-bust.html#socket

Patron adoptado:

```c
getaddrinfo();
socket(res->ai_family, res->ai_socktype, res->ai_protocol);
bind();      /* para sockets listener */
listen();    /* para servidores */
connect();   /* para sockets cliente hacia backend */
freeaddrinfo();
```

Antes, parte del codigo construia manualmente estructuras `sockaddr_in` con `AF_INET`, `INADDR_ANY`, `htons()` e `inet_pton()`. Ese enfoque funcionaba para IPv4, pero era menos flexible y menos parecido al flujo recomendado por Beej.

Despues del cambio:

- TWS crea su socket listener con `getaddrinfo(NULL, port, ..., AI_PASSIVE)`.
- PIBL crea su socket listener con `getaddrinfo(NULL, port, ..., AI_PASSIVE)`.
- PIBL conecta a backends con `getaddrinfo(backend_ip, backend_port, ...)`.
- Se recorren los resultados de `getaddrinfo()` hasta encontrar uno que permita crear socket y hacer `bind()` o `connect()`.
- Se mantiene `SO_REUSEADDR` para evitar errores al reiniciar servidores.
- Se conserva el funcionamiento existente de threads, logs, Round Robin, cache y reenvio HTTP.
- Queda preparado para IPv4 e IPv6 mediante `AF_UNSPEC`.

Archivos afectados por esta adaptacion:

- `ws/src/server.c`
- `pibl/src/server.c`
- `pibl/src/proxy.c`

Verificacion posterior al cambio:

- `ws` compila correctamente en Linux/WSL.
- `pibl` compila correctamente en Linux/WSL.
- Se probo `GET /index.html` a traves del PIBL y respondio `200 OK`.
- Se probo `HEAD /index.html` y respondio headers correctos.

### `ws/src/main.c`

Antes el archivo tenia una mezcla de codigo de servidor, errores de sintaxis y una funcion `handle_client` definida dentro de `main`, lo cual impedia compilar correctamente.

Cambios realizados:

- Se dejo `main.c` solo como punto de entrada del TWS.
- Se implemento validacion de argumentos:
  - `HTTP_PORT`
  - `LogFile`
  - `DocumentRootFolder`
- Se agrego validacion robusta del puerto con `strtol`.
- Se valida que `DocumentRootFolder` exista y sea un directorio.
- Se inicializa el logger con `logger_init`.
- Se llama a `server_start(port, doc_root)`.
- Se eliminio la logica de sockets duplicada que no correspondia a este archivo.

Requisitos relacionados:

- RF-18: CLI del TWS.
- RF-17: inicializacion del logger.
- RF-16: arranque del servidor concurrente.

### `ws/src/server.c`

Antes estaba practicamente vacio, solo con comentarios e includes.

Cambios realizados:

- Se implemento `server_start`.
- Se crea socket TCP con `socket(AF_INET, SOCK_STREAM, 0)`.
- Se configura `SO_REUSEADDR`.
- Se hace `bind` en `INADDR_ANY`.
- Se hace `listen` con backlog `128`.
- Se acepta cada cliente con `accept`.
- Se crea un thread por cliente con `pthread_create`.
- Se usa `pthread_detach` para evitar hacer `join`.
- Se agrego estructura de contexto por cliente:
  - file descriptor
  - IP del cliente
  - document root
- Se implemento `handle_client`.
- Se implemento lectura de peticiones HTTP hasta detectar `\r\n\r\n`.
- Para POST, se sigue leyendo hasta completar el body indicado por `Content-Length`.
- Se integra el parser HTTP existente.
- Se responde `400 Bad Request` para peticiones malformadas.
- Se responde `404 Not Found` para recursos inexistentes.
- Se responde `200 OK` para recursos validos.
- Se implemento respuesta especial para `POST /submit`.
- Se registran peticiones y respuestas en el logger.
- Se libera memoria y se cierra el socket al terminar cada thread.

Requisitos relacionados:

- RF-12: parseo de GET, HEAD y POST.
- RF-13: respuesta 200.
- RF-14: respuesta 400.
- RF-15: respuesta 404.
- RF-16: concurrencia con threads.
- RF-17: logging.
- RF-19: servir desde DocumentRootFolder.

### `ws/src/http_response.c`

Antes no implementaba ninguna de las funciones declaradas.

Cambios realizados:

- Se implemento envio completo con funcion auxiliar `send_all`.
- Se implemento `http_response_200`.
- Se generan headers HTTP/1.1:
  - status line
  - `Content-Type`
  - `Content-Length`
  - `Connection: close`
- Para `GET` y `POST`, se envia el body del archivo.
- Para `HEAD`, se envian solo headers.
- Se lee el archivo en bloques de 4096 bytes para soportar archivos grandes.
- Se implemento `http_response_400`.
- Se implemento `http_response_404`.
- Se agrego `http_response_404_head` para responder 404 sin body cuando el metodo es HEAD.

Requisitos relacionados:

- RF-13: codigo 200 y envio de archivos.
- RF-14: codigo 400.
- RF-15: codigo 404.
- RF-19: envio de recursos desde DocumentRootFolder.

### `ws/src/http_response.h`

Cambios realizados:

- Se agrego la declaracion de `http_response_404_head`.

Requisitos relacionados:

- RF-15: `HEAD /recurso-inexistente` debe responder 404 sin body.

### `ws/src/file_handler.c`

Antes no implementaba funciones reales.

Cambios realizados:

- Se implemento `file_build_path`.
- Se resuelve `/` como `/index.html`.
- Se rechazan URIs sospechosas:
  - rutas que no empiezan con `/`
  - rutas con `..`
  - rutas con `\`
- Se implemento `file_exists` usando `stat` y `S_ISREG`.
- Se implemento `file_get_size`.
- Se implemento `file_get_content_type`.
- Se soportan MIME types:
  - `text/html`
  - `text/css`
  - `application/javascript`
  - `image/jpeg`
  - `image/png`
  - `image/gif`
  - `image/x-icon`
  - `text/plain`
  - `text/csv`
  - `application/octet-stream`

Requisitos relacionados:

- RF-13: `Content-Type` y `Content-Length`.
- RF-15: deteccion de recurso inexistente.
- RF-19: servir archivos y prevenir path traversal.

### `ws/src/logger.c`

Antes no implementaba el logger.

Cambios realizados:

- Se implemento `logger_init`.
- Se abre el archivo de log en modo append.
- Se implemento `logger_log`.
- Cada linea incluye timestamp con formato `YYYY-MM-DD HH:MM:SS`.
- Se escribe tanto en stdout como en archivo.
- Se usa `pthread_mutex_t` para evitar mezcla de logs entre threads.
- Se hace `fflush` despues de cada escritura.
- Se implemento `logger_close`.

Requisitos relacionados:

- RF-17: logger TWS a terminal y archivo.

### `pibl/src/cache.c`

Antes solo tenia comentarios e includes, y el proyecto no enlazaba porque faltaban `cache_init`, `cache_lookup`, `cache_store` y `cache_is_valid`.

Cambios realizados:

- Se implemento `cache_init`.
- Se guarda el TTL global.
- Se crea el directorio `cache/` si no existe.
- Si TTL es `0`, el cache queda deshabilitado.
- Se implemento `cache_lookup`.
- Se convierte URI a ruta de cache:
  - `/index.html` -> `cache/index.html`
  - `/css/style.css` -> `cache/css/style.css`
- Se valida TTL con `stat` y `st_mtime`.
- Si el recurso expiro, se elimina con `unlink`.
- Si el recurso es valido, se lee la respuesta HTTP completa desde disco.
- Se implemento `cache_store`.
- Se crean subdirectorios intermedios cuando son necesarios.
- Se escribe la respuesta HTTP completa: headers + body.
- Se implemento `cache_is_valid`.
- Se agrego mutex para proteger lecturas/escrituras concurrentes al cache.
- Se agrega header `Age` cuando se sirve una respuesta desde cache.
- Se rechazan URIs inseguras con:
  - `..`
  - `\`
  - query string con `?`

Requisitos relacionados:

- RF-08: cache en disco.
- RF-09: TTL configurable.
- RF-07: soporte para logs de HIT/MISS en integracion con server.

### `pibl/src/server.c`

Antes tenia los puntos TODO para cache y leia solo una vez del cliente.

Cambios realizados:

- Se incluyo `cache.h`.
- Se agrego lectura robusta de peticiones de cliente con `read_client_request`.
- Para POST, se lee hasta completar el body segun `Content-Length`.
- Se agrego lookup de cache antes de contactar backends.
- Solo se consulta cache para metodo `GET`.
- Si hay cache HIT:
  - se envia la respuesta desde disco al cliente
  - se registra `CACHE HIT`
  - no se contacta ningun backend
- Si no hay cache:
  - se mantiene el flujo Round Robin hacia backend
  - se mantiene failover al siguiente backend si uno falla
- Despues de recibir respuesta del backend:
  - se inserta header `Via: PIBL/1.0`
  - si es `GET` y la respuesta es `HTTP/1.1 200`, se guarda en cache
  - se registra `cache STORE`
- Se conserva envio al cliente en bloques de 4096 bytes.

Requisitos relacionados:

- RF-03: socket cliente hacia backend.
- RF-04: reenviar respuesta.
- RF-05: concurrencia.
- RF-07: logging.
- RF-08: cache en disco.
- RF-09: TTL.
- RF-10: Round Robin.

### `pibl/pibl.conf`

Antes tenia solo IPs privadas de ejemplo `10.0.1.x`, lo que hacia incomoda la prueba local.

Cambios realizados:

- Se dejo configuracion local lista:
  - `backend 127.0.0.1 8081`
  - `backend 127.0.0.1 8082`
  - `backend 127.0.0.1 8083`
- Se dejaron comentadas las lineas ejemplo para AWS:
  - `backend 10.0.1.10 8080`
  - `backend 10.0.1.11 8080`
  - `backend 10.0.1.12 8080`
- Se aclaro que en AWS deben reemplazarse por IPs privadas reales.

Requisitos relacionados:

- RF-11: archivo de configuracion.
- RF-10: lista de backends para Round Robin.
- RF-21: preparacion para AWS.

### `README.md`

Antes estaba incompleto y tenia placeholders.

Cambios realizados:

- Se agrego introduccion.
- Se agrego descripcion de arquitectura.
- Se documento TWS.
- Se documento PIBL.
- Se documento webapp y casos de prueba.
- Se agregaron comandos de compilacion.
- Se agregaron comandos de ejecucion local con tres TWS y un PIBL.
- Se agrego lista completa de pruebas con `curl`, `nc`, `stat` y logs.
- Se agrego seccion de despliegue AWS.
- Se agregaron conclusiones.
- Se agregaron referencias.

Pendiente manual:

- Completar nombres y codigos reales de los tres integrantes.
- Agregar capturas de pantalla si el profesor las exige en la sustentacion.

Requisitos relacionados:

- RF-22: documentacion final.

## Pruebas Realizadas

Las pruebas fueron ejecutadas en WSL/Linux.

### Compilacion TWS

Comando:

```bash
cd ws
make clean
make
```

Resultado:

- Compila correctamente con `gcc -Wall -Wextra -pthread`.

### Compilacion PIBL

Comando:

```bash
cd pibl
make clean
make
```

Resultado:

- Compila correctamente con `gcc -Wall -Wextra -pthread`.

### Pruebas Funcionales

Se levantaron tres TWS locales:

```bash
./tws 8081 logs/tws1.log ../webapp
./tws 8082 logs/tws2.log ../webapp
./tws 8083 logs/tws3.log ../webapp
```

Se levanto el PIBL:

```bash
./pibl 8080 pibl.conf logs/pibl.log 30
```

Resultados verificados:

- `GET /index.html` retorna `200 OK`.
- `HEAD /index.html` retorna headers correctos sin body.
- `GET /files/archivo_grande.bin` descarga `1048576` bytes.
- `POST /submit` retorna `200 OK`.
- `GET /no-existe.html` retorna `404 Not Found`.
- Cache MISS crea archivo en `pibl/cache/`.
- Cache HIT sirve desde disco y agrega header `Age`.
- Logs del PIBL muestran backend elegido y estado `MISS` o `CACHE HIT`.

## Lista Recomendada Para Probar Todo

### 1. Compilar

```bash
cd ws
make clean
make

cd ../pibl
make clean
make
```

### 2. Levantar Backends TWS

Terminal 1:

```bash
cd ws
mkdir -p logs
./tws 8081 logs/tws1.log ../webapp
```

Terminal 2:

```bash
cd ws
mkdir -p logs
./tws 8082 logs/tws2.log ../webapp
```

Terminal 3:

```bash
cd ws
mkdir -p logs
./tws 8083 logs/tws3.log ../webapp
```

### 3. Levantar PIBL

Terminal 4:

```bash
cd pibl
mkdir -p logs cache
./pibl 8080 pibl.conf logs/pibl.log 30
```

### 4. Probar GET

```bash
curl -v http://127.0.0.1:8080/index.html
```

Esperado:

- `HTTP/1.1 200 OK`
- `Content-Type: text/html`
- `Content-Length`

### 5. Probar HEAD

```bash
curl -I http://127.0.0.1:8080/index.html
```

Esperado:

- Headers de `200 OK`.
- Sin cuerpo.

### 6. Probar POST

```bash
curl -v -X POST http://127.0.0.1:8080/submit \
  -H "Content-Type: application/x-www-form-urlencoded" \
  -d "nombre=Esteban&mensaje=Hola"
```

Esperado:

- `HTTP/1.1 200 OK`.
- Registro en logs de TWS y PIBL.

### 7. Probar 404

```bash
curl -v http://127.0.0.1:8080/no-existe.html
```

Esperado:

- `HTTP/1.1 404 Not Found`.

### 8. Probar 400

```bash
printf 'BASURA\r\n\r\n' | nc 127.0.0.1 8080
```

Esperado:

- `HTTP/1.1 400 Bad Request`.

### 9. Probar Archivo Grande

```bash
curl -o /tmp/archivo_grande.bin http://127.0.0.1:8080/files/archivo_grande.bin
stat -c%s /tmp/archivo_grande.bin
```

Esperado:

```text
1048576
```

### 10. Probar Cache HIT/MISS

```bash
rm -f pibl/cache/index.html
curl -I http://127.0.0.1:8080/index.html
curl -I http://127.0.0.1:8080/index.html
tail -n 20 pibl/logs/pibl.log
```

Esperado:

- Primera peticion: `MISS`.
- Segunda peticion: `CACHE HIT`.
- Header `Age` en la segunda respuesta.

### 11. Probar TTL

Arrancar PIBL con TTL corto:

```bash
./pibl 8080 pibl.conf logs/pibl.log 2
```

Luego:

```bash
curl -I http://127.0.0.1:8080/index.html
sleep 3
curl -I http://127.0.0.1:8080/index.html
tail -n 20 pibl/logs/pibl.log
```

Esperado:

- Despues de esperar mas que el TTL, vuelve a consultar backend.

### 12. Probar Round Robin

```bash
for i in 1 2 3 4 5 6; do
  curl -s -o /dev/null http://127.0.0.1:8080/gallery.html
done
tail -n 30 pibl/logs/pibl.log
```

Esperado:

- Rotacion entre `8081`, `8082`, `8083`.

### 13. Probar Failover

Detener uno de los TWS y luego ejecutar:

```bash
curl -v http://127.0.0.1:8080/index.html
tail -n 20 pibl/logs/pibl.log
```

Esperado:

- PIBL registra fallo del backend caido.
- PIBL intenta el siguiente backend.

### 14. Probar Path Traversal

```bash
curl -v http://127.0.0.1:8080/../../../etc/passwd
```

Esperado:

- `400 Bad Request` o rechazo equivalente.

## Estado Final Frente A Requisitos

- PIBL escrito en C con sockets: completado.
- TWS escrito en C con sockets: completado.
- Concurrencia con threads: completado.
- HTTP/1.1 basico: completado.
- GET, HEAD, POST: completado.
- 200, 400, 404: completado.
- Logs stdout + archivo: completado.
- Round Robin: completado.
- Failover basico: completado.
- Cache en disco: completado.
- TTL configurable: completado.
- Webapp con cuatro casos: ya estaba presente y queda soportada.
- README de entrega: completado, pendiente solo nombres/codigos reales y capturas si se requieren.
- AWS: documentado; falta desplegarlo y actualizar `pibl.conf` con IPs privadas reales.
