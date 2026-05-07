# webapp/ — Aplicación web de prueba para TWS + PIBL

Esta carpeta contiene la **aplicación web replicada en los tres servidores TWS**.
Es la misma en los tres backend; el PIBL (Proxy Inverso + Balanceador de Carga)
balancea las peticiones entre ellos con Round Robin.

## Estructura

```
webapp/
├── index.html         Caso 1: hipertextos + 1 imagen
├── gallery.html       Caso 2: múltiples imágenes
├── bigfile.html       Caso 3: 1 archivo de ~1 MB
├── multifiles.html    Caso 4: múltiples archivos (~1 MB total)
├── form.html          Demo de POST y enlaces para probar 404
├── 404.html           Página 404 opcional para servir como cuerpo del error
├── css/
│   └── style.css      Estilos (text/css)
├── img/
│   ├── logo.png       (image/png)
│   ├── foto1.jpg      (image/jpeg)
│   ├── foto2.png      (image/png)
│   ├── foto3.jpg      (image/jpeg)
│   ├── foto4.jpg      (image/jpeg)
│   └── foto5.png      (image/png)
└── files/
    ├── archivo_grande.bin   ~1 MB (Caso 3)
    ├── file_part1.bin       ~350 KB (Caso 4)
    ├── file_part2.bin       ~340 KB (Caso 4)
    ├── file_part3.bin       ~340 KB (Caso 4)
    ├── sample.txt           text/plain
    └── datos.csv            text/csv
```

## Cobertura de los casos del PDF

| Caso | Recurso                  | Verifica                                   |
|------|--------------------------|--------------------------------------------|
| 1    | `index.html`             | HTML + CSS + 1 imagen + hipertextos        |
| 2    | `gallery.html`           | HTML + múltiples imágenes (jpg + png)      |
| 3    | `bigfile.html`           | Descarga de archivo binario de ~1 MB       |
| 4    | `multifiles.html`        | Múltiples archivos en paralelo (~1 MB)     |
| —    | `form.html`              | Método POST                                |
| —    | `/cualquier-no-existe`   | Respuesta 404 del TWS                      |

## Tipos MIME que ejercita

- `text/html`, `text/css`, `text/plain`, `text/csv`
- `image/jpeg`, `image/png`
- `application/octet-stream` (archivos `.bin`)

## Cómo lanzarla

Esta carpeta es el **DocumentRootFolder** del TWS. Se lanza así:

```bash
./server 8080 ./logs/tws.log /ruta/a/webapp
```

Y se accede a través del PIBL (puerto 8080 por defecto):

```
http://<IP_PIBL>:8080/index.html
http://<IP_PIBL>:8080/gallery.html
http://<IP_PIBL>:8080/bigfile.html
http://<IP_PIBL>:8080/multifiles.html
```

## Regenerar los activos binarios

Si en algún momento falta el contenido binario (por ejemplo al clonar en una
nueva máquina), se puede regenerar todo desde PowerShell:

```powershell
# imágenes (requiere Windows con System.Drawing)
# y archivos binarios aleatorios — ver scripts en docs/
```

> En Linux puedes regenerar los binarios con:
> ```bash
> dd if=/dev/urandom of=webapp/files/archivo_grande.bin bs=1M count=1
> dd if=/dev/urandom of=webapp/files/file_part1.bin bs=1024 count=350
> dd if=/dev/urandom of=webapp/files/file_part2.bin bs=1024 count=340
> dd if=/dev/urandom of=webapp/files/file_part3.bin bs=1024 count=340
> ```
