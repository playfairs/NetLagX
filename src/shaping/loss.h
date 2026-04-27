#ifndef LOSS_H
#define LOSS_H

#include "../../include/netlagx.h"
#include <time.h>

typedef enum {
    LOSS_PATTERN_UNIFORM = 0,
    LOSS_PATTERN_BURST = 1,
    LOSS_PATTERN_PERIODIC = 2,
    LOSS_PATTERN_RANDOM = 3,
    LOSS_PATTERN_ADAPTIVE = 4
} loss_pattern_t;

typedef struct {
    loss_pattern_t pattern;
    double loss_rate;
    double burst_probability;
    int burst_size_min;
    int burst_size_max;
    int period_length;
    int period_drops;
    
    size_t total_packets_seen;
    size_t total_packets_dropped;
    size_t total_bytes_dropped;
    time_t start_time;
    
    int current_burst_count;
    int current_period_position;
    int consecutive_drops;
    int consecutive_keeps;
    
    double actual_loss_rate;
    double burst_loss_rate;
    
    pthread_mutex_t mutex;
    int initialized;
} loss_context_t;

typedef enum {
    LOSS_SUCCESS = 0,
    LOSS_ERROR_INIT = -1,
    LOSS_ERROR_INVALID = -2,
    LOSS_ERROR_CONFIG = -3
} loss_result_t;

int loss_init(void);
void loss_cleanup(void);

int loss_should_drop(double loss_rate);
int loss_apply(packet_t* packet, double loss_rate);
int loss_apply_pattern(packet_t* packet);

int loss_set_pattern(loss_pattern_t pattern);
int loss_set_loss_rate(double loss_rate);
int loss_set_burst_parameters(double burst_probability, int burst_min, int burst_max);
int loss_set_periodic_parameters(int period_length, int period_drops);

int loss_is_in_burst(void);
int loss_should_drop_burst(double base_rate);
int loss_should_drop_periodic(void);
int loss_should_drop_adaptive(double base_rate);

void loss_process_queue(void);

void loss_get_stats(size_t* total_seen, size_t* total_dropped, size_t* total_bytes,
                   double* actual_rate, double* burst_rate);
void loss_reset_stats(void);

double loss_get_current_loss_rate(void);
int loss_get_pattern_info(loss_pattern_t* pattern, double* loss_rate);

#endif