#include "../../../include/netlagx.h"
#include <stdarg.h>
#include <time.h>

void log_message(const char* level, const char* format, ...) {
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
    
    va_list args;
    va_start(args, format);
    
    printf("[%s] %s: ", timestamp, level);
    vprintf(format, args);
    printf("\n");
    
    va_end(args);
}
