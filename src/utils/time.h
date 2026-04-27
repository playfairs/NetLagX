#ifndef TIME_H
#define TIME_H

#include <time.h>
#include <sys/time.h>

typedef struct {
    time_t start_time;
    time_t last_update;
    double elapsed_ms;
    double total_elapsed_ms;
} time_tracker_t;

typedef enum {
    TIME_SUCCESS = 0,
    TIME_ERROR_INVALID = -1
} time_result_t;

time_t time_get_current(void);
double time_get_current_ms(void);
double time_get_current_us(void);

int time_init_tracker(time_tracker_t* tracker);
void time_reset_tracker(time_tracker_t* tracker);
double time_get_elapsed_ms(time_tracker_t* tracker);
double time_get_elapsed_us(time_tracker_t* tracker);
void time_update_tracker(time_tracker_t* tracker);

double time_diff_ms(time_t start, time_t end);
double time_diff_us(struct timeval* start, struct timeval* end);

void time_sleep_ms(int milliseconds);
void time_sleep_us(int microseconds);

const char* time_format_duration(double duration_ms);
const char* time_format_timestamp(time_t timestamp);

int time_is_timeout(time_t start_time, int timeout_ms);

#endif