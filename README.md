# Cluster-Web-Telematica

Proyecto 1: Proxy Inverso + Balanceador de Carga + Web Server (PIBL-WS).

## Integrantes

- Esteban Molina - Codigo
- Integrante 2 - Codigo
- Integrante 3 - Codigo

## Introduccion

Este proyecto implementa una arquitectura cliente/servidor HTTP/1.1 compuesta por un Proxy Inverso + Balanceador de Carga (PIBL) y tres servidores web TWS. El cliente se conecta unicamente al PIBL; el PIBL recibe la peticion, selecciona un backend mediante Round Robin, reenvia la solicitud al TWS elegido, recibe la respuesta y la retorna al cliente.

La solucion esta escrita en C usando API Sockets POSIX y pthreads. El PIBL incorpora cache persistente en disco con TTL configurable, de forma que las respuestas GET exitosas se almacenan en `pibl/cache/` y pueden servirse posteriormente sin contactar al backend mientras sigan vigentes.

## Desarrollo

### Arquitectura

```
Cliente HTTP
    |
    v
PIBL - Proxy Inverso + Balanceador
    |-- TWS 1 - DocumentRoot webapp/
    |-- TWS 2 - DocumentRoot webapp/
    |-- TWS 3 - DocumentRoot webapp/
```

En pruebas locales los TWS escuchan en `127.0.0.1:8081`, `127.0.0.1:8082` y `127.0.0.1:8083`; el PIBL escucha en `8080`. En AWS se deben reemplazar esos backends por las IPs privadas de las tres instancias TWS.

### TWS

El TWS se ejecuta como:

```bash
./tws <HTTP_PORT> <LogFile> <DocumentRootFolder>
```

Funciones implementadas:

- Parsing de HTTP/1.1 para `GET`, `HEAD` y `POST`.
- Validacion de `Host`, version `HTTP/1.1` y request-line.
- Respuestas `200 OK`, `400 Bad Request` y `404 Not Found`.
- Servicio de archivos desde `DocumentRootFolder`.
- MIME types: HTML, CSS, JS, JPG/JPEG, PNG, GIF, ICO, TXT, CSV y binario por defecto.
- Proteccion basica contra path traversal rechazando rutas con `..` o `\`.
- Concurrencia con un thread por cliente.
- Logger thread-safe a stdout y archivo.

### PIBL

El PIBL se ejecuta como:

```bash
./pibl <HTTP_PORT> <ConfigFile> <LogFile> <CacheTTL>
```

Funciones implementadas:

- Listener HTTP en puerto 80 u 8080.
- Un thread por conexion entrante.
- Parser HTTP/1.1 antes de reenviar la peticion.
- Nuevo socket cliente por cada intento de backend.
- Reenvio completo de request y response.
- Balanceo Round Robin protegido por mutex.
- Failover: si un backend falla, intenta con el siguiente.
- Logger dual stdout + archivo.
- Cache persistente en disco para respuestas `GET 200 OK`.
- TTL configurable por CLI. Con `CacheTTL=0`, el cache queda deshabilitado.
- Header `Age` cuando una respuesta se sirve desde cache.

### Webapp

La carpeta `webapp/` es el `DocumentRootFolder` para los tres TWS:

- `index.html`: caso 1, hipertextos + una imagen.
- `gallery.html`: caso 2, multiples imagenes.
- `bigfile.html`: caso 3, archivo binario de aproximadamente 1 MB.
- `multifiles.html`: caso 4, multiples archivos que suman aproximadamente 1 MB.
- `form.html`: prueba adicional de metodo POST.

## Compilacion

Ejecutar en Linux o WSL:

```bash
cd ws
make clean
make

cd ../pibl
make clean
make
```

## Ejecucion Local

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

Terminal 4:

```bash
cd pibl
mkdir -p logs cache
./pibl 8080 pibl.conf logs/pibl.log 30
```

Abrir en navegador:

```text
http://127.0.0.1:8080/index.html
http://127.0.0.1:8080/gallery.html
http://127.0.0.1:8080/bigfile.html
http://127.0.0.1:8080/multifiles.html
http://127.0.0.1:8080/form.html
```

## Lista De Pruebas

1. GET normal:

```bash
curl -v http://127.0.0.1:8080/index.html
```

Esperado: `HTTP/1.1 200 OK`, `Content-Type: text/html`, `Content-Length`.

2. HEAD:

```bash
curl -I http://127.0.0.1:8080/index.html
```

Esperado: headers de `200 OK` sin body.

3. POST:

```bash
curl -v -X POST http://127.0.0.1:8080/submit \
  -H "Content-Type: application/x-www-form-urlencoded" \
  -d "nombre=Esteban&mensaje=Hola"
```

Esperado: `200 OK` y log de POST en PIBL y TWS.

4. 404:

```bash
curl -v http://127.0.0.1:8080/no-existe.html
```

Esperado: `HTTP/1.1 404 Not Found`.

5. 400 por peticion malformada:

```bash
printf 'BASURA\r\n\r\n' | nc 127.0.0.1 8080
```

Esperado: `HTTP/1.1 400 Bad Request`.

6. Archivo grande:

```bash
curl -o /tmp/archivo_grande.bin http://127.0.0.1:8080/files/archivo_grande.bin
stat -c%s /tmp/archivo_grande.bin
```

Esperado: `1048576`.

7. Cache MISS/HIT:

```bash
rm -f pibl/cache/index.html
curl -I http://127.0.0.1:8080/index.html
curl -I http://127.0.0.1:8080/index.html
```

Esperado: primera peticion registra `MISS` y crea `pibl/cache/index.html`; segunda registra `CACHE HIT` y devuelve header `Age`.

8. TTL expirado:

```bash
# Arranca el PIBL con TTL corto:
./pibl 8080 pibl.conf logs/pibl.log 2

curl -I http://127.0.0.1:8080/index.html
sleep 3
curl -I http://127.0.0.1:8080/index.html
```

Esperado: despues de `sleep 3`, el recurso expira y vuelve a backend.

9. Round Robin:

```bash
for i in 1 2 3 4 5 6; do
  curl -s -o /dev/null http://127.0.0.1:8080/gallery.html
done
tail -n 30 pibl/logs/pibl.log
```

Esperado: los logs muestran rotacion entre `127.0.0.1:8081`, `8082`, `8083`.

10. Failover:

```bash
# Detener uno de los TWS, por ejemplo el de 8082.
curl -v http://127.0.0.1:8080/index.html
tail -n 20 pibl/logs/pibl.log
```

Esperado: PIBL registra fallo del backend caido e intenta el siguiente.

11. Path traversal:

```bash
curl -v http://127.0.0.1:8080/../../../etc/passwd
```

Esperado: `400 Bad Request` o rechazo equivalente.

## Despliegue AWS

Usar cuatro instancias EC2 Ubuntu:

- EC2-PIBL: IP publica, puerto 8080 abierto a clientes.
- EC2-TWS1: IP privada, puerto 8080 abierto desde la VPC.
- EC2-TWS2: IP privada, puerto 8080 abierto desde la VPC.
- EC2-TWS3: IP privada, puerto 8080 abierto desde la VPC.

En cada TWS:

```bash
cd ws
make
mkdir -p logs
./tws 8080 logs/tws.log ../webapp
```

En PIBL editar `pibl/pibl.conf` con las IPs privadas reales:

```text
backend 10.x.x.x 8080
backend 10.x.x.y 8080
backend 10.x.x.z 8080
```

Luego:

```bash
cd pibl
make
mkdir -p logs cache
./pibl 8080 pibl.conf logs/pibl.log 300
```

## Conclusiones

La solucion separa responsabilidades entre TWS y PIBL: los TWS sirven recursos estaticos y procesan HTTP basico, mientras el PIBL concentra balanceo, failover, cache y logging de las transacciones. El uso de threads permite atender multiples clientes concurrentes y el cache persistente reduce solicitudes repetidas a los backends.

Los puntos criticos para una sustentacion son verificar compilacion en Linux, demostrar Round Robin con logs, demostrar cache HIT/MISS con `Age`, y probar los cuatro casos de la webapp desde un navegador externo en AWS.

## Referencias

- RFC 2616: Hypertext Transfer Protocol HTTP/1.1.
- Linux man pages: `socket`, `bind`, `listen`, `accept`, `connect`, `read`, `write`.
- POSIX Threads Programming: `pthread_create`, `pthread_detach`, `pthread_mutex`.
- Material del curso Telematica/Internet: Arquitectura y Protocolos.
