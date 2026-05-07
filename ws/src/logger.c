#include "logger.h"

#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <time.h>

static FILE *g_log_file = NULL;
static pthread_mutex_t g_log_mutex = PTHREAD_MUTEX_INITIALIZER;
static int g_logger_ready = 0;

int logger_init(const char *log_file) {
    g_log_file = fopen(log_file, "a");
    if (!g_log_file) {
        return -1;
    }

    g_logger_ready = 1;
    return 0;
}

void logger_log(const char *format, ...) {
    time_t now;
    struct tm tm_info;
    char timestamp[32];
    va_list args;
    va_list copy;

    if (!g_logger_ready || !g_log_file) {
        return;
    }

    now = time(NULL);
    localtime_r(&now, &tm_info);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &tm_info);

    pthread_mutex_lock(&g_log_mutex);

    va_start(args, format);
    va_copy(copy, args);

    fprintf(stdout, "[%s] ", timestamp);
    vfprintf(stdout, format, args);
    fprintf(stdout, "\n");
    fflush(stdout);

    fprintf(g_log_file, "[%s] ", timestamp);
    vfprintf(g_log_file, format, copy);
    fprintf(g_log_file, "\n");
    fflush(g_log_file);

    va_end(copy);
    va_end(args);

    pthread_mutex_unlock(&g_log_mutex);
}

void logger_close(void) {
    pthread_mutex_lock(&g_log_mutex);
    if (g_log_file) {
        fflush(g_log_file);
        fclose(g_log_file);
        g_log_file = NULL;
    }
    g_logger_ready = 0;
    pthread_mutex_unlock(&g_log_mutex);
}
