#include "../asm/optimizations.h"
#include "../darwin/xnu_integration.h"
#include "../utils/logger.h"
#include "../utils/time.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define TEST_SIZE (1024 * 1024)
#define TEST_ITERATIONS 1000

static void test_assembly_optimizations(void) {
    printf("assembly optimizations test\n\n");
    
    init_optimizations();
    
    printf("testing memory ops.\n");
    uint8_t* src = malloc(TEST_SIZE);
    uint8_t* dst = malloc(TEST_SIZE);
    
    if (!src || !dst) {
        printf("memory allocation failed\n");
        return;
    }
    
    for (size_t i = 0; i < TEST_SIZE; i++) {
        src[i] = (uint8_t)(i & 0xFF);
    }
    
    uint64_t start_time = get_timestamp_optimized();
    for (int i = 0; i < TEST_ITERATIONS; i++) {
        memcpy_optimized(dst, src, TEST_SIZE);
    }
    uint64_t end_time = get_timestamp_optimized();
    
    printf("Optimized memcpy: %lu cycles for %d iterations of %d bytes\n",
           end_time - start_time, TEST_ITERATIONS, TEST_SIZE);
    
    int errors = 0;
    for (size_t i = 0; i < TEST_SIZE; i++) {
        if (dst[i] != src[i]) {
            errors++;
        }
    }
    printf("Data integrity: %s (%d errors)\n", errors == 0 ? "PASS" : "FAIL", errors);
    
    start_time = get_timestamp_optimized();
    for (int i = 0; i < TEST_ITERATIONS; i++) {
        memset_optimized(dst, 0xAA, TEST_SIZE);
    }
    end_time = get_timestamp_optimized();
    
    printf("Optimized memset: %lu cycles for %d iterations of %d bytes\n",
           end_time - start_time, TEST_ITERATIONS, TEST_SIZE);
    
    start_time = get_timestamp_optimized();
    uint16_t checksum = 0;
    for (int i = 0; i < TEST_ITERATIONS; i++) {
        checksum = checksum_optimized(src, TEST_SIZE);
    }
    end_time = get_timestamp_optimized();
    
    printf("Optimized checksum: %lu cycles for %d iterations (checksum: 0x%04X)\n",
           end_time - start_time, TEST_ITERATIONS, checksum);
    
    start_time = get_timestamp_optimized();
    uint32_t total_bits = 0;
    for (int i = 0; i < TEST_ITERATIONS * 1000; i++) {
        total_bits += popcount_optimized(i);
    }
    end_time = get_timestamp_optimized();
    
    printf("Optimized popcount: %lu cycles for %d iterations (total bits: %u)\n",
           end_time - start_time, TEST_ITERATIONS * 1000, total_bits);
    
    perf_counters_t counters;
    get_performance_counters(&counters);
    printf("\nPerformance Counters:\n");
    printf("  Packets processed: %lu\n", counters.packets_processed);
    printf("  Bytes processed: %lu\n", counters.bytes_processed);
    printf("  Checksum cycles: %lu\n", counters.cycles_checksum);
    printf("  Memcpy cycles: %lu\n", counters.cycles_memcpy);
    printf("  Memset cycles: %lu\n", counters.cycles_memset);
    
    free(src);
    free(dst);
}

static void test_darwin_integration(void) {
    printf("\nXNU/Darwin Integration\n\n");
    
#ifdef PLATFORM_DARWIN
    xnu_system_info_t sys_info;
    xnu_result_t result = xnu_get_system_info(&sys_info);
    
    if (result == XNU_SUCCESS) {
        printf("System Information:\n");
        printf("  CPU Model: %s\n", sys_info.cpu_model);
        printf("  CPU Vendor: %s\n", sys_info.cpu_vendor);
        printf("  CPU Count: %u (logical: %u)\n", sys_info.cpu_count, sys_info.logical_cpu_count);
        printf("  Physical Memory: %lu MB\n", sys_info.physical_memory / (1024 * 1024));
        printf("  Available Memory: %lu MB\n", sys_info.available_memory / (1024 * 1024));
        printf("  Page Size: %u bytes\n", sys_info.page_size);
        printf("  Timebase: %lu/%lu\n", sys_info.timebase.numer, sys_info.timebase.denom);
    } else {
        printf("Failed to get system information: %d\n", result);
    }
    
    uint64_t start = xnu_get_absolute_time();
    usleep(1000);
    uint64_t end = xnu_get_absolute_time();
    uint64_t elapsed_ns = xnu_time_to_nanos(end - start);
    
    printf("\nHigh-Precision Timing:\n");
    printf("  Measured delay: %lu ns\n", elapsed_ns);
    
    xnu_interface_info_t* interfaces;
    size_t interface_count;
    
    result = xnu_get_interface_list(&interfaces, &interface_count);
    if (result == XNU_SUCCESS) {
        printf("\nNetwork Interfaces (%zu):\n", interface_count);
        for (size_t i = 0; i < interface_count; i++) {
            xnu_interface_info_t* iface = &interfaces[i];
            printf("  %s: index=%u, mtu=%u, flags=0x%08x\n",
                   iface->if_name, iface->if_index, iface->if_mtu, iface->if_flags);
            
            if (iface->if_flags & IFF_RUNNING) {
                printf("    MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                       iface->if_mac[0], iface->if_mac[1], iface->if_mac[2],
                       iface->if_mac[3], iface->if_mac[4], iface->if_mac[5]);
            }
        }
        free(interfaces);
    } else {
        printf("Failed to get interface list: %d\n", result);
    }
    
    result = xnu_check_root_privileges();
    printf("\nroot privileges: %s\n", result == XNU_SUCCESS ? "Yes" : "No");
    
    xnu_optimize_for_darwin();
    printf("Darwin optimizations enabled\n");
    
    result = xnu_enable_low_latency_mode();
    printf("Low-latency mode: %s\n", result == XNU_SUCCESS ? "Enabled" : "Failed");
    
#else
    printf("Darwin integration not available on this platform\n");
#endif
}

static void test_cpu_features(void) {
    printf("\nCPU Features Test\n\n");
    
    if (is_x86_64_platform()) {
        printf("Platform: x86_64\n");
        
        printf("CPU Features:\n");
        printf("  SSE2: %s\n", has_sse2() ? "Yes" : "No");
        printf("  SSE4.1: %s\n", has_sse4_1() ? "Yes" : "No");
        printf("  AVX: %s\n", has_avx() ? "Yes" : "No");
        printf("  AVX2: %s\n", has_avx2() ? "Yes" : "No");
        printf("  RDTSCP: %s\n", has_rdtscp() ? "Yes" : "No");
        printf("  Invariant TSC: %s\n", has_invariant_tsc() ? "Yes" : "No");
        
        printf("CPU Info:\n");
        printf("  Vendor: %s\n", get_cpu_vendor());
        printf("  Model: %s\n", get_cpu_model());
        
        uint32_t eax, ebx, ecx, edx;
        get_cpuid_info(0, &eax, &ebx, &ecx, &edx);
        printf("  CPUID.0: EAX=0x%08X, EBX=0x%08X, ECX=0x%08X, EDX=0x%08X\n", eax, ebx, ecx, edx);
    } else {
        printf("Platform: Not x86_64\n");
    }
}

static void test_string_operations(void) {
    printf("\nstring ops test\n\n");
    
    const char* test_string = "NetLagX Performance Test String";
    size_t len = strlen_optimized(test_string);
    printf("String length test: \"%s\" -> %zu characters\n", test_string, len);
    
    const char* str1 = "NetLagX";
    const char* str2 = "NetLagX";
    const char* str3 = "Different";
    
    int cmp1 = strcmp_optimized(str1, str2);
    int cmp2 = strcmp_optimized(str1, str3);
    
    printf("String comparison:\n");
    printf("  \"%s\" vs \"%s\" = %d\n", str1, str2, cmp1);
    printf("  \"%s\" vs \"%s\" = %d\n", str1, str3, cmp2);
    
    char* found = strchr_optimized(test_string, 'P');
    if (found) {
        printf("Character search: 'P' found at position %ld\n", found - test_string);
    } else {
        printf("Character search: 'P' not found\n");
    }
}

int main(int argc, char* argv[]) {
    printf("NetLagX Performance Integration Test\n\n");
    
    logger_init(LOG_LEVEL_INFO, LOG_OUTPUT_CONSOLE, NULL, 0, 0, 0, 0);
    
    test_cpu_features();
    test_assembly_optimizations();
    test_darwin_integration();
    test_string_operations();
    
    printf("\ndone\n");
    
    cleanup_optimizations();
    logger_cleanup();
    
    return 0;
}
