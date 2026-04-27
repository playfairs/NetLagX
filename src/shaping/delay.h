#ifndef DELAY_H
#define DELAY_H

#include "../../include/netlagx.h"
#include <time.h>

typedef struct {
    packet_t packet;
    time_t release_time;
    struct delayed_packet* next;
    int priority;
} delayed_packet_t;

typedef struct {
    delayed_packet_t* queue_head;
    delayed_packet_t* queue_tail;
    size_t queue_size;
    size_t max_queue_size;
    
    size_t total_packets_delayed;
    size_t total_bytes_delayed;
    time_t start_time;
    
    double average_delay_ms;
    double max_delay_ms;
    double min_delay_ms;
    
    pthread_mutex_t mutex;
    int initialized;
} delay_context_t;

typedef enum {
    DELAY_SUCCESS = 0,
    DELAY_ERROR_INIT = -1,
    DELAY_ERROR_QUEUE_FULL = -2,
    DELAY_ERROR_INVALID = -3,
    DELAY_ERROR_MEMORY = -4
} delay_result_t;

int delay_init(void);
void delay_cleanup(void);

int delay_apply(packet_t* packet, int delay_ms);
int delay_add_to_queue(packet_t* packet, int delay_ms);
delayed_packet_t* delay_get_ready_packet(void);

void delay_process_queue(void);
void delay_clear_queue(void);

int delay_set_max_queue_size(size_t max_size);
size_t delay_get_queue_size(void);

void delay_get_stats(size_t* total_packets, size_t* total_bytes, 
                    double* avg_delay, double* max_delay, double* min_delay);
void delay_reset_stats(void);

int delay_calculate_optimal_delay(size_t current_queue_size, int base_delay_ms);
double delay_get_current_load(void);

#endif