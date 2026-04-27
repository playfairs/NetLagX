#ifndef OPTIMIZATIONS_H
#define OPTIMIZATIONS_H

#include <stdint.h>
#include <stddef.h>

extern uint16_t checksum_asm(const void* data, size_t length);
extern void* memcpy_asm(void* dest, const void* src, size_t n);
extern void* memset_asm(void* s, int c, size_t n);
extern uint64_t rdtsc_asm(void);
extern void cpuid_asm(uint32_t leaf, uint32_t* eax, uint32_t* ebx, uint32_t* ecx, uint32_t* edx);
extern void prefetch_asm(const void* addr);
extern void memory_barrier_asm(void);

extern int packet_filter_fast_asm(const void* packet, size_t size, const char* target_ip, uint16_t target_port);
extern void packet_process_batch_asm(void* packets, size_t count, size_t packet_size);

extern void precise_delay_ns_asm(uint64_t nanoseconds);
extern uint64_t get_timestamp_counter_asm(void);

extern uint32_t popcount_asm(uint32_t x);
extern uint32_t find_first_set_asm(uint32_t x);
extern uint32_t find_last_set_asm(uint32_t x);

extern size_t strlen_asm(const char* str);
extern int strcmp_asm(const char* s1, const char* s2);
extern char* strchr_asm(const char* s, int c);

typedef struct {
    int has_sse2;
    int has_sse4_1;
    int has_avx;
    int has_avx2;
    int has_rdtscp;
    int has_invariant_tsc;
    char cpu_vendor[16];
    char cpu_model[64];
} cpu_features_t;

extern void detect_cpu_features_asm(cpu_features_t* features);
extern int is_x86_64_asm(void);

typedef struct {
    uint64_t cycles_packet_process;
    uint64_t cycles_checksum;
    uint64_t cycles_memcpy;
    uint64_t cycles_memset;
    uint64_t packets_processed;
    uint64_t bytes_processed;
} perf_counters_t;

extern void reset_perf_counters_asm(perf_counters_t* counters);
extern void read_perf_counters_asm(perf_counters_t* counters);

#endif
