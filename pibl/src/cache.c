#define _GNU_SOURCE

#include "cache.h"

#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define CACHE_DIR "cache"

static int g_ttl_seconds = 0;
static pthread_mutex_t g_cache_mutex = PTHREAD_MUTEX_INITIALIZER;

static int uri_to_cache_path(const char *uri, char *out, size_t out_size);
static int ensure_parent_dirs(const char *path);
static int insert_age_header(char *response, size_t *resp_len,
                             size_t resp_max, time_t mtime);

int cache_init(int ttl_seconds) {
    g_ttl_seconds = ttl_seconds < 0 ? 0 : ttl_seconds;

    if (mkdir(CACHE_DIR, 0755) < 0 && errno != EEXIST) {
        return -1;
    }

    return 0;
}

int cache_lookup(const char *uri, char *response, size_t *resp_len,
                 size_t resp_max) {
    char path[PATH_MAX];
    FILE *fp;
    size_t n;

    if (g_ttl_seconds == 0 || !uri || !response || !resp_len ||
        resp_max == 0) {
        return 0;
    }

    if (uri_to_cache_path(uri, path, sizeof(path)) < 0) {
        return 0;
    }

    pthread_mutex_lock(&g_cache_mutex);

    if (!cache_is_valid(path)) {
        unlink(path);
        pthread_mutex_unlock(&g_cache_mutex);
        return 0;
    }

    fp = fopen(path, "rb");
    if (!fp) {
        pthread_mutex_unlock(&g_cache_mutex);
        return 0;
    }

    n = fread(response, 1, resp_max, fp);
    if (ferror(fp) || (!feof(fp) && n == resp_max)) {
        fclose(fp);
        pthread_mutex_unlock(&g_cache_mutex);
        return 0;
    }

    fclose(fp);
    *resp_len = n;
    {
        struct stat st;
        if (stat(path, &st) == 0) {
            insert_age_header(response, resp_len, resp_max, st.st_mtime);
        }
    }
    pthread_mutex_unlock(&g_cache_mutex);
    return 1;
}

int cache_store(const char *uri, const char *response, size_t resp_len) {
    char path[PATH_MAX];
    FILE *fp;
    size_t written;

    if (g_ttl_seconds == 0 || !uri || !response || resp_len == 0) {
        return 0;
    }

    if (uri_to_cache_path(uri, path, sizeof(path)) < 0) {
        return -1;
    }

    pthread_mutex_lock(&g_cache_mutex);

    if (ensure_parent_dirs(path) < 0) {
        pthread_mutex_unlock(&g_cache_mutex);
        return -1;
    }

    fp = fopen(path, "wb");
    if (!fp) {
        pthread_mutex_unlock(&g_cache_mutex);
        return -1;
    }

    written = fwrite(response, 1, resp_len, fp);
    fclose(fp);

    pthread_mutex_unlock(&g_cache_mutex);
    return written == resp_len ? 0 : -1;
}

int cache_is_valid(const char *cache_path) {
    struct stat st;
    time_t now;

    if (g_ttl_seconds == 0 || !cache_path) {
        return 0;
    }

    if (stat(cache_path, &st) < 0 || !S_ISREG(st.st_mode)) {
        return 0;
    }

    now = time(NULL);
    return (now - st.st_mtime) <= g_ttl_seconds;
}

static int uri_to_cache_path(const char *uri, char *out, size_t out_size) {
    const char *safe_uri = uri;
    int n;

    if (!uri || !out || out_size == 0 || uri[0] != '/' ||
        strstr(uri, "..") != NULL || strchr(uri, '\\') != NULL ||
        strchr(uri, '?') != NULL) {
        return -1;
    }

    if (strcmp(uri, "/") == 0) {
        safe_uri = "/index.html";
    }

    n = snprintf(out, out_size, "%s%s", CACHE_DIR, safe_uri);
    if (n < 0 || (size_t)n >= out_size) {
        return -1;
    }

    return 0;
}

static int ensure_parent_dirs(const char *path) {
    char tmp[PATH_MAX];
    char *p;

    if (!path || strlen(path) >= sizeof(tmp)) {
        return -1;
    }

    strcpy(tmp, path);
    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, 0755) < 0 && errno != EEXIST) {
                return -1;
            }
            *p = '/';
        }
    }

    return 0;
}

static int insert_age_header(char *response, size_t *resp_len,
                             size_t resp_max, time_t mtime) {
    char age_header[64];
    void *first_crlf;
    size_t insert_at;
    size_t age_len;
    time_t now = time(NULL);
    long age = (long)(now - mtime);

    if (!response || !resp_len || *resp_len == 0) {
        return -1;
    }

    if (age < 0) {
        age = 0;
    }

    age_len = (size_t)snprintf(age_header, sizeof(age_header),
                               "Age: %ld\r\n", age);
    if (age_len == 0 || age_len >= sizeof(age_header) ||
        *resp_len + age_len > resp_max) {
        return -1;
    }

    first_crlf = memmem(response, *resp_len, "\r\n", 2);
    if (!first_crlf) {
        return -1;
    }

    insert_at = (size_t)((char *)first_crlf - response) + 2;
    memmove(response + insert_at + age_len,
            response + insert_at,
            *resp_len - insert_at);
    memcpy(response + insert_at, age_header, age_len);
    *resp_len += age_len;
    return 0;
}
