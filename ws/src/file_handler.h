/*
 * =============================================================================
 * TWS - file_handler.h - Interfaz para manejo de archivos del DocumentRoot
 * =============================================================================
 *
 * REQUISITOS:
 *   1. Declarar file_exists(doc_root, uri) → 1 si existe, 0 si no
 *   2. Declarar file_get_size(path) → tamaño del archivo en bytes
 *   3. Declarar file_get_content_type(path) → string del MIME type
 *   4. Declarar file_build_path(doc_root, uri, out_path, max_len)
 *      → construir ruta completa y segura
 *
 * NOTAS:
 *   - Si URI es "/" → resolver a "/index.html"
 *   - MIME types mínimos: text/html, text/css, image/jpeg, image/png,
 *     application/octet-stream (default)
 * =============================================================================
 */

#ifndef TWS_FILE_HANDLER_H
#define TWS_FILE_HANDLER_H

#include <stddef.h>

int file_exists(const char *doc_root, const char *uri);
size_t file_get_size(const char *full_path);
const char *file_get_content_type(const char *full_path);
int file_build_path(const char *doc_root, const char *uri,
                    char *out_path, size_t max_len);

#endif
