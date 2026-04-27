#ifndef JITTER_H
#define JITTER_H

#include "../../include/netlagx.h"
#include <time.h>

typedef enum {
    JITTER_DISTRIBUTION_UNIFORM = 0,
    JITTER_DISTRIBUTION_GAUSSIAN = 1,
    JITTER_DISTRIBUTION_EXPONENTIAL = 2,
    JITTER_DISTRIBUTION_CUSTOM = 3
} jitter_distribution_t;

typedef struct {
    jitter_distribution_t distribution;
    double mean_ms;
    double std_dev_ms;
    double min_ms;
    double max_ms;
    double lambda;
    
    size_t total_packets_jittered;
    size_t total_bytes_jittered;
    time_t start_time;
    
    double average_jitter_ms;
    double max_jitter_applied;
    double min_jitter_applied;
    
    pthread_mutex_t mutex;
    int initialized;
} jitter_context_t;

typedef enum {
    JITTER_SUCCESS = 0,
    JITTER_ERROR_INIT = -1,
    JITTER_ERROR_INVALID = -2,
    JITTER_ERROR_CONFIG = -3
} jitter_result_t;

int jitter_init(void);
void jitter_cleanup(void);

int jitter_apply(packet_t* packet, int base_delay_ms);
int jitter_add_jitter(packet_t* packet, int base_delay_ms);

int jitter_set_distribution(jitter_distribution_t distribution);
int jitter_set_parameters(double mean_ms, double std_dev_ms, double min_ms, double max_ms);
int jitter_set_exponential_lambda(double lambda);

double jitter_get_random_jitter(int base_delay_ms);
double jitter_calculate_jitter(int base_delay_ms);

void jitter_process_queue(void);

void jitter_get_stats(size_t* total_packets, size_t* total_bytes,
                     double* avg_jitter, double* max_jitter, double* min_jitter);
void jitter_reset_stats(void);

double jitter_get_current_variance(void);
int jitter_get_distribution_info(jitter_distribution_t* distribution, 
                               double* mean_ms, double* std_dev_ms);

#endif