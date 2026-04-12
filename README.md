# Cluster-Web-Telematica

<!--
  =============================================================================
  README.md - Documentación principal del proyecto
  =============================================================================

  REQUISITOS QUE DEBE CUMPLIR ESTE ARCHIVO (según el PDF):
    1. Nombres y datos de los integrantes del grupo
    2. Introducción y descripción del proyecto
    3. Desarrollo del proyecto:
       - Arquitectura del sistema (PIBL + TWS + webapp)
       - Diseño e implementación del TWS (Web Server)
       - Diseño e implementación del PIBL (Proxy + Load Balancer)
       - Sistema de caché en disco con TTL configurable
       - Balanceo de carga con Round Robin
    4. Aspectos logrados y no logrados
    5. Conclusiones
    6. Referencias (RFC 2616, documentación de sockets, etc.)

  PASOS A TOMAR:
    - Paso 1: Llenar datos de integrantes al inicio
    - Paso 2: Escribir la introducción cuando el proyecto esté funcionando
    - Paso 3: Documentar decisiones de diseño con diagramas
    - Paso 4: Capturar screenshots de las pruebas
    - Paso 5: Completar conclusiones al final

  CLAVES PARA EL ÉXITO:
    - Este README es parte de la entrega evaluada
    - Incluir capturas de pantalla de las pruebas (curl, navegador)
    - Referenciar los diagramas de secuencia
    - Explicar cómo compilar y ejecutar (make, ./tws, ./pibl)
    - Documentar la infraestructura AWS (IPs, puertos, security groups)
  =============================================================================
-->

## Integrantes
- Nombre 1 - Código
- Nombre 2 - Código

## Compilación y Ejecución

### TWS (Web Server)
```bash
cd ws
make
./tws 8080 logs/tws.log ../webapp
```

### PIBL (Proxy + Load Balancer)
```bash
cd pibl
make
./pibl 80 pibl.conf logs/pibl.log 300
```

## Estructura del Proyecto
```
prueba-cluster/
├── README.md
├── .gitignore
├── pibl/                  # Proxy Inverso + Balanceador de Carga
│   ├── Makefile
│   ├── pibl.conf          # Configuración de backends
│   ├── src/               # Código fuente del PIBL
│   ├── cache/             # Caché en disco (runtime)
│   └── logs/              # Logs del PIBL (runtime)
├── ws/                    # Telematics Web Server (TWS)
│   ├── Makefile
│   ├── src/               # Código fuente del TWS
│   └── logs/              # Logs del TWS (runtime)
└── webapp/                # Contenido web (DocumentRootFolder)
    ├── index.html
    ├── gallery.html
    ├── bigfile.html
    ├── multifiles.html
    ├── css/style.css
    ├── img/               # Imágenes de prueba
    └── files/             # Archivos grandes de prueba
```
