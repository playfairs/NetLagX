#include "optimizations.h"
#include "../utils/logger.h"
#include <string.h>
#include <stdio.h>
#include <unistd.h>

extern uint16_t checksum_asm(const void* data, size_t length);
extern void* memcpy_asm(void* dest, const void* src, size_t n);
extern void* memset_asm(void* s, int c, size_t n);
extern uint64_t rdtsc_asm(void);
extern void cpuid_asm(uint32_t leaf, uint32_t* eax, uint32_t* ebx, uint32_t* ecx, uint32_t* edx);
extern void prefetch_asm(const void* addr);
extern void memory_barrier_asm(void);
extern uint64_t get_timestamp_counter_asm(void);
extern uint32_t popcount_asm(uint32_t x);
extern uint32_t find_first_set_asm(uint32_t x);
extern uint32_t find_last_set_asm(uint32_t x);
extern size_t strlen_asm(const char* str);
extern int strcmp_asm(const char* s1, const char* s2);
extern char* strchr_asm(const char* s, int c);
extern void detect_cpu_features_asm(cpu_features_t* features);
extern int is_x86_64_asm(void);
extern void reset_perf_counters_asm(perf_counters_t* counters);
extern void read_perf_counters_asm(perf_counters_t* counters);

static cpu_features_t g_cpu_features = {0};
static int g_cpu_features_detected = 0;

static perf_counters_t g_perf_counters = {0};

static void init_cpu_features(void) {
    if (g_cpu_features_detected) {
        return;
    }
    
    detect_cpu_features_asm(&g_cpu_features);
    g_cpu_features_detected = 1;
    
    logger_info("CPU Features detected: SSE2=%s, SSE4.1=%s, AVX=%s, AVX2=%s, RDTSCP=%s, InvariantTSC=%s",
                g_cpu_features.has_sse2 ? "Yes" : "No",
                g_cpu_features.has_sse4_1 ? "Yes" : "No", 
                g_cpu_features.has_avx ? "Yes" : "No",
                g_cpu_features.has_avx2 ? "Yes" : "No",
                g_cpu_features.has_rdtscp ? "Yes" : "No",
                g_cpu_features.has_invariant_tsc ? "Yes" : "No");
    
    logger_info("CPU: %s %s", g_cpu_features.cpu_vendor, g_cpu_features.cpu_model);
}

uint16_t checksum_optimized(const void* data, size_t length) {
    if (!data || length == 0) {
        return 0;
    }
    
    init_cpu_features();
    
    uint64_t start_time = get_timestamp_counter_asm();
    uint16_t result = checksum_asm(data, length);
    uint64_t end_time = get_timestamp_counter_asm();
    
    __sync_fetch_and_add(&g_perf_counters.cycles_checksum, end_time - start_time);
    __sync_fetch_and_add(&g_perf_counters.bytes_processed, length);
    
    return result;
}

void* memcpy_optimized(void* dest, const void* src, size_t n) {
    if (!dest || !src || n == 0) {
        return dest;
    }
    
    init_cpu_features();
    
    uint64_t start_time = get_timestamp_counter_asm();
    void* result = memcpy_asm(dest, src, n);
    uint64_t end_time = get_timestamp_counter_asm();
    
    __sync_fetch_and_add(&g_perf_counters.cycles_memcpy, end_time - start_time);
    __sync_fetch_and_add(&g_perf_counters.bytes_processed, n);
    
    return result;
}

void* memset_optimized(void* s, int c, size_t n) {
    if (!s || n == 0) {
        return s;
    }
    
    init_cpu_features();
    
    uint64_t start_time = get_timestamp_counter_asm();
    void* result = memset_asm(s, c, n);
    uint64_t end_time = get_timestamp_counter_asm();
    
    __sync_fetch_and_add(&g_perf_counters.cycles_memset, end_time - start_time);
    __sync_fetch_and_add(&g_perf_counters.bytes_processed, n);
    
    return result;
}

uint64_t get_timestamp_optimized(void) {
    init_cpu_features();
    return get_timestamp_counter_asm();
}

int has_sse2(void) {
    init_cpu_features();
    return g_cpu_features.has_sse2;
}

int has_sse4_1(void) {
    init_cpu_features();
    return g_cpu_features.has_sse4_1;
}

int has_avx(void) {
    init_cpu_features();
    return g_cpu_features.has_avx;
}

int has_avx2(void) {
    init_cpu_features();
    return g_cpu_features.has_avx2;
}

int has_rdtscp(void) {
    init_cpu_features();
    return g_cpu_features.has_rdtscp;
}

int has_invariant_tsc(void) {
    init_cpu_features();
    return g_cpu_features.has_invariant_tsc;
}

const char* get_cpu_vendor(void) {
    init_cpu_features();
    return g_cpu_features.cpu_vendor;
}

const char* get_cpu_model(void) {
    init_cpu_features();
    return g_cpu_features.cpu_model;
}

uint32_t popcount_optimized(uint32_t x) {
    return popcount_asm(x);
}

uint32_t find_first_set_optimized(uint32_t x) {
    return find_first_set_asm(x);
}

uint32_t find_last_set_optimized(uint32_t x) {
    return find_last_set_asm(x);
}

size_t strlen_optimized(const char* str) {
    if (!str) {
        return 0;
    }
    return strlen_asm(str);
}

int strcmp_optimized(const char* s1, const char* s2) {
    if (!s1 || !s2) {
        return s1 ? 1 : (s2 ? -1 : 0);
    }
    return strcmp_asm(s1, s2);
}

char* strchr_optimized(const char* s, int c) {
    if (!s) {
        return NULL;
    }
    return strchr_asm(s, c);
}

void prefetch_data(const void* addr) {
    if (addr) {
        prefetch_asm(addr);
    }
}

void memory_barrier(void) {
    memory_barrier_asm();
}

void reset_performance_counters(void) {
    reset_perf_counters_asm(&g_perf_counters);
}

void get_performance_counters(perf_counters_t* counters) {
    if (counters) {
        read_perf_counters_asm(&g_perf_counters);
        counters->packets_processed = g_perf_counters.packets_processed;
        counters->bytes_processed = g_perf_counters.bytes_processed;
    }
}

void increment_packet_counter(void) {
    __sync_fetch_and_add(&g_perf_counters.packets_processed, 1);
}

int is_x86_64_platform(void) {
    return is_x86_64_asm();
}

void get_cpuid_info(uint32_t leaf, uint32_t* eax, uint32_t* ebx, uint32_t* ecx, uint32_t* edx) {
    if (eax && ebx && ecx && edx) {
        cpuid_asm(leaf, eax, ebx, ecx, edx);
    }
}

int packet_filter_fast(const void* packet, size_t size, const char* target_ip, uint16_t target_port) {
    // this would be implemented in assembly for maximum performance
    // for now, we'll just provide a much more simple implementation
    if (!packet || !target_ip || size < sizeof(struct iphdr)) {
        return 0;
    }
    
    struct iphdr* ip = (struct iphdr*)packet;
    
    if (ip->version != 4) {
        return 0;
    }
    
    if (ip->protocol != IPPROTO_TCP && ip->protocol != IPPROTO_UDP) {
        return 0;
    }
    
    char src_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &ip->saddr, src_ip, sizeof(src_ip));
    
    if (strcmp(src_ip, target_ip) != 0) {
        return 0;
    }
    
    if (target_port != 0) {
        uint16_t src_port = 0;
        
        if (ip->protocol == IPPROTO_TCP) {
            struct tcphdr* tcp = (struct tcphdr*)((uint8_t*)ip + (ip->ihl * 4));
            src_port = ntohs(tcp->source);
        } else if (ip->protocol == IPPROTO_UDP) {
            struct udphdr* udp = (struct udphdr*)((uint8_t*)ip + (ip->ihl * 4));
            src_port = ntohs(udp->source);
        }
        
        if (src_port != target_port) {
            return 0;
        }
    }
    
    return 1;
}

void process_packet_batch(void* packets, size_t count, size_t packet_size) {
    if (!packets || count == 0 || packet_size == 0) {
        return;
    }
    
    init_cpu_features();
    
    uint64_t start_time = get_timestamp_counter_asm();
    
    for (size_t i = 0; i < count; i++) {
        void* packet = (uint8_t*)packets + (i * packet_size);
        
        if (i + 1 < count) {
            prefetch_data((uint8_t*)packets + ((i + 1) * packet_size));
        }
        
        increment_packet_counter();
    }
    
    uint64_t end_time = get_timestamp_counter_asm();
    __sync_fetch_and_add(&g_perf_counters.cycles_packet_process, end_time - start_time);
}

void init_optimizations(void) {
    init_cpu_features();
    reset_performance_counters();
    
    logger_info("Assembly optimizations initialized");
    logger_info("Platform: %s", is_x86_64_platform() ? "x86_64" : "Other");
}

void cleanup_optimizations(void) {
    g_cpu_features_detected = 0;
    memset(&g_cpu_features, 0, sizeof(g_cpu_features));
    memset(&g_perf_counters, 0, sizeof(g_perf_counters));
    
    logger_info("Assembly optimizations cleaned up");
}
