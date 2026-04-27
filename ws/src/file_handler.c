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

/* Autor: Camila Martinez
 * Función: Dar información sobre los archivos en disco.
 */
 
#include "file_handler.h"

#include <stdio.h> /* Strings en formato, evita desbordamientos de buffer */
#include <string.h> /* Manejo de strings */
#include <sys/stat.h> /* stat() para verificar existencia y obtener tamaño */
#include <limits.h> /* PATH_MAX para tamaño máximo de rutas */

int file_build_path(const char *doc_root, const char *uri, char *out_path, size_t max_len) {
    if (strcmp(uri, "/") == 0)
        uri = "/index.html";

    if (strstr(uri, "..") !=NULL)
        return -1;

    snprintf(out_path, max_len, "%s%s", doc_root, uri);
    return 0;
}

int file_exists(const char *doc_root, const char*uri) {
    char full_path[PATH_MAX];

    if (file_build_path(doc_root, uri, full_path, sizeof(full_path)) != 0)
        return 0;
    
    struct stat st;

    if (stat(full_path, &st) == -1)
        return 0;
    
    return S_ISREG(st.st_mode);
}

size_t file_get_size(const char *full_path) {
    struct stat st;

    if (stat(full_path, &st) == -1)
        return 0;

    return (size_t)st.st_size;
}

const char *file_get_content_type(const char *full_path) {
    const char *ext = strrchr(full_path, '.');

    if (!ext)
        return "application/octet-stream";

    if (strcmp(ext, ".html") == 0)
        return "text/html";
    
    if (strcmp(ext, ".css") == 0)
        return "text/css";

    if (strcmp(ext, ".js") == 0)
        return "application/javascript";
    
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0)
        return "image/jpeg";

    if (strcmp(ext, ".png") == 0)
        return "image/png";

    if (strcmp(ext, ".gif") == 0)
        return "image/gif";
    
    if (strcmp(ext, ".ico") == 0)
        return "image/x-icon";
    
    if (strcmp(ext, ".txt") == 0)
        return "text/plain";

    return "application/octet-stream";
}