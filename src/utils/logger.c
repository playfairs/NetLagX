#include "logger.h"
#include "time.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <time.h>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

static logger_context_t logger_ctx = {0};

static const char* color_codes[] = {
    "\033[0;36m",  // DEBUG - Cyan
    "\033[0;32m",  // INFO - Green
    "\033[0;33m",  // WARNING - Yellow
    "\033[0;31m",  // ERROR - Red
    "\033[1;31m"   // CRITICAL - Bold Red
};

static const char* color_reset = "\033[0m";

int logger_init(log_level_t min_level, log_output_t output_mask) {
    if (logger_ctx.initialized) {
        return LOGGER_SUCCESS;
    }
    
    memset(&logger_ctx, 0, sizeof(logger_context_t));
    
    logger_ctx.min_level = min_level;
    logger_ctx.output_mask = output_mask;
    logger_ctx.console_colors = 1;
    logger_ctx.timestamps = 1;
    logger_ctx.thread_ids = 0;
    logger_ctx.max_file_size = 10 * 1024 * 1024;
    logger_ctx.rotation_enabled = 1;
    
    if (pthread_mutex_init(&logger_ctx.mutex, NULL) != 0) {
        return LOGGER_ERROR_INIT;
    }
    
    if (output_mask & LOG_OUTPUT_FILE) {
        if (logger_ctx.log_file_path[0] == '\0') {
            strcpy(logger_ctx.log_file_path, "netlagx.log");
        }
        
        logger_ctx.log_file = fopen(logger_ctx.log_file_path, "a");
        if (!logger_ctx.log_file) {
            logger_ctx.output_mask &= ~LOG_OUTPUT_FILE;
        }
    }
    
    logger_ctx.initialized = 1;
    
    logger_info("Logger initialized (level: %d, output: %d)", min_level, output_mask);
    return LOGGER_SUCCESS;
}

void logger_cleanup(void) {
    if (!logger_ctx.initialized) {
        return;
    }
    
    if (logger_ctx.log_file) {
        fclose(logger_ctx.log_file);
        logger_ctx.log_file = NULL;
    }
    
    pthread_mutex_destroy(&logger_ctx.mutex);
    memset(&logger_ctx, 0, sizeof(logger_context_t));
}

int logger_set_level(log_level_t level) {
    if (!logger_ctx.initialized) {
        return LOGGER_ERROR_INIT;
    }
    
    pthread_mutex_lock(&logger_ctx.mutex);
    logger_ctx.min_level = level;
    pthread_mutex_unlock(&logger_ctx.mutex);
    
    return LOGGER_SUCCESS;
}

int logger_set_output(log_output_t output_mask) {
    if (!logger_ctx.initialized) {
        return LOGGER_ERROR_INIT;
    }
    
    pthread_mutex_lock(&logger_ctx.mutex);
    
    if ((output_mask & LOG_OUTPUT_FILE) && !logger_ctx.log_file) {
        if (logger_ctx.log_file_path[0] == '\0') {
            strcpy(logger_ctx.log_file_path, "netlagx.log");
        }
        
        logger_ctx.log_file = fopen(logger_ctx.log_file_path, "a");
        if (!logger_ctx.log_file) {
            output_mask &= ~LOG_OUTPUT_FILE;
        }
    }
    
    logger_ctx.output_mask = output_mask;
    pthread_mutex_unlock(&logger_ctx.mutex);
    
    return LOGGER_SUCCESS;
}

int logger_set_log_file(const char* file_path) {
    if (!logger_ctx.initialized || !file_path) {
        return LOGGER_ERROR_CONFIG;
    }
    
    pthread_mutex_lock(&logger_ctx.mutex);
    
    if (logger_ctx.log_file) {
        fclose(logger_ctx.log_file);
        logger_ctx.log_file = NULL;
    }
    
    strncpy(logger_ctx.log_file_path, file_path, sizeof(logger_ctx.log_file_path) - 1);
    logger_ctx.log_file_path[sizeof(logger_ctx.log_file_path) - 1] = '\0';
    
    if (logger_ctx.output_mask & LOG_OUTPUT_FILE) {
        logger_ctx.log_file = fopen(logger_ctx.log_file_path, "a");
        if (!logger_ctx.log_file) {
            pthread_mutex_unlock(&logger_ctx.mutex);
            return LOGGER_ERROR_FILE;
        }
    }
    
    pthread_mutex_unlock(&logger_ctx.mutex);
    
    return LOGGER_SUCCESS;
}

int logger_set_console_colors(int enabled) {
    if (!logger_ctx.initialized) {
        return LOGGER_ERROR_INIT;
    }
    
    pthread_mutex_lock(&logger_ctx.mutex);
    logger_ctx.console_colors = enabled;
    pthread_mutex_unlock(&logger_ctx.mutex);
    
    return LOGGER_SUCCESS;
}

int logger_set_timestamps(int enabled) {
    if (!logger_ctx.initialized) {
        return LOGGER_ERROR_INIT;
    }
    
    pthread_mutex_lock(&logger_ctx.mutex);
    logger_ctx.timestamps = enabled;
    pthread_mutex_unlock(&logger_ctx.mutex);
    
    return LOGGER_SUCCESS;
}

int logger_set_thread_ids(int enabled) {
    if (!logger_ctx.initialized) {
        return LOGGER_ERROR_INIT;
    }
    
    pthread_mutex_lock(&logger_ctx.mutex);
    logger_ctx.thread_ids = enabled;
    pthread_mutex_unlock(&logger_ctx.mutex);
    
    return LOGGER_SUCCESS;
}

int logger_set_max_file_size(size_t max_size) {
    if (!logger_ctx.initialized) {
        return LOGGER_ERROR_INIT;
    }
    
    pthread_mutex_lock(&logger_ctx.mutex);
    logger_ctx.max_file_size = max_size;
    pthread_mutex_unlock(&logger_ctx.mutex);
    
    return LOGGER_SUCCESS;
}

int logger_set_rotation(int enabled) {
    if (!logger_ctx.initialized) {
        return LOGGER_ERROR_INIT;
    }
    
    pthread_mutex_lock(&logger_ctx.mutex);
    logger_ctx.rotation_enabled = enabled;
    pthread_mutex_unlock(&logger_ctx.mutex);
    
    return LOGGER_SUCCESS;
}

void logger_debug(const char* format, ...) {
    va_list args;
    va_start(args, format);
    logger_vlog(LOG_LEVEL_DEBUG, format, args);
    va_end(args);
}

void logger_info(const char* format, ...) {
    va_list args;
    va_start(args, format);
    logger_vlog(LOG_LEVEL_INFO, format, args);
    va_end(args);
}

void logger_warning(const char* format, ...) {
    va_list args;
    va_start(args, format);
    logger_vlog(LOG_LEVEL_WARNING, format, args);
    va_end(args);
}

void logger_error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    logger_vlog(LOG_LEVEL_ERROR, format, args);
    va_end(args);
}

void logger_critical(const char* format, ...) {
    va_list args;
    va_start(args, format);
    logger_vlog(LOG_LEVEL_CRITICAL, format, args);
    va_end(args);
}

void logger_log(log_level_t level, const char* format, ...) {
    va_list args;
    va_start(args, format);
    logger_vlog(level, format, args);
    va_end(args);
}

void logger_vlog(log_level_t level, const char* format, va_list args) {
    if (!logger_ctx.initialized || level < logger_ctx.min_level) {
        return;
    }
    
    pthread_mutex_lock(&logger_ctx.mutex);
    
    char buffer[2048];
    char* ptr = buffer;
    size_t remaining = sizeof(buffer);
    
    if (logger_ctx.timestamps) {
        const char* timestamp = logger_get_timestamp();
        int written = snprintf(ptr, remaining, "[%s] ", timestamp);
        ptr += written;
        remaining -= written;
    }
    
    if (logger_ctx.thread_ids) {
        unsigned long tid = logger_get_thread_id();
        int written = snprintf(ptr, remaining, "[TID:%lu] ", tid);
        ptr += written;
        remaining -= written;
    }
    
    const char* level_str = logger_level_to_string(level);
    int written = snprintf(ptr, remaining, "[%s] ", level_str);
    ptr += written;
    remaining -= written;
    
    written = vsnprintf(ptr, remaining, format, args);
    ptr += written;
    remaining -= written;
    
    if (remaining > 1) {
        strcpy(ptr, "\n");
    }
    
    if (logger_ctx.output_mask & LOG_OUTPUT_CONSOLE) {
        if (logger_ctx.console_colors && isatty(STDOUT_FILENO)) {
            fprintf(stdout, "%s%s%s", color_codes[level], buffer, color_reset);
        } else {
            fprintf(stdout, "%s", buffer);
        }
        fflush(stdout);
    }
    
    if ((logger_ctx.output_mask & LOG_OUTPUT_FILE) && logger_ctx.log_file) {
        fprintf(logger_ctx.log_file, "%s", buffer);
        fflush(logger_ctx.log_file);
        
        if (logger_ctx.rotation_enabled) {
            fseek(logger_ctx.log_file, 0, SEEK_END);
            long current_size = ftell(logger_ctx.log_file);
            if (current_size > (long)logger_ctx.max_file_size) {
                logger_rotate_file();
            }
        }
    }
    
    pthread_mutex_unlock(&logger_ctx.mutex);
}

int logger_rotate_file(void) {
    if (!logger_ctx.log_file || !logger_ctx.rotation_enabled) {
        return LOGGER_ERROR_CONFIG;
    }
    
    fclose(logger_ctx.log_file);
    
    char backup_path[512];
    snprintf(backup_path, sizeof(backup_path), "%s.old", logger_ctx.log_file_path);
    
    rename(logger_ctx.log_file_path, backup_path);
    
    logger_ctx.log_file = fopen(logger_ctx.log_file_path, "a");
    if (!logger_ctx.log_file) {
        return LOGGER_ERROR_FILE;
    }
    
    logger_info("Log file rotated");
    return LOGGER_SUCCESS;
}

void logger_flush(void) {
    if (!logger_ctx.initialized) {
        return;
    }
    
    pthread_mutex_lock(&logger_ctx.mutex);
    
    if (logger_ctx.output_mask & LOG_OUTPUT_CONSOLE) {
        fflush(stdout);
    }
    
    if ((logger_ctx.output_mask & LOG_OUTPUT_FILE) && logger_ctx.log_file) {
        fflush(logger_ctx.log_file);
    }
    
    pthread_mutex_unlock(&logger_ctx.mutex);
}

const char* logger_level_to_string(log_level_t level) {
    switch (level) {
        case LOG_LEVEL_DEBUG:    return "DEBUG";
        case LOG_LEVEL_INFO:     return "INFO";
        case LOG_LEVEL_WARNING:  return "WARN";
        case LOG_LEVEL_ERROR:    return "ERROR";
        case LOG_LEVEL_CRITICAL: return "CRIT";
        default:                 return "UNKNOWN";
    }
}

const char* logger_get_timestamp(void) {
    static char timestamp[32];
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
    return timestamp;
}

unsigned long logger_get_thread_id(void) {
    return (unsigned long)pthread_self();
}