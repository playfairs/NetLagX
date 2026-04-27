#include "../../../include/netlagx.h"

typedef struct {
    time_t timestamp;
    size_t size;
    struct traffic_stats* next;
} traffic_stats_t;

static traffic_stats_t* stats_head = NULL;
static pthread_mutex_t stats_mutex = PTHREAD_MUTEX_INITIALIZER;
static size_t total_bytes = 0;
static size_t total_packets = 0;
static time_t start_time = 0;

void traffic_stats_init() {
    pthread_mutex_lock(&stats_mutex);
    stats_head = NULL;
    total_bytes = 0;
    total_packets = 0;
    start_time = time(NULL);
    pthread_mutex_unlock(&stats_mutex);
}

void traffic_stats_add_packet(size_t packet_size) {
    pthread_mutex_lock(&stats_mutex);
    
    traffic_stats_t* stat = malloc(sizeof(traffic_stats_t));
    if (stat) {
        stat->timestamp = time(NULL);
        stat->size = packet_size;
        stat->next = stats_head;
        stats_head = stat;
        
        total_bytes += packet_size;
        total_packets++;
    }
    
    pthread_mutex_unlock(&stats_mutex);
}

void traffic_stats_cleanup() {
    pthread_mutex_lock(&stats_mutex);
    
    traffic_stats_t* current = stats_head;
    while (current) {
        traffic_stats_t* next = current->next;
        free(current);
        current = next;
    }
    stats_head = NULL;
    
    pthread_mutex_unlock(&stats_mutex);
}

void traffic_stats_print() {
    pthread_mutex_lock(&stats_mutex);
    
    time_t now = time(NULL);
    double runtime = difftime(now, start_time);
    
    printf("=== Traffic Statistics ===\n");
    printf("Runtime: %.1f seconds\n", runtime);
    printf("Total Packets: %zu\n", total_packets);
    printf("Total Bytes: %zu\n", total_bytes);
    
    if (runtime > 0) {
        printf("Average Packet Rate: %.2f packets/sec\n", total_packets / runtime);
        printf("Average Throughput: %.2f KB/sec\n", (total_bytes / 1024.0) / runtime);
    }
    
    pthread_mutex_unlock(&stats_mutex);
}

int traffic_apply_shaping(packet_t* packet, lag_config_t* config) {
    if (!packet || !config) return -1;
    
    traffic_stats_add_packet(packet->size);
    
    switch (config->mode) {
        case MODE_THROTTLE:
            if (config->throttle_rate > 0.0 && config->throttle_rate < 1.0) {
                time_t now = time(NULL);
                static time_t last_send_time = 0;
                static int packets_in_window = 0;
                const int window_size = 1;
                const int max_packets_per_window = (int)(10.0 * config->throttle_rate);
                
                if (now - last_send_time >= window_size) {
                    last_send_time = now;
                    packets_in_window = 0;
                }
                
                if (packets_in_window >= max_packets_per_window) {
                    struct timespec delay = {0, (1000000000L / max_packets_per_window)};
                    nanosleep(&delay, NULL);
                }
                
                packets_in_window++;
            }
            break;
            
        default:
            break;
    }
    
    return 0;
}
