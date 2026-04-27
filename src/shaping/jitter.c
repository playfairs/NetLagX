#include "jitter.h"
#include "delay.h"
#include "../utils/logger.h"
#include "../utils/time.h"
#include <stdlib.h>
#include <math.h>

static jitter_context_t jitter_ctx = {0};

static double gaussian_random(double mean, double std_dev);
static double exponential_random(double lambda);
static double uniform_random(double min, double max);

int jitter_init(void) {
    if (jitter_ctx.initialized) {
        return JITTER_SUCCESS;
    }
    
    memset(&jitter_ctx, 0, sizeof(jitter_context_t));
    
    jitter_ctx.distribution = JITTER_DISTRIBUTION_UNIFORM;
    jitter_ctx.mean_ms = 50.0;
    jitter_ctx.std_dev_ms = 20.0;
    jitter_ctx.min_ms = 0.0;
    jitter_ctx.max_ms = 100.0;
    jitter_ctx.lambda = 0.1;
    jitter_ctx.start_time = time_get_current();
    
    if (pthread_mutex_init(&jitter_ctx.mutex, NULL) != 0) {
        return JITTER_ERROR_INIT;
    }
    
    jitter_ctx.initialized = 1;
    
    logger_info("Jitter module initialized (distribution: %d, mean: %.2fms)", 
                jitter_ctx.distribution, jitter_ctx.mean_ms);
    return JITTER_SUCCESS;
}

void jitter_cleanup(void) {
    if (!jitter_ctx.initialized) {
        return;
    }
    
    pthread_mutex_destroy(&jitter_ctx.mutex);
    memset(&jitter_ctx, 0, sizeof(jitter_context_t));
    
    logger_info("Jitter module cleaned up");
}

int jitter_apply(packet_t* packet, int base_delay_ms) {
    if (!packet || base_delay_ms < 0) {
        return JITTER_ERROR_INVALID;
    }
    
    double jitter_amount = jitter_calculate_jitter(base_delay_ms);
    int total_delay = base_delay_ms + (int)round(jitter_amount);
    
    if (total_delay < 0) {
        total_delay = 0;
    }
    
    return delay_apply(packet, total_delay);
}

int jitter_add_jitter(packet_t* packet, int base_delay_ms) {
    if (!packet || base_delay_ms < 0) {
        return JITTER_ERROR_INVALID;
    }
    
    double jitter_amount = jitter_get_random_jitter(base_delay_ms);
    int total_delay = base_delay_ms + (int)round(jitter_amount);
    
    if (total_delay < 0) {
        total_delay = 0;
    }
    
    pthread_mutex_lock(&jitter_ctx.mutex);
    
    jitter_ctx.total_packets_jittered++;
    jitter_ctx.total_bytes_jittered += packet->size;
    
    if (jitter_ctx.min_jitter_applied == 0.0 || jitter_amount < jitter_ctx.min_jitter_applied) {
        jitter_ctx.min_jitter_applied = jitter_amount;
    }
    if (jitter_amount > jitter_ctx.max_jitter_applied) {
        jitter_ctx.max_jitter_applied = jitter_amount;
    }
    
    if (jitter_ctx.total_packets_jittered > 0) {
        jitter_ctx.average_jitter_ms = (jitter_ctx.average_jitter_ms * (jitter_ctx.total_packets_jittered - 1) + jitter_amount) / 
                                      jitter_ctx.total_packets_jittered;
    } else {
        jitter_ctx.average_jitter_ms = jitter_amount;
    }
    
    pthread_mutex_unlock(&jitter_ctx.mutex);
    
    logger_debug("Applied jitter: %.2fms (base: %dms, total: %dms)", 
                jitter_amount, base_delay_ms, total_delay);
    
    return delay_apply(packet, total_delay);
}

int jitter_set_distribution(jitter_distribution_t distribution) {
    if (!jitter_ctx.initialized) {
        return JITTER_ERROR_INIT;
    }
    
    if (distribution < JITTER_DISTRIBUTION_UNIFORM || distribution > JITTER_DISTRIBUTION_CUSTOM) {
        return JITTER_ERROR_INVALID;
    }
    
    pthread_mutex_lock(&jitter_ctx.mutex);
    jitter_ctx.distribution = distribution;
    pthread_mutex_unlock(&jitter_ctx.mutex);
    
    logger_info("Jitter distribution set to %d", distribution);
    return JITTER_SUCCESS;
}

int jitter_set_parameters(double mean_ms, double std_dev_ms, double min_ms, double max_ms) {
    if (!jitter_ctx.initialized || mean_ms < 0 || std_dev_ms < 0 || min_ms < 0 || max_ms < min_ms) {
        return JITTER_ERROR_INVALID;
    }
    
    pthread_mutex_lock(&jitter_ctx.mutex);
    jitter_ctx.mean_ms = mean_ms;
    jitter_ctx.std_dev_ms = std_dev_ms;
    jitter_ctx.min_ms = min_ms;
    jitter_ctx.max_ms = max_ms;
    pthread_mutex_unlock(&jitter_ctx.mutex);
    
    logger_info("Jitter parameters set: mean=%.2fms, std_dev=%.2fms, min=%.2fms, max=%.2fms",
                mean_ms, std_dev_ms, min_ms, max_ms);
    return JITTER_SUCCESS;
}

int jitter_set_exponential_lambda(double lambda) {
    if (!jitter_ctx.initialized || lambda <= 0) {
        return JITTER_ERROR_INVALID;
    }
    
    pthread_mutex_lock(&jitter_ctx.mutex);
    jitter_ctx.lambda = lambda;
    pthread_mutex_unlock(&jitter_ctx.mutex);
    
    logger_info("Jitter exponential lambda set to %.6f", lambda);
    return JITTER_SUCCESS;
}

double jitter_get_random_jitter(int base_delay_ms) {
    if (!jitter_ctx.initialized || base_delay_ms < 0) {
        return 0.0;
    }
    
    pthread_mutex_lock(&jitter_ctx.mutex);
    jitter_distribution_t dist = jitter_ctx.distribution;
    double mean = jitter_ctx.mean_ms;
    double std_dev = jitter_ctx.std_dev_ms;
    double min_val = jitter_ctx.min_ms;
    double max_val = jitter_ctx.max_ms;
    double lambda = jitter_ctx.lambda;
    pthread_mutex_unlock(&jitter_ctx.mutex);
    
    double jitter = 0.0;
    
    switch (dist) {
        case JITTER_DISTRIBUTION_UNIFORM:
            jitter = uniform_random(min_val, max_val);
            break;
            
        case JITTER_DISTRIBUTION_GAUSSIAN:
            jitter = gaussian_random(mean, std_dev);
            if (jitter < min_val) jitter = min_val;
            if (jitter > max_val) jitter = max_val;
            break;
            
        case JITTER_DISTRIBUTION_EXPONENTIAL:
            jitter = exponential_random(lambda);
            if (jitter < min_val) jitter = min_val;
            if (jitter > max_val) jitter = max_val;
            break;
            
        case JITTER_DISTRIBUTION_CUSTOM:
        default:
            jitter = uniform_random(min_val, max_val);
            break;
    }
    
    return jitter;
}

double jitter_calculate_jitter(int base_delay_ms) {
    if (!jitter_ctx.initialized || base_delay_ms < 0) {
        return 0.0;
    }
    
    double jitter_factor = jitter_get_random_jitter(base_delay_ms);
    
    return jitter_factor;
}

void jitter_process_queue(void) {
    if (!jitter_ctx.initialized) {
        return;
    }
}

void jitter_get_stats(size_t* total_packets, size_t* total_bytes,
                     double* avg_jitter, double* max_jitter, double* min_jitter) {
    if (!jitter_ctx.initialized) {
        if (total_packets) *total_packets = 0;
        if (total_bytes) *total_bytes = 0;
        if (avg_jitter) *avg_jitter = 0.0;
        if (max_jitter) *max_jitter = 0.0;
        if (min_jitter) *min_jitter = 0.0;
        return;
    }
    
    pthread_mutex_lock(&jitter_ctx.mutex);
    
    if (total_packets) *total_packets = jitter_ctx.total_packets_jittered;
    if (total_bytes) *total_bytes = jitter_ctx.total_bytes_jittered;
    if (avg_jitter) *avg_jitter = jitter_ctx.average_jitter_ms;
    if (max_jitter) *max_jitter = jitter_ctx.max_jitter_applied;
    if (min_jitter) *min_jitter = jitter_ctx.min_jitter_applied;
    
    pthread_mutex_unlock(&jitter_ctx.mutex);
}

void jitter_reset_stats(void) {
    if (!jitter_ctx.initialized) {
        return;
    }
    
    pthread_mutex_lock(&jitter_ctx.mutex);
    
    jitter_ctx.total_packets_jittered = 0;
    jitter_ctx.total_bytes_jittered = 0;
    jitter_ctx.start_time = time_get_current();
    jitter_ctx.average_jitter_ms = 0.0;
    jitter_ctx.max_jitter_applied = 0.0;
    jitter_ctx.min_jitter_applied = 0.0;
    
    pthread_mutex_unlock(&jitter_ctx.mutex);
    
    logger_info("Jitter statistics reset");
}

double jitter_get_current_variance(void) {
    if (!jitter_ctx.initialized) {
        return 0.0;
    }
    
    pthread_mutex_lock(&jitter_ctx.mutex);
    double variance = jitter_ctx.std_dev_ms * jitter_ctx.std_dev_ms;
    pthread_mutex_unlock(&jitter_ctx.mutex);
    
    return variance;
}

int jitter_get_distribution_info(jitter_distribution_t* distribution, 
                               double* mean_ms, double* std_dev_ms) {
    if (!jitter_ctx.initialized) {
        return JITTER_ERROR_INIT;
    }
    
    pthread_mutex_lock(&jitter_ctx.mutex);
    
    if (distribution) *distribution = jitter_ctx.distribution;
    if (mean_ms) *mean_ms = jitter_ctx.mean_ms;
    if (std_dev_ms) *std_dev_ms = jitter_ctx.std_dev_ms;
    
    pthread_mutex_unlock(&jitter_ctx.mutex);
    
    return JITTER_SUCCESS;
}

static double gaussian_random(double mean, double std_dev) {
    static int has_spare = 0;
    static double spare;
    
    if (has_spare) {
        has_spare = 0;
        return mean + std_dev * spare;
    }
    
    has_spare = 1;
    
    static const double two_pi = 2.0 * M_PI;
    double u, v, s;
    
    do {
        u = (rand() / ((double)RAND_MAX)) * 2.0 - 1.0;
        v = (rand() / ((double)RAND_MAX)) * 2.0 - 1.0;
        s = u * u + v * v;
    } while (s >= 1.0 || s == 0.0);
    
    s = sqrt(-2.0 * log(s) / s);
    
    spare = v * s;
    return mean + std_dev * u * s;
}

static double exponential_random(double lambda) {
    double u;
    do {
        u = ((double)rand() / RAND_MAX);
    } while (u == 0.0);
    
    return -log(u) / lambda;
}

static double uniform_random(double min, double max) {
    double u = ((double)rand() / RAND_MAX);
    return min + u * (max - min);
}