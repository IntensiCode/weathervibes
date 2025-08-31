#ifndef LOGGER_H
#define LOGGER_H

#include <glib.h>

// Initialize logging system - call once at startup
void logger_init(void);

// Cleanup logging system - call at shutdown
void logger_cleanup(void);

// Logging functions
void log_debug(const char *format, ...) G_GNUC_PRINTF(1, 2);
void log_info(const char *format, ...) G_GNUC_PRINTF(1, 2);
void log_warn(const char *format, ...) G_GNUC_PRINTF(1, 2);
void log_error(const char *format, ...) G_GNUC_PRINTF(1, 2);

#endif