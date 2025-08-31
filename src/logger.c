#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>

static FILE *log_file = NULL;
static char *log_file_path = NULL;

void logger_init(void) {
    if (log_file) {
        return; // Already initialized
    }
    
    // Create simple log file path
    log_file_path = g_strdup("/tmp/weather.log");
    
    // Open log file in append mode
    log_file = fopen(log_file_path, "a");
    if (!log_file) {
        g_warning("Failed to open log file %s", log_file_path);
        return;
    }
    
    // Write initial log entry
    pid_t pid = getpid();
    log_info("Weather applet logging started (PID: %d)", pid);
}

void logger_cleanup(void) {
    if (log_file) {
        log_info("Weather applet logging stopped");
        fclose(log_file);
        log_file = NULL;
    }
    
    if (log_file_path) {
        g_free(log_file_path);
        log_file_path = NULL;
    }
}

static void log_message(GLogLevelFlags level, const char *level_str, const char *format, va_list args) {
    // Get current time
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm);
    
    // Format the message
    char *message = g_strdup_vprintf(format, args);
    
    // Write to GLib logging
    g_log(G_LOG_DOMAIN, level, "%s", message);
    
    // Write to our log file
    if (log_file) {
        fprintf(log_file, "[%s] %s: %s\n", timestamp, level_str, message);
        fflush(log_file); // Ensure immediate write
    }
    
    g_free(message);
}

void log_debug(const char *format, ...) {
    va_list args;
    va_start(args, format);
    log_message(G_LOG_LEVEL_DEBUG, "DEBUG", format, args);
    va_end(args);
}

void log_info(const char *format, ...) {
    va_list args;
    va_start(args, format);
    log_message(G_LOG_LEVEL_INFO, "INFO", format, args);
    va_end(args);
}

void log_warn(const char *format, ...) {
    va_list args;
    va_start(args, format);
    log_message(G_LOG_LEVEL_WARNING, "WARN", format, args);
    va_end(args);
}

void log_error(const char *format, ...) {
    va_list args;
    va_start(args, format);
    log_message(G_LOG_LEVEL_CRITICAL, "ERROR", format, args);
    va_end(args);
}