/*
 * =============================================================================
 * TWS - file_handler.c - Acceso al sistema de archivos para servir recursos
 * =============================================================================
 *
 * REQUISITOS QUE DEBE CUMPLIR ESTE ARCHIVO:
 *   1. file_build_path(): Construir ruta segura "doc_root + uri"
 *      - Si uri == "/" → cambiar a "/index.html"
 *      - Sanitizar: no permitir ".." ni "/" al inicio del uri
 *      - Resultado en out_path (buffer preasignado)
 *   2. file_exists(): Usar stat() para verificar si el archivo existe
 *      y es un archivo regular (no directorio)
 *   3. file_get_size(): Usar stat() para obtener st_size
 *   4. file_get_content_type(): Determinar MIME type según extensión:
 *      - .html → "text/html"
 *      - .css  → "text/css"
 *      - .js   → "application/javascript"
 *      - .jpg/.jpeg → "image/jpeg"
 *      - .png  → "image/png"
 *      - .gif  → "image/gif"
 *      - .ico  → "image/x-icon"
 *      - .txt  → "text/plain"
 *      - default → "application/octet-stream"
 *
 * PASOS A TOMAR:
 *   - Paso 1: Implementar file_build_path con snprintf(out, max, "%s%s", root, uri)
 *   - Paso 2: Implementar file_exists con stat() + S_ISREG()
 *   - Paso 3: Implementar file_get_size con stat()
 *   - Paso 4: Implementar file_get_content_type con strrchr() para buscar extensión
 *
 * CLAVES PARA EL ÉXITO:
 *   - SEGURIDAD: Sanitizar URI para evitar path traversal (../../etc/passwd)
 *     Rechazar URIs que contengan ".." → retornar -1
 *   - Usar realpath() para resolver la ruta absoluta y verificar que
 *     siga dentro del DocumentRootFolder
 *   - stat() retorna -1 si el archivo no existe → manejar correctamente
 *   - Para archivos grandes (>1MB), se leen en bloques desde http_response
 *     → file_handler solo provee metadata, no carga archivos en memoria
 * =============================================================================
 */

#include "file_handler.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <limits.h>
