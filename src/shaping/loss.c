#include "loss.h"
#include "../utils/logger.h"
#include "../utils/time.h"
#include <stdlib.h>
#include <string.h>

static loss_context_t loss_ctx = {0};

int loss_init(void) {
    if (loss_ctx.initialized) {
        return LOSS_SUCCESS;
    }
    
    memset(&loss_ctx, 0, sizeof(loss_context_t));
    
    loss_ctx.pattern = LOSS_PATTERN_UNIFORM;
    loss_ctx.loss_rate = 0.1;
    loss_ctx.burst_probability = 0.05;
    loss_ctx.burst_size_min = 3;
    loss_ctx.burst_size_max = 10;
    loss_ctx.period_length = 100;
    loss_ctx.period_drops = 10;
    loss_ctx.start_time = time_get_current();
    
    if (pthread_mutex_init(&loss_ctx.mutex, NULL) != 0) {
        return LOSS_ERROR_INIT;
    }
    
    loss_ctx.initialized = 1;
    
    logger_info("Loss module initialized (pattern: %d, rate: %.2f%%)", 
                loss_ctx.pattern, loss_ctx.loss_rate * 100);
    return LOSS_SUCCESS;
}

void loss_cleanup(void) {
    if (!loss_ctx.initialized) {
        return;
    }
    
    pthread_mutex_destroy(&loss_ctx.mutex);
    memset(&loss_ctx, 0, sizeof(loss_context_t));
    
    logger_info("Loss module cleaned up");
}

int loss_should_drop(double loss_rate) {
    if (loss_rate < 0.0 || loss_rate > 1.0) {
        return 0;
    }
    
    double random_value = ((double)rand() / RAND_MAX);
    return (random_value < loss_rate) ? 1 : 0;
}

int loss_apply(packet_t* packet, double loss_rate) {
    if (!packet) {
        return LOSS_ERROR_INVALID;
    }
    
    pthread_mutex_lock(&loss_ctx.mutex);
    
    loss_ctx.total_packets_seen++;
    
    int should_drop = loss_should_drop(loss_rate);
    
    if (should_drop) {
        loss_ctx.total_packets_dropped++;
        loss_ctx.total_bytes_dropped += packet->size;
        loss_ctx.consecutive_drops++;
        loss_ctx.consecutive_keeps = 0;
    } else {
        loss_ctx.consecutive_keeps++;
        loss_ctx.consecutive_drops = 0;
    }
    
    if (loss_ctx.total_packets_seen > 0) {
        loss_ctx.actual_loss_rate = (double)loss_ctx.total_packets_dropped / loss_ctx.total_packets_seen;
    }
    
    pthread_mutex_unlock(&loss_ctx.mutex);
    
    if (should_drop) {
        logger_debug("Packet dropped (size: %zu, loss rate: %.2f%%)", 
                    packet->size, loss_ctx.actual_loss_rate * 100);
        return -1;
    }
    
    return LOSS_SUCCESS;
}

int loss_apply_pattern(packet_t* packet) {
    if (!packet) {
        return LOSS_ERROR_INVALID;
    }
    
    pthread_mutex_lock(&loss_ctx.mutex);
    loss_pattern_t pattern = loss_ctx.pattern;
    double base_rate = loss_ctx.loss_rate;
    pthread_mutex_unlock(&loss_ctx.mutex);
    
    int should_drop = 0;
    
    switch (pattern) {
        case LOSS_PATTERN_UNIFORM:
            should_drop = loss_should_drop(base_rate);
            break;
            
        case LOSS_PATTERN_BURST:
            should_drop = loss_should_drop_burst(base_rate);
            break;
            
        case LOSS_PATTERN_PERIODIC:
            should_drop = loss_should_drop_periodic();
            break;
            
        case LOSS_PATTERN_RANDOM:
            should_drop = loss_should_drop(base_rate);
            break;
            
        case LOSS_PATTERN_ADAPTIVE:
            should_drop = loss_should_drop_adaptive(base_rate);
            break;
            
        default:
            should_drop = loss_should_drop(base_rate);
            break;
    }
    
    pthread_mutex_lock(&loss_ctx.mutex);
    
    loss_ctx.total_packets_seen++;
    
    if (should_drop) {
        loss_ctx.total_packets_dropped++;
        loss_ctx.total_bytes_dropped += packet->size;
        loss_ctx.consecutive_drops++;
        loss_ctx.consecutive_keeps = 0;
    } else {
        loss_ctx.consecutive_keeps++;
        loss_ctx.consecutive_drops = 0;
    }
    
    if (loss_ctx.total_packets_seen > 0) {
        loss_ctx.actual_loss_rate = (double)loss_ctx.total_packets_dropped / loss_ctx.total_packets_seen;
    }
    
    pthread_mutex_unlock(&loss_ctx.mutex);
    
    if (should_drop) {
        logger_debug("Packet dropped by pattern %d (size: %zu)", pattern, packet->size);
        return -1;
    }
    
    return LOSS_SUCCESS;
}

int loss_set_pattern(loss_pattern_t pattern) {
    if (!loss_ctx.initialized) {
        return LOSS_ERROR_INIT;
    }
    
    if (pattern < LOSS_PATTERN_UNIFORM || pattern > LOSS_PATTERN_ADAPTIVE) {
        return LOSS_ERROR_INVALID;
    }
    
    pthread_mutex_lock(&loss_ctx.mutex);
    loss_ctx.pattern = pattern;
    loss_ctx.current_burst_count = 0;
    loss_ctx.current_period_position = 0;
    pthread_mutex_unlock(&loss_ctx.mutex);
    
    logger_info("Loss pattern set to %d", pattern);
    return LOSS_SUCCESS;
}

int loss_set_loss_rate(double loss_rate) {
    if (!loss_ctx.initialized || loss_rate < 0.0 || loss_rate > 1.0) {
        return LOSS_ERROR_INVALID;
    }
    
    pthread_mutex_lock(&loss_ctx.mutex);
    loss_ctx.loss_rate = loss_rate;
    pthread_mutex_unlock(&loss_ctx.mutex);
    
    logger_info("Loss rate set to %.2f%%", loss_rate * 100);
    return LOSS_SUCCESS;
}

int loss_set_burst_parameters(double burst_probability, int burst_min, int burst_max) {
    if (!loss_ctx.initialized || burst_probability < 0.0 || burst_probability > 1.0 || 
        burst_min < 1 || burst_max < burst_min) {
        return LOSS_ERROR_INVALID;
    }
    
    pthread_mutex_lock(&loss_ctx.mutex);
    loss_ctx.burst_probability = burst_probability;
    loss_ctx.burst_size_min = burst_min;
    loss_ctx.burst_size_max = burst_max;
    pthread_mutex_unlock(&loss_ctx.mutex);
    
    logger_info("Burst parameters set: prob=%.2f%%, min=%d, max=%d", 
                burst_probability * 100, burst_min, burst_max);
    return LOSS_SUCCESS;
}

int loss_set_periodic_parameters(int period_length, int period_drops) {
    if (!loss_ctx.initialized || period_length <= 0 || period_drops < 0 || 
        period_drops > period_length) {
        return LOSS_ERROR_INVALID;
    }
    
    pthread_mutex_lock(&loss_ctx.mutex);
    loss_ctx.period_length = period_length;
    loss_ctx.period_drops = period_drops;
    loss_ctx.current_period_position = 0;
    pthread_mutex_unlock(&loss_ctx.mutex);
    
    logger_info("Periodic parameters set: length=%d, drops=%d", period_length, period_drops);
    return LOSS_SUCCESS;
}

int loss_is_in_burst(void) {
    if (!loss_ctx.initialized) {
        return 0;
    }
    
    pthread_mutex_lock(&loss_ctx.mutex);
    int in_burst = (loss_ctx.current_burst_count > 0);
    pthread_mutex_unlock(&loss_ctx.mutex);
    
    return in_burst;
}

int loss_should_drop_burst(double base_rate) {
    pthread_mutex_lock(&loss_ctx.mutex);
    
    int should_drop = 0;
    
    if (loss_ctx.current_burst_count > 0) {
        should_drop = 1;
        loss_ctx.current_burst_count--;
        
        if (loss_ctx.current_burst_count == 0) {
            loss_ctx.burst_loss_rate = 0.0;
        }
    } else {
        double burst_prob = loss_ctx.burst_probability;
        double random_value = ((double)rand() / RAND_MAX);
        
        if (random_value < burst_prob) {
            int burst_size = loss_ctx.burst_size_min + 
                           (rand() % (loss_ctx.burst_size_max - loss_ctx.burst_size_min + 1));
            loss_ctx.current_burst_count = burst_size - 1;
            should_drop = 1;
            
            loss_ctx.burst_loss_rate = (double)burst_size / (burst_size + 10);
        }
    }
    
    pthread_mutex_unlock(&loss_ctx.mutex);
    
    return should_drop;
}

int loss_should_drop_periodic(void) {
    pthread_mutex_lock(&loss_ctx.mutex);
    
    int should_drop = 0;
    
    if (loss_ctx.current_period_position < loss_ctx.period_drops) {
        should_drop = 1;
    }
    
    loss_ctx.current_period_position++;
    if (loss_ctx.current_period_position >= loss_ctx.period_length) {
        loss_ctx.current_period_position = 0;
    }
    
    pthread_mutex_unlock(&loss_ctx.mutex);
    
    return should_drop;
}

int loss_should_drop_adaptive(double base_rate) {
    pthread_mutex_lock(&loss_ctx.mutex);
    
    double adaptive_rate = base_rate;
    
    if (loss_ctx.consecutive_keeps > 50) {
        adaptive_rate *= 2.0;
        if (adaptive_rate > 0.5) adaptive_rate = 0.5;
    } else if (loss_ctx.consecutive_drops > 10) {
        adaptive_rate *= 0.5;
    }
    
    int should_drop = loss_should_drop(adaptive_rate);
    
    pthread_mutex_unlock(&loss_ctx.mutex);
    
    return should_drop;
}

void loss_process_queue(void) {
    if (!loss_ctx.initialized) {
        return;
    }
}

void loss_get_stats(size_t* total_seen, size_t* total_dropped, size_t* total_bytes,
                   double* actual_rate, double* burst_rate) {
    if (!loss_ctx.initialized) {
        if (total_seen) *total_seen = 0;
        if (total_dropped) *total_dropped = 0;
        if (total_bytes) *total_bytes = 0;
        if (actual_rate) *actual_rate = 0.0;
        if (burst_rate) *burst_rate = 0.0;
        return;
    }
    
    pthread_mutex_lock(&loss_ctx.mutex);
    
    if (total_seen) *total_seen = loss_ctx.total_packets_seen;
    if (total_dropped) *total_dropped = loss_ctx.total_packets_dropped;
    if (total_bytes) *total_bytes = loss_ctx.total_bytes_dropped;
    if (actual_rate) *actual_rate = loss_ctx.actual_loss_rate;
    if (burst_rate) *burst_rate = loss_ctx.burst_loss_rate;
    
    pthread_mutex_unlock(&loss_ctx.mutex);
}

void loss_reset_stats(void) {
    if (!loss_ctx.initialized) {
        return;
    }
    
    pthread_mutex_lock(&loss_ctx.mutex);
    
    loss_ctx.total_packets_seen = 0;
    loss_ctx.total_packets_dropped = 0;
    loss_ctx.total_bytes_dropped = 0;
    loss_ctx.start_time = time_get_current();
    loss_ctx.actual_loss_rate = 0.0;
    loss_ctx.burst_loss_rate = 0.0;
    loss_ctx.consecutive_drops = 0;
    loss_ctx.consecutive_keeps = 0;
    
    pthread_mutex_unlock(&loss_ctx.mutex);
    
    logger_info("Loss statistics reset");
}

double loss_get_current_loss_rate(void) {
    if (!loss_ctx.initialized) {
        return 0.0;
    }
    
    pthread_mutex_lock(&loss_ctx.mutex);
    double rate = loss_ctx.actual_loss_rate;
    pthread_mutex_unlock(&loss_ctx.mutex);
    
    return rate;
}

int loss_get_pattern_info(loss_pattern_t* pattern, double* loss_rate) {
    if (!loss_ctx.initialized) {
        return LOSS_ERROR_INIT;
    }
    
    pthread_mutex_lock(&loss_ctx.mutex);
    
    if (pattern) *pattern = loss_ctx.pattern;
    if (loss_rate) *loss_rate = loss_ctx.loss_rate;
    
    pthread_mutex_unlock(&loss_ctx.mutex);
    
    return LOSS_SUCCESS;
}