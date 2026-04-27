#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <stdarg.h>

typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO = 1,
    LOG_LEVEL_WARNING = 2,
    LOG_LEVEL_ERROR = 3,
    LOG_LEVEL_CRITICAL = 4
} log_level_t;

typedef enum {
    LOG_OUTPUT_CONSOLE = 1,
    LOG_OUTPUT_FILE = 2,
    LOG_OUTPUT_SYSLOG = 4,
    LOG_OUTPUT_ALL = LOG_OUTPUT_CONSOLE | LOG_OUTPUT_FILE | LOG_OUTPUT_SYSLOG
} log_output_t;

typedef struct {
    log_level_t min_level;
    log_output_t output_mask;
    char log_file_path[256];
    FILE* log_file;
    int console_colors;
    int timestamps;
    int thread_ids;
    size_t max_file_size;
    int rotation_enabled;
    
    pthread_mutex_t mutex;
    int initialized;
} logger_context_t;

typedef enum {
    LOGGER_SUCCESS = 0,
    LOGGER_ERROR_INIT = -1,
    LOGGER_ERROR_CONFIG = -2,
    LOGGER_ERROR_FILE = -3
} logger_result_t;

int logger_init(log_level_t min_level, log_output_t output_mask);
void logger_cleanup(void);

int logger_set_level(log_level_t level);
int logger_set_output(log_output_t output_mask);
int logger_set_log_file(const char* file_path);
int logger_set_console_colors(int enabled);
int logger_set_timestamps(int enabled);
int logger_set_thread_ids(int enabled);
int logger_set_max_file_size(size_t max_size);
int logger_set_rotation(int enabled);

void logger_debug(const char* format, ...);
void logger_info(const char* format, ...);
void logger_warning(const char* format, ...);
void logger_error(const char* format, ...);
void logger_critical(const char* format, ...);

void logger_log(log_level_t level, const char* format, ...);
void logger_vlog(log_level_t level, const char* format, va_list args);

int logger_rotate_file(void);
void logger_flush(void);

const char* logger_level_to_string(log_level_t level);
const char* logger_get_timestamp(void);
unsigned long logger_get_thread_id(void);

#endif