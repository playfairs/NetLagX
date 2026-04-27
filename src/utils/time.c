#include "time.h"
#include <string.h>
#include <unistd.h>

time_t time_get_current(void) {
    return time(NULL);
}

double time_get_current_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec * 1000.0 + (double)tv.tv_usec / 1000.0;
}

double time_get_current_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec * 1000000.0 + (double)tv.tv_usec;
}

int time_init_tracker(time_tracker_t* tracker) {
    if (!tracker) {
        return TIME_ERROR_INVALID;
    }
    
    memset(tracker, 0, sizeof(time_tracker_t));
    tracker->start_time = time_get_current();
    tracker->last_update = tracker->start_time;
    
    return TIME_SUCCESS;
}

void time_reset_tracker(time_tracker_t* tracker) {
    if (!tracker) {
        return;
    }
    
    tracker->start_time = time_get_current();
    tracker->last_update = tracker->start_time;
    tracker->elapsed_ms = 0.0;
    tracker->total_elapsed_ms = 0.0;
}

double time_get_elapsed_ms(time_tracker_t* tracker) {
    if (!tracker) {
        return 0.0;
    }
    
    time_t now = time_get_current();
    return time_diff_ms(tracker->start_time, now);
}

double time_get_elapsed_us(time_tracker_t* tracker) {
    if (!tracker) {
        return 0.0;
    }
    
    struct timeval start, now;
    gettimeofday(&start, NULL);
    gettimeofday(&now, NULL);
    
    return time_diff_us(&start, &now);
}

void time_update_tracker(time_tracker_t* tracker) {
    if (!tracker) {
        return;
    }
    
    time_t now = time_get_current();
    tracker->elapsed_ms = time_diff_ms(tracker->last_update, now);
    tracker->total_elapsed_ms = time_diff_ms(tracker->start_time, now);
    tracker->last_update = now;
}

double time_diff_ms(time_t start, time_t end) {
    return difftime(end, start) * 1000.0;
}

double time_diff_us(struct timeval* start, struct timeval* end) {
    if (!start || !end) {
        return 0.0;
    }
    
    return (double)(end->tv_sec - start->tv_sec) * 1000000.0 + 
           (double)(end->tv_usec - start->tv_usec);
}

void time_sleep_ms(int milliseconds) {
    if (milliseconds <= 0) {
        return;
    }
    
    usleep(milliseconds * 1000);
}

void time_sleep_us(int microseconds) {
    if (microseconds <= 0) {
        return;
    }
    
    usleep(microseconds);
}

const char* time_format_duration(double duration_ms) {
    static char buffer[64];
    
    if (duration_ms < 1000.0) {
        snprintf(buffer, sizeof(buffer), "%.2f ms", duration_ms);
    } else if (duration_ms < 60000.0) {
        double seconds = duration_ms / 1000.0;
        snprintf(buffer, sizeof(buffer), "%.2f s", seconds);
    } else if (duration_ms < 3600000.0) {
        double minutes = duration_ms / 60000.0;
        snprintf(buffer, sizeof(buffer), "%.2f min", minutes);
    } else {
        double hours = duration_ms / 3600000.0;
        snprintf(buffer, sizeof(buffer), "%.2f h", hours);
    }
    
    return buffer;
}

const char* time_format_timestamp(time_t timestamp) {
    static char buffer[32];
    struct tm* tm_info = localtime(&timestamp);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);
    return buffer;
}

int time_is_timeout(time_t start_time, int timeout_ms) {
    if (timeout_ms <= 0) {
        return 0;
    }
    
    time_t now = time_get_current();
    double elapsed_ms = time_diff_ms(start_time, now);
    
    return (elapsed_ms >= timeout_ms) ? 1 : 0;
}