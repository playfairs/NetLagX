#ifndef XNU_INTEGRATION_H
#define XNU_INTEGRATION_H

#include <stdint.h>
#include <sys/types.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <sys/sysctl.h>
#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>

#ifdef __OBJC__
#import <Foundation/Foundation.h>
#import <SystemConfiguration/SystemConfiguration.h>
#import <CoreFoundation/CoreFoundation.h>
#import <IOKit/IOKitLib.h>
#import <IOKit/network/IONetworkInterface.h>
#endif

typedef struct {
    uint32_t if_index;
    char if_name[IFNAMSIZ];
    uint8_t if_mac[6];
    uint32_t if_mtu;
    uint32_t if_flags;
    uint64_t if_ibytes;
    uint64_t if_obytes;
    uint64_t if_ipackets;
    uint64_t if_opackets;
    uint64_t if_ierrors;
    uint64_t if_oerrors;
    uint64_t if_collisions;
} xnu_interface_info_t;

typedef struct {
    pid_t pid;
    char comm[MAXCOMLEN + 1];
    uid_t uid;
    gid_t gid;
    uint64_t rx_bytes;
    uint64_t tx_bytes;
    uint32_t socket_count;
    uint32_t tcp_connections;
    uint32_t udp_connections;
    uint32_t last_activity;
} xnu_process_network_info_t;

typedef struct {
    mach_timebase_info_data_t timebase;
    uint64_t start_time;
    uint64_t cpu_frequency;
    uint32_t cpu_count;
    uint32_t logical_cpu_count;
    char cpu_model[64];
    char cpu_vendor[16];
    uint64_t physical_memory;
    uint64_t available_memory;
    uint32_t page_size;
} xnu_system_info_t;

typedef enum {
    XNU_SUCCESS = 0,
    XNU_ERROR_PERMISSION = -1,
    XNU_ERROR_NOT_FOUND = -2,
    XNU_ERROR_INVALID = -3,
    XNU_ERROR_SYSTEM = -4
} xnu_result_t;

extern uint64_t xnu_get_absolute_time(void);
extern uint64_t xnu_get_mach_time(void);
extern uint64_t xnu_time_to_nanos(uint64_t mach_time);
extern uint64_t xnu_nanos_to_time(uint64_t nanos);
extern void xnu_init_timebase(void);

extern xnu_result_t xnu_get_interface_list(xnu_interface_info_t** interfaces, size_t* count);
extern xnu_result_t xnu_get_interface_info(const char* if_name, xnu_interface_info_t* info);
extern xnu_result_t xnu_set_interface_promiscuous(const char* if_name, int enable);
extern xnu_result_t xnu_get_interface_mac(const char* if_name, uint8_t* mac);

extern xnu_result_t xnu_get_process_network_info(pid_t pid, xnu_process_network_info_t* info);
extern xnu_result_t xnu_get_all_process_network_info(xnu_process_network_info_t** processes, size_t* count);
extern xnu_result_t xnu_find_process_by_port(uint16_t port, pid_t* pid);

extern xnu_result_t xnu_get_system_info(xnu_system_info_t* info);
extern xnu_result_t xnu_get_cpu_usage(double* usage_percent);
extern xnu_result_t xnu_get_memory_usage(uint64_t* used, uint64_t* available);
extern xnu_result_t xnu_get_network_stats(uint64_t* rx_bytes, uint64_t* tx_bytes);

extern int xnu_create_raw_socket_optimized(int protocol);
extern xnu_result_t xnu_bind_raw_socket_to_interface(int sockfd, const char* if_name);
extern xnu_result_t xnu_set_socket_buffer_size(int sockfd, size_t size);
extern xnu_result_t xnu_enable_packet_timestamps(int sockfd);

extern int xnu_bpf_device_open(void);
extern xnu_result_t xnu_bpf_attach_program(int bpf_fd, const struct bpf_program* program);
extern xnu_result_t xnu_bpf_set_buffer_size(int bpf_fd, size_t size);
extern xnu_result_t xnu_bpf_read_packet(int bpf_fd, void* buffer, size_t* len);

#ifdef __OBJC__
extern xnu_result_t xnu_sc_get_interface_names(NSArray** names);
extern xnu_result_t xnu_sc_get_interface_info(NSString* if_name, NSDictionary** info);
extern xnu_result_t xnu_sc_monitor_network_changes(void (^callback)(NSString* key, NSDictionary* info));

extern xnu_result_t xnu_io_get_network_interfaces(io_iterator_t* iterator);
extern xnu_result_t xnu_io_get_interface_properties(io_service_t service, CFDictionaryRef* properties);
extern xnu_result_t xnu_io_set_interface_property(io_service_t service, CFStringRef key, CFTypeRef value);
#endif

extern xnu_result_t xnu_enable_kernel_packet_filter(int enable);
extern xnu_result_t xnu_set_kernel_thread_priority(pthread_t thread, int priority);
extern xnu_result_t xnu_bind_thread_to_cpu(pthread_t thread, int cpu_id);

extern xnu_result_t xnu_check_root_privileges(void);
extern xnu_result_t xnu_request_privileges(void);
extern xnu_result_t xnu_drop_privileges(void);

extern xnu_result_t xnu_get_perf_counters(uint64_t* packets_processed, uint64_t* bytes_processed);
extern xnu_result_t xnu_reset_perf_counters(void);
extern xnu_result_t xnu_start_perf_monitoring(void);
extern xnu_result_t xnu_stop_perf_monitoring(void);

extern void xnu_optimize_for_darwin(void);
extern xnu_result_t xnu_enable_low_latency_mode(void);
extern xnu_result_t xnu_set_realtime_priority(pthread_t thread);

#endif
