#include "delay.h"
#include "../utils/logger.h"
#include "../utils/time.h"
#include <stdlib.h>
#include <string.h>

#define DEFAULT_MAX_QUEUE_SIZE 1000

static delay_context_t delay_ctx = {0};

int delay_init(void) {
    if (delay_ctx.initialized) {
        return DELAY_SUCCESS;
    }
    
    memset(&delay_ctx, 0, sizeof(delay_context_t));
    
    delay_ctx.max_queue_size = DEFAULT_MAX_QUEUE_SIZE;
    delay_ctx.min_delay_ms = 0.0;
    delay_ctx.max_delay_ms = 0.0;
    delay_ctx.start_time = time_get_current();
    
    if (pthread_mutex_init(&delay_ctx.mutex, NULL) != 0) {
        return DELAY_ERROR_INIT;
    }
    
    delay_ctx.initialized = 1;
    
    logger_info("Delay module initialized (max queue size: %zu)", delay_ctx.max_queue_size);
    return DELAY_SUCCESS;
}

void delay_cleanup(void) {
    if (!delay_ctx.initialized) {
        return;
    }
    
    delay_clear_queue();
    
    pthread_mutex_destroy(&delay_ctx.mutex);
    memset(&delay_ctx, 0, sizeof(delay_context_t));
    
    logger_info("Delay module cleaned up");
}

int delay_apply(packet_t* packet, int delay_ms) {
    if (!packet || delay_ms < 0) {
        return DELAY_ERROR_INVALID;
    }
    
    if (delay_ms == 0) {
        return DELAY_SUCCESS;
    }
    
    return delay_add_to_queue(packet, delay_ms);
}

int delay_add_to_queue(packet_t* packet, int delay_ms) {
    if (!packet || delay_ms < 0) {
        return DELAY_ERROR_INVALID;
    }
    
    pthread_mutex_lock(&delay_ctx.mutex);
    
    if (delay_ctx.queue_size >= delay_ctx.max_queue_size) {
        pthread_mutex_unlock(&delay_ctx.mutex);
        logger_warning("Delay queue is full, dropping packet");
        return DELAY_ERROR_QUEUE_FULL;
    }
    
    delayed_packet_t* delayed = malloc(sizeof(delayed_packet_t));
    if (!delayed) {
        pthread_mutex_unlock(&delay_ctx.mutex);
        return DELAY_ERROR_MEMORY;
    }
    
    memcpy(&delayed->packet, packet, sizeof(packet_t));
    delayed->release_time = time_get_current() + (delay_ms / 1000);
    delayed->next = NULL;
    delayed->priority = 0;
    
    if (!delay_ctx.queue_head) {
        delay_ctx.queue_head = delayed;
        delay_ctx.queue_tail = delayed;
    } else {
        delay_ctx.queue_tail->next = delayed;
        delay_ctx.queue_tail = delayed;
    }
    
    delay_ctx.queue_size++;
    delay_ctx.total_packets_delayed++;
    delay_ctx.total_bytes_delayed += packet->size;
    
    double actual_delay_ms = delay_ms;
    if (delay_ctx.min_delay_ms == 0.0 || actual_delay_ms < delay_ctx.min_delay_ms) {
        delay_ctx.min_delay_ms = actual_delay_ms;
    }
    if (actual_delay_ms > delay_ctx.max_delay_ms) {
        delay_ctx.max_delay_ms = actual_delay_ms;
    }
    
    if (delay_ctx.total_packets_delayed > 0) {
        delay_ctx.average_delay_ms = (delay_ctx.average_delay_ms * (delay_ctx.total_packets_delayed - 1) + actual_delay_ms) / 
                                    delay_ctx.total_packets_delayed;
    } else {
        delay_ctx.average_delay_ms = actual_delay_ms;
    }
    
    pthread_mutex_unlock(&delay_ctx.mutex);
    
    logger_debug("Packet added to delay queue (delay: %dms, queue size: %zu)", delay_ms, delay_ctx.queue_size);
    return DELAY_SUCCESS;
}

delayed_packet_t* delay_get_ready_packet(void) {
    if (!delay_ctx.initialized) {
        return NULL;
    }
    
    pthread_mutex_lock(&delay_ctx.mutex);
    
    delayed_packet_t* current = delay_ctx.queue_head;
    delayed_packet_t* prev = NULL;
    time_t now = time_get_current();
    
    while (current) {
        if (current->release_time <= now) {
            if (prev) {
                prev->next = current->next;
            } else {
                delay_ctx.queue_head = current->next;
            }
            
            if (current == delay_ctx.queue_tail) {
                delay_ctx.queue_tail = prev;
            }
            
            delay_ctx.queue_size--;
            
            pthread_mutex_unlock(&delay_ctx.mutex);
            
            logger_debug("Packet released from delay queue (remaining: %zu)", delay_ctx.queue_size);
            return current;
        }
        
        prev = current;
        current = current->next;
    }
    
    pthread_mutex_unlock(&delay_ctx.mutex);
    return NULL;
}

void delay_process_queue(void) {
    if (!delay_ctx.initialized) {
        return;
    }
    
    delayed_packet_t* packet;
    int processed = 0;
    
    while ((packet = delay_get_ready_packet()) != NULL) {
        processed++;
        
        logger_debug("Processing delayed packet (size: %zu)", packet->packet.size);
        
        free(packet);
    }
    
    if (processed > 0) {
        logger_debug("Processed %d delayed packets", processed);
    }
}

void delay_clear_queue(void) {
    if (!delay_ctx.initialized) {
        return;
    }
    
    pthread_mutex_lock(&delay_ctx.mutex);
    
    delayed_packet_t* current = delay_ctx.queue_head;
    while (current) {
        delayed_packet_t* next = current->next;
        free(current);
        current = next;
    }
    
    delay_ctx.queue_head = NULL;
    delay_ctx.queue_tail = NULL;
    delay_ctx.queue_size = 0;
    
    pthread_mutex_unlock(&delay_ctx.mutex);
    
    logger_info("Delay queue cleared");
}

int delay_set_max_queue_size(size_t max_size) {
    if (!delay_ctx.initialized || max_size == 0) {
        return DELAY_ERROR_INVALID;
    }
    
    pthread_mutex_lock(&delay_ctx.mutex);
    delay_ctx.max_queue_size = max_size;
    pthread_mutex_unlock(&delay_ctx.mutex);
    
    logger_info("Delay queue max size set to %zu", max_size);
    return DELAY_SUCCESS;
}

size_t delay_get_queue_size(void) {
    if (!delay_ctx.initialized) {
        return 0;
    }
    
    pthread_mutex_lock(&delay_ctx.mutex);
    size_t size = delay_ctx.queue_size;
    pthread_mutex_unlock(&delay_ctx.mutex);
    
    return size;
}

void delay_get_stats(size_t* total_packets, size_t* total_bytes, 
                    double* avg_delay, double* max_delay, double* min_delay) {
    if (!delay_ctx.initialized) {
        if (total_packets) *total_packets = 0;
        if (total_bytes) *total_bytes = 0;
        if (avg_delay) *avg_delay = 0.0;
        if (max_delay) *max_delay = 0.0;
        if (min_delay) *min_delay = 0.0;
        return;
    }
    
    pthread_mutex_lock(&delay_ctx.mutex);
    
    if (total_packets) *total_packets = delay_ctx.total_packets_delayed;
    if (total_bytes) *total_bytes = delay_ctx.total_bytes_delayed;
    if (avg_delay) *avg_delay = delay_ctx.average_delay_ms;
    if (max_delay) *max_delay = delay_ctx.max_delay_ms;
    if (min_delay) *min_delay = delay_ctx.min_delay_ms;
    
    pthread_mutex_unlock(&delay_ctx.mutex);
}

void delay_reset_stats(void) {
    if (!delay_ctx.initialized) {
        return;
    }
    
    pthread_mutex_lock(&delay_ctx.mutex);
    
    delay_ctx.total_packets_delayed = 0;
    delay_ctx.total_bytes_delayed = 0;
    delay_ctx.start_time = time_get_current();
    delay_ctx.average_delay_ms = 0.0;
    delay_ctx.max_delay_ms = 0.0;
    delay_ctx.min_delay_ms = 0.0;
    
    pthread_mutex_unlock(&delay_ctx.mutex);
    
    logger_info("Delay statistics reset");
}

int delay_calculate_optimal_delay(size_t current_queue_size, int base_delay_ms) {
    if (base_delay_ms < 0) {
        return 0;
    }
    
    double load_factor = (double)current_queue_size / delay_ctx.max_queue_size;
    
    if (load_factor > 0.8) {
        return base_delay_ms * 2;
    } else if (load_factor > 0.6) {
        return base_delay_ms + (base_delay_ms / 2);
    } else if (load_factor > 0.4) {
        return base_delay_ms;
    } else {
        return base_delay_ms / 2;
    }
}

double delay_get_current_load(void) {
    if (!delay_ctx.initialized || delay_ctx.max_queue_size == 0) {
        return 0.0;
    }
    
    pthread_mutex_lock(&delay_ctx.mutex);
    double load = (double)delay_ctx.queue_size / delay_ctx.max_queue_size;
    pthread_mutex_unlock(&delay_ctx.mutex);
    
    return load;
}