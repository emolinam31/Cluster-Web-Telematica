#include "file_handler.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

int file_build_path(const char *doc_root, const char *uri,
                    char *out_path, size_t max_len) {
    const char *safe_uri;
    int n;

    if (!doc_root || !uri || !out_path || max_len == 0) {
        return -1;
    }

    if (uri[0] != '/' || strstr(uri, "..") != NULL ||
        strchr(uri, '\\') != NULL) {
        return -1;
    }

    safe_uri = (strcmp(uri, "/") == 0) ? "/index.html" : uri;
    n = snprintf(out_path, max_len, "%s%s", doc_root, safe_uri);
    if (n < 0 || (size_t)n >= max_len) {
        return -1;
    }

    return 0;
}

int file_exists(const char *doc_root, const char *uri) {
    char path[PATH_MAX];
    struct stat st;

    if (file_build_path(doc_root, uri, path, sizeof(path)) < 0) {
        return 0;
    }

    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

size_t file_get_size(const char *full_path) {
    struct stat st;

    if (!full_path || stat(full_path, &st) != 0 || !S_ISREG(st.st_mode)) {
        return 0;
    }

    return (size_t)st.st_size;
}

const char *file_get_content_type(const char *full_path) {
    const char *ext;

    if (!full_path) {
        return "application/octet-stream";
    }

    ext = strrchr(full_path, '.');
    if (!ext) {
        return "application/octet-stream";
    }

    if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0) {
        return "text/html";
    }
    if (strcmp(ext, ".css") == 0) {
        return "text/css";
    }
    if (strcmp(ext, ".js") == 0) {
        return "application/javascript";
    }
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) {
        return "image/jpeg";
    }
    if (strcmp(ext, ".png") == 0) {
        return "image/png";
    }
    if (strcmp(ext, ".gif") == 0) {
        return "image/gif";
    }
    if (strcmp(ext, ".ico") == 0) {
        return "image/x-icon";
    }
    if (strcmp(ext, ".txt") == 0) {
        return "text/plain";
    }
    if (strcmp(ext, ".csv") == 0) {
        return "text/csv";
    }

    return "application/octet-stream";
}
