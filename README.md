# Cluster-Web-Telematica

Proyecto 1 de Telematica: implementacion de un cluster web compuesto por un
Proxy Inverso + Balanceador de Carga (PIBL) y tres servidores web TWS.

Este repositorio contiene el codigo fuente, la aplicacion web de prueba, la
configuracion de backends y las instrucciones necesarias para compilar, ejecutar,
probar y desplegar el cluster.

## Integrantes

1. Felipe Ochoa Lotero
2. Camila Martinez Montoya
3. Esteban Molina Mejia

> Pendiente: agregar los codigos reales de los integrantes.

## Alcance Del Proyecto

Este proyecto implementa una arquitectura HTTP distribuida donde el cliente
entra por un unico punto, el PIBL, y este se encarga de repartir las solicitudes
hacia tres servidores TWS. La idea principal es separar responsabilidades: los
TWS sirven contenido web y el PIBL maneja el acceso externo, el balanceo, el
cache, el failover y el registro de actividad.

Funcionalidades cubiertas:

| Componente | Funcionalidad                                                                                |
|------------|----------------------------------------------------------------------------------------------|
| TWS        | Servidor HTTP/1.1 escrito en C usando sockets.                                               |
| TWS        | Soporte de metodos `GET`, `HEAD` y `POST`.                                                   |
| TWS        | Respuestas `200 OK`, `400 Bad Request` y `404 Not Found`.                                    |
| TWS        | Servicio de archivos desde `DocumentRootFolder`.                                             |
| TWS        | Concurrencia mediante threads.                                                               |
| TWS        | Registro de peticiones y respuestas en stdout y archivo.                                     |
| PIBL       | Proxy inverso HTTP/1.1 escrito en C usando sockets.                                          |
| PIBL       | Escucha en puerto `80` o `8080`.                                                             |
| PIBL       | Crea un socket cliente nuevo hacia el backend seleccionado.                                  |
| PIBL       | Reenvia la respuesta del backend al cliente.                                                 |
| PIBL       | Maneja multiples clientes con threads.                                                       |
| PIBL       | Registra peticiones y respuestas en stdout y archivo.                                        |
| PIBL       | Implementa cache persistente en disco.                                                       |
| PIBL       | Usa TTL configurable por linea de comandos.                                                  |
| PIBL       | Balancea carga con Round Robin.                                                              |
| PIBL       | Lee backends desde archivo de configuracion.                                                 |
| Webapp     | Incluye cuatro casos de prueba: pagina basica, galeria, archivo grande y multiples archivos. |
| Despliegue | Despliegue en cuatro instancias EC2: una para PIBL y tres para TWS.                          |

Datos que todavia se deben completar antes de cerrar la entrega:

- Codigos reales de los integrantes.
- IP publica final del PIBL desplegado en AWS.
- Capturas de pantalla o evidencias graficas de las pruebas.

## Introduccion

El objetivo del proyecto es construir un web cluster basado en una arquitectura
cliente-servidor capaz de atender solicitudes HTTP a traves de un punto de
entrada unico. El cliente no se conecta directamente a los servidores web, sino
al PIBL. El PIBL recibe la peticion, selecciona un TWS mediante Round Robin,
reenvia la solicitud, recibe la respuesta y la retorna al cliente.

La solucion esta implementada en C con sockets POSIX y `pthreads`. El TWS se
encarga de servir recursos estaticos y responder solicitudes HTTP basicas. El
PIBL concentra las responsabilidades de proxy inverso, balanceo de carga,
failover, cache persistente en disco y logging.

## Desarrollo

### Arquitectura General

La arquitectura logica que usamos es:

```text
Cliente HTTP
    |
    | HTTP/1.1
    v
PIBL - Proxy Inverso + Balanceador de Carga
    |
    | Socket TCP hacia backend seleccionado
    +-- TWS 1 - DocumentRoot webapp/
    +-- TWS 2 - DocumentRoot webapp/
    +-- TWS 3 - DocumentRoot webapp/
```

Para el despliegue en AWS trabajamos con cuatro instancias EC2:

| Instancia | Rol                         | Red                                              |
|-----------|-----------------------------|--------------------------------------------------|
| EC2 PIBL  | Proxy inverso y balanceador | IP publica, puerto `8080` expuesto a clientes ,,,|
| EC2 TWS 1 | Servidor web backend        | IP privada, puerto `8080` accesible desde la VPC |
| EC2 TWS 2 | Servidor web backend        | IP privada, puerto `8080` accesible desde la VPC |
| EC2 TWS 3 | Servidor web backend        | IP privada, puerto `8080` accesible desde la VPC |

El archivo actual `pibl/pibl.conf` contiene estos backends privados:

```text
backend 10.0.1.196 8080
backend 10.0.1.171 8080
backend 10.0.1.190 8080
```

Para pruebas locales se puede usar la misma arquitectura en una sola maquina,
cambiando `pibl/pibl.conf` temporalmente a:

```text
backend 127.0.0.1 8081
backend 127.0.0.1 8082
backend 127.0.0.1 8083
```

### TWS

El TWS esta en la carpeta `ws/`. Su punto de entrada es `ws/src/main.c` y se
compila en un ejecutable llamado `tws`.

Forma de ejecucion:

```bash
./tws <HTTP_PORT> <LogFile> <DocumentRootFolder>
```

Ejemplo:

```bash
./tws 8080 logs/tws.log ../webapp
```

Lo que hace el TWS:

- Valida argumentos de entrada.
- Valida que el puerto este entre `1` y `65535`.
- Valida que `DocumentRootFolder` exista y sea un directorio.
- Crea el socket TCP con `getaddrinfo`, `socket`, `bind` y `listen`.
- Acepta clientes con `accept`.
- Maneja cada conexion en un thread independiente.
- Parsea request-line, version HTTP, header `Host` y `Content-Length`.
- Soporta los metodos `GET`, `HEAD` y `POST`.
- Responde `200 OK` cuando el recurso existe.
- Responde `400 Bad Request` cuando la peticion no es valida.
- Responde `404 Not Found` cuando el recurso no existe.
- Sirve archivos desde el `DocumentRootFolder`.
- Rechaza rutas con `..` o `\` para evitar path traversal.
- Detecta tipos MIME para HTML, CSS, JS, JPEG, PNG, GIF, ICO, TXT, CSV y binario por defecto.
- Registra actividad en stdout y archivo usando un logger thread-safe.

El metodo `HEAD` retorna los mismos headers de un `GET`, pero sin cuerpo. Para
`POST /submit`, el servidor responde una pagina HTML simple confirmando la
recepcion.

### PIBL

El PIBL esta en la carpeta `pibl/`. Su punto de entrada es `pibl/src/main.c` y
se compila en un ejecutable llamado `pibl`.

Forma de ejecucion:

```bash
./pibl <HTTP_PORT> <ConfigFile> <LogFile> <CacheTTL>
```

Ejemplo:

```bash
./pibl 8080 pibl.conf logs/pibl.log 300
```

El puerto del PIBL esta restringido a `80` o `8080`.

Lo que hace el PIBL:

- Valida argumentos de entrada.
- Lee los backends desde `pibl.conf`.
- Inicializa el balanceador Round Robin.
- Inicializa el cache con TTL configurable.
- Crea el socket listener para clientes HTTP.
- Maneja cada cliente en un thread independiente.
- Parsea la solicitud HTTP/1.1 antes de reenviarla.
- Reenvia la peticion al backend seleccionado mediante un socket nuevo.
- Reenvia la respuesta del backend hacia el cliente.
- Agrega el header `Via: PIBL/1.0`.
- Hace failover basico: si un backend falla, intenta con el siguiente.
- Registra actividad en stdout y archivo usando un logger thread-safe.

### Balanceo Round Robin

El balanceador se implementa en `pibl/src/balancer.c`. Mantiene una lista de
backends cargada desde `pibl.conf` y un indice compartido protegido con
`pthread_mutex_t`.

La seleccion sigue esta rotacion:

```text
TWS 1 -> TWS 2 -> TWS 3 -> TWS 1 -> ...
```

Si un backend no responde, el PIBL registra el fallo y prueba el siguiente
backend disponible hasta agotar la cantidad configurada.

### Cache En Disco

El cache del PIBL se implementa en `pibl/src/cache.c` y usa el directorio
`pibl/cache/` relativo al lugar desde donde se ejecuta el proceso.

Comportamiento del cache:

- Solo se consulta y almacena cache para solicitudes `GET`.
- Solo se almacenan respuestas `HTTP/1.1 200 OK`.
- El archivo cacheado contiene la respuesta HTTP completa: headers y body.
- El TTL se recibe por CLI en segundos.
- Con `CacheTTL=0`, el cache queda deshabilitado.
- Si una entrada supera el TTL, se elimina y se vuelve a consultar al backend.
- Cuando hay cache HIT, el PIBL responde desde disco y agrega el header `Age`.
- El acceso al cache esta protegido con mutex para evitar escrituras concurrentes inconsistentes.

Ejemplo:

```bash
./pibl 8080 pibl.conf logs/pibl.log 30
```

En ese caso, una respuesta cacheada es valida durante 30 segundos.

### Webapp De Pruebas

La carpeta `webapp/` se usa como `DocumentRootFolder` de los tres TWS.

| Recurso          | Caso que verifica                                       |
|------------------|---------------------------------------------------------|
| `index.html`     | Pagina base con hipertextos, estilos y una imagen.      |
| `gallery.html`   | Carga de multiples imagenes.                            |
| `bigfile.html`   | Descarga de archivo binario cercano a 1 MB.             |
| `multifiles.html`| Carga de multiples archivos que en conjunto rondan 1 MB.|
| `form.html`      | Prueba adicional de metodo `POST`.                      |
| `404.html`       | Recurso HTML usado como pagina de apoyo para errores.   |

Archivos binarios disponibles:

```text
webapp/files/archivo_grande.bin
webapp/files/file_part1.bin
webapp/files/file_part2.bin
webapp/files/file_part3.bin
```

## Compilacion

El proyecto se compila en Linux/Ubuntu. Desde la raiz del repositorio:

```bash
cd ws
make clean
make

cd ../pibl
make clean
make
```

Resultado esperado:

- `ws/tws`
- `pibl/pibl`

Los Makefiles usan `gcc` con `-Wall -Wextra -pthread`.

## Ejecucion Local

Para probar todo en una sola maquina, primero se ajusta `pibl/pibl.conf` con
los backends locales `127.0.0.1:8081`, `127.0.0.1:8082` y `127.0.0.1:8083`.

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

Rutas para abrir desde navegador:

```text
http://127.0.0.1:8080/index.html
http://127.0.0.1:8080/gallery.html
http://127.0.0.1:8080/bigfile.html
http://127.0.0.1:8080/multifiles.html
http://127.0.0.1:8080/form.html
```

## Pruebas Funcionales

### GET

```bash
curl -v http://127.0.0.1:8080/index.html
```

Resultado esperado: `HTTP/1.1 200 OK`, `Content-Type: text/html` y
`Content-Length`.

### HEAD

```bash
curl -I http://127.0.0.1:8080/index.html
```

Resultado esperado: headers de `200 OK` sin body.

### POST

```bash
curl -v -X POST http://127.0.0.1:8080/submit \
  -H "Content-Type: application/x-www-form-urlencoded" \
  -d "nombre=Esteban&mensaje=Hola"
```

Resultado esperado: `HTTP/1.1 200 OK` y registro del `POST` en logs de PIBL y
TWS.

### 404

```bash
curl -v http://127.0.0.1:8080/no-existe.html
```

Resultado esperado: `HTTP/1.1 404 Not Found`.

### 400

```bash
printf 'BASURA\r\n\r\n' | nc 127.0.0.1 8080
```

Resultado esperado: `HTTP/1.1 400 Bad Request`.

### Archivo Grande

```bash
curl -o /tmp/archivo_grande.bin http://127.0.0.1:8080/files/archivo_grande.bin
stat -c%s /tmp/archivo_grande.bin
```

Resultado esperado: descarga completa del archivo. En este repositorio el
archivo `webapp/files/archivo_grande.bin` esta creado para el caso de prueba de
aproximadamente 1 MB.

### Cache HIT Y MISS

```bash
rm -f pibl/cache/index.html
curl -I http://127.0.0.1:8080/index.html
curl -I http://127.0.0.1:8080/index.html
tail -n 20 pibl/logs/pibl.log
```

Resultado esperado:

- Primera peticion: MISS y almacenamiento en `pibl/cache/`.
- Segunda peticion: `CACHE HIT`.
- Header `Age` en la respuesta servida desde cache.

### TTL Expirado

Arrancar el PIBL con TTL corto:

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

Resultado esperado: despues de esperar mas que el TTL, el PIBL invalida el
cache y consulta nuevamente a un backend.

### Round Robin

```bash
for i in 1 2 3 4 5 6; do
  curl -s -o /dev/null http://127.0.0.1:8080/gallery.html
done
tail -n 30 pibl/logs/pibl.log
```

Resultado esperado: los logs muestran rotacion entre los tres backends
configurados.

### Failover

Detener uno de los TWS y ejecutar:

```bash
curl -v http://127.0.0.1:8080/index.html
tail -n 20 pibl/logs/pibl.log
```

Resultado esperado: el PIBL registra el fallo del backend caido e intenta con
otro backend.

### Path Traversal

```bash
curl -v http://127.0.0.1:8080/../../../etc/passwd
```

Resultado esperado: rechazo de la solicitud con `400 Bad Request` o respuesta
equivalente de error.

## Despliegue En AWS EC2

El despliegue se trabaja con cuatro instancias EC2 Ubuntu dentro de la misma
VPC: una instancia para el PIBL y tres instancias para los TWS.

### Reglas De Red

- La instancia PIBL expone el puerto `8080` a los clientes externos.
- Las instancias TWS aceptan el puerto `8080` desde la VPC o desde el Security
  Group del PIBL.
- El PIBL se conecta a los TWS usando las IPs privadas.

### En Cada TWS

```bash
cd ws
make clean
make
mkdir -p logs
./tws 8080 logs/tws.log ../webapp
```

### En PIBL

El archivo `pibl/pibl.conf` debe quedar con las IPs privadas reales de los TWS:

```text
backend 10.0.1.196 8080
backend 10.0.1.171 8080
backend 10.0.1.190 8080
```

Luego se ejecuta el PIBL:

```bash
cd pibl
make clean
make
mkdir -p logs cache
./pibl 8080 pibl.conf logs/pibl.log 300
```

El acceso externo se hace contra la IP publica de la instancia PIBL:

```text
http://<IP_PUBLICA_PIBL>:8080/index.html
http://<IP_PUBLICA_PIBL>:8080/gallery.html
http://<IP_PUBLICA_PIBL>:8080/bigfile.html
http://<IP_PUBLICA_PIBL>:8080/multifiles.html
```

## Diagramas De Secuencia

Estos son los flujos principales que usamos para explicar el funcionamiento del
cluster:

| Diagrama | Flujo | Imagen |
|----------|-------|--------|
| DS-01 | `GET` normal con cache MISS y selección Round Robin. | ![DS-01](https://github.com/user-attachments/assets/54d240a1-8b4e-4e19-aa3f-5456e0a34f7b) |
| DS-02 | Cache HIT servido directamente desde disco. | ![DS-02](https://github.com/user-attachments/assets/f615396f-3886-4137-bd3a-5ae0fb170b97) |
| DS-03 | Cache expirado por TTL y nueva consulta al backend. | ![DS-03](https://github.com/user-attachments/assets/d39d3a54-9dfd-4124-a0de-c402773d500d) |
| DS-04 | Petición `HEAD` sin cuerpo de respuesta. | ![DS-04](https://github.com/user-attachments/assets/2520d823-1e58-4b37-86f5-694e4e45bb1d) |
| DS-05 | Petición `POST` reenviada al TWS. | ![DS-05](https://github.com/user-attachments/assets/e872343e-223d-43d2-957d-ec4ddab39aa7) |
| DS-06 | Manejo de errores HTTP `400` y `404`. | ![DS-06](https://github.com/user-attachments/assets/21b40aeb-b5f1-42e9-be7c-95fbf1fa24c1) |
| DS-07 | Arquitectura general de cliente, PIBL y tres TWS. | ![DS-07](https://github.com/user-attachments/assets/a57eff08-881f-4f4e-8a6a-c5e23222ea7d) |

## Evidencias

| Componente | Evidencia |
|---|---|
| PIBL ejecutándose | <img src="https://github.com/user-attachments/assets/fd79ac94-58df-4d85-bb6d-bda59c8017eb" width="500"/> |
| TWS ejecutándose | <img src="https://github.com/user-attachments/assets/fd79ac94-58df-4d85-bb6d-bda59c8017eb" width="500"/> |
| EC2 PIBL | <img src="https://github.com/user-attachments/assets/7a165917-77df-47c5-bd7c-5ade1e956db6" width="700"/> |
| EC2 TWS | <img src="https://github.com/user-attachments/assets/f6464fb6-c134-4173-a5f1-97c602c8ccb3" width="700"/> |
| VPC | <img src="https://github.com/user-attachments/assets/2b0903fb-fcda-4519-94a8-ccac45c8935a" width="700"/> |
| Security Group PIBL | <img src="https://github.com/user-attachments/assets/1081eed1-5392-46ea-9548-c948495beea7" width="700"/> |
| Security Group WS | <img src="https://github.com/user-attachments/assets/c82a7665-a2d1-4c24-8bf4-d200d79669e7" width="700"/> |
| Subnet | <img src="https://github.com/user-attachments/assets/cb94fc18-a063-4da8-82dd-189bddd003d4" width="700"/> |
| Web App | <img src="https://github.com/user-attachments/assets/a91f1e99-f917-4792-93ea-7c4a77d37cb7" width="700"/> |
| Prueba final | <img src="https://github.com/user-attachments/assets/d8b8a313-ae0f-455d-9c01-301fc5980414" width="500"/> |


## Conclusiones

La solucion separa responsabilidades de forma clara. Los TWS atienden el
contenido HTTP desde el `DocumentRootFolder`, mientras que el PIBL centraliza el
acceso externo, decide el backend, maneja fallos, registra actividad y reduce
consultas repetidas mediante cache en disco.

El uso de threads permite atender multiples conexiones de forma concurrente en
ambos componentes. El mutex en el balanceador evita condiciones de carrera al
rotar los backends y el mutex del cache protege las operaciones de lectura y
escritura sobre archivos compartidos.

Los puntos mas importantes para validar en sustentacion son la compilacion en
Linux/Ubuntu, la distribucion Round Robin entre los tres TWS, el comportamiento
MISS/HIT del cache, la expiracion por TTL y el acceso externo al PIBL desplegado
en AWS.

## Referencias

- Fielding, R. et al. RFC 2616: Hypertext Transfer Protocol -- HTTP/1.1.
- Linux man pages: `socket`, `bind`, `listen`, `accept`, `connect`, `read`,
  `write`, `getaddrinfo`.
- POSIX Threads: `pthread_create`, `pthread_detach`, `pthread_mutex_t`.
- Beej's Guide to Network Programming: uso de sockets TCP en C.
- Guia del proyecto: `resources/PDF-ProyectoN1-PILB-WS-v1.0 (1).pdf`.
- Material del curso de Telematica e Internet.
