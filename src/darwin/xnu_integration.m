#import "xnu_integration.h"
#import <Foundation/Foundation.h>
#import <SystemConfiguration/SystemConfiguration.h>
#import <CoreFoundation/CoreFoundation.h>
#import <IOKit/IOKitLib.h>
#import <IOKit/network/IONetworkInterface.h>
#import <mach/mach.h>
#import <mach/mach_time.h>
#import <sys/sysctl.h>
#import <net/if.h>
#import <net/route.h>
#import <netinet/in.h>
#import <unistd.h>
#import <pthread.h>

static xnu_system_info_t g_system_info = {0};
static int g_system_info_initialized = 0;
static mach_port_t g_host_port = MACH_PORT_NULL;

static uint64_t g_packets_processed = 0;
static uint64_t g_bytes_processed = 0;
static pthread_mutex_t g_perf_mutex = PTHREAD_MUTEX_INITIALIZER;

static SCNetworkReachabilityRef g_reachability_ref = NULL;

static void xnu_init_system_info(void);
static uint64_t xnu_get_sysctl_int64(const char* name);
static uint32_t xnu_get_sysctl_int32(const char* name);
static NSString* xnu_get_sysctl_string(const char* name);

uint64_t xnu_get_absolute_time(void) {
    return mach_absolute_time();
}

uint64_t xnu_get_mach_time(void) {
    return mach_continuous_time();
}

uint64_t xnu_time_to_nanos(uint64_t mach_time) {
    if (!g_system_info_initialized) {
        xnu_init_system_info();
    }
    
    return mach_time * g_system_info.timebase.numer / g_system_info.timebase.denom;
}

uint64_t xnu_nanos_to_time(uint64_t nanos) {
    if (!g_system_info_initialized) {
        xnu_init_system_info();
    }
    
    return nanos * g_system_info.timebase.denom / g_system_info.timebase.numer;
}

void xnu_init_timebase(void) {
    mach_timebase_info(&g_system_info.timebase);
    g_system_info.start_time = mach_absolute_time();
}

static void xnu_init_system_info(void) {
    if (g_system_info_initialized) {
        return;
    }
    
    xnu_init_timebase();
    
    g_system_info.cpu_count = xnu_get_sysctl_int32("hw.ncpu");
    g_system_info.logical_cpu_count = xnu_get_sysctl_int32("hw.logicalcpu");
    
    g_system_info.physical_memory = xnu_get_sysctl_int64("hw.memsize");
    g_system_info.page_size = xnu_get_sysctl_int32("hw.pagesize");
    
    NSString* cpuModel = xnu_get_sysctl_string("hw.model");
    if (cpuModel) {
        strncpy(g_system_info.cpu_model, [cpuModel UTF8String], sizeof(g_system_info.cpu_model) - 1);
    }
    
    NSString* cpuVendor = xnu_get_sysctl_string("machdep.cpu.vendor");
    if (cpuVendor) {
        strncpy(g_system_info.cpu_vendor, [cpuVendor UTF8String], sizeof(g_system_info.cpu_vendor) - 1);
    }
    
    g_host_port = mach_host_self();
    
    g_system_info_initialized = 1;
}

xnu_result_t xnu_get_system_info(xnu_system_info_t* info) {
    if (!info) {
        return XNU_ERROR_INVALID;
    }
    
    if (!g_system_info_initialized) {
        xnu_init_system_info();
    }
    
    *info = g_system_info;
    
    vm_statistics_data_t vm_stats;
    mach_msg_type_number_t count = HOST_VM_INFO_COUNT;
    
    if (host_statistics(g_host_port, HOST_VM_INFO, (host_info_t)&vm_stats, &count) == KERN_SUCCESS) {
        g_system_info.available_memory = (uint64_t)vm_stats.free_count * g_system_info.page_size;
        info->available_memory = g_system_info.available_memory;
    }
    
    return XNU_SUCCESS;
}

xnu_result_t xnu_get_interface_list(xnu_interface_info_t** interfaces, size_t* count) {
    if (!interfaces || !count) {
        return XNU_ERROR_INVALID;
    }
    
    struct ifconf ifc;
    char buffer[8192];
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    
    if (sockfd < 0) {
        return XNU_ERROR_SYSTEM;
    }
    
    ifc.ifc_len = sizeof(buffer);
    ifc.ifc_buf = buffer;
    
    if (ioctl(sockfd, SIOCGIFCONF, &ifc) < 0) {
        close(sockfd);
        return XNU_ERROR_SYSTEM;
    }
    
    size_t if_count = 0;
    struct ifreq* ifr = (struct ifreq*)buffer;
    while ((char*)ifr < buffer + ifc.ifc_len) {
        if_count++;
        ifr = (struct ifreq*)((char*)ifr + sizeof(struct ifreq));
    }
    
    if (if_count == 0) {
        close(sockfd);
        *count = 0;
        *interfaces = NULL;
        return XNU_SUCCESS;
    }
    
    *interfaces = malloc(if_count * sizeof(xnu_interface_info_t));
    if (!*interfaces) {
        close(sockfd);
        return XNU_ERROR_SYSTEM;
    }
    
    size_t index = 0;
    ifr = (struct ifreq*)buffer;
    while ((char*)ifr < buffer + ifc.ifc_len) {
        xnu_interface_info_t* info = &(*interfaces)[index];
        memset(info, 0, sizeof(xnu_interface_info_t));
        
        strncpy(info->if_name, ifr->ifr_name, IFNAMSIZ - 1);
        
        info->if_index = if_nametoindex(ifr->ifr_name);
        
        if (ioctl(sockfd, SIOCGIFFLAGS, ifr) == 0) {
            info->if_flags = ifr->ifr_flags;
        }
        
        if (info->if_flags & IFF_RUNNING) {
            if (ioctl(sockfd, SIOCGIFHWADDR, ifr) == 0) {
                memcpy(info->if_mac, ifr->ifr_hwaddr.sa_data, 6);
            }
        }
        
        if (ioctl(sockfd, SIOCGIFMTU, ifr) == 0) {
            info->if_mtu = ifr->ifr_mtu;
        }
        
        index++;
        ifr = (struct ifreq*)((char*)ifr + sizeof(struct ifreq));
    }
    
    close(sockfd);
    *count = if_count;
    return XNU_SUCCESS;
}

xnu_result_t xnu_get_interface_info(const char* if_name, xnu_interface_info_t* info) {
    if (!if_name || !info) {
        return XNU_ERROR_INVALID;
    }
    
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        return XNU_ERROR_SYSTEM;
    }
    
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, if_name, IFNAMSIZ - 1);
    
    if (ioctl(sockfd, SIOCGIFFLAGS, &ifr) < 0) {
        close(sockfd);
        return XNU_ERROR_NOT_FOUND;
    }
    
    memset(info, 0, sizeof(xnu_interface_info_t));
    strncpy(info->if_name, if_name, IFNAMSIZ - 1);
    info->if_flags = ifr.ifr_flags;
    info->if_index = if_nametoindex(if_name);
    
    if (ioctl(sockfd, SIOCGIFHWADDR, &ifr) == 0) {
        memcpy(info->if_mac, ifr.ifr_hwaddr.sa_data, 6);
    }
    
    if (ioctl(sockfd, SIOCGIFMTU, &ifr) == 0) {
        info->if_mtu = ifr.ifr_mtu;
    }
    
    close(sockfd);
    return XNU_SUCCESS;
}

xnu_result_t xnu_get_interface_mac(const char* if_name, uint8_t* mac) {
    if (!if_name || !mac) {
        return XNU_ERROR_INVALID;
    }
    
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        return XNU_ERROR_SYSTEM;
    }
    
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, if_name, IFNAMSIZ - 1);
    
    if (ioctl(sockfd, SIOCGIFHWADDR, &ifr) < 0) {
        close(sockfd);
        return XNU_ERROR_NOT_FOUND;
    }
    
    memcpy(mac, ifr.ifr_hwaddr.sa_data, 6);
    close(sockfd);
    return XNU_SUCCESS;
}

xnu_result_t xnu_find_process_by_port(uint16_t port, pid_t* pid) {
    if (!pid) {
        return XNU_ERROR_INVALID;
    }
    
    NSString* lsofCommand = [NSString stringWithFormat:@"lsof -i :%d -nP -t", port];
    NSTask* task = [[NSTask alloc] init];
    [task setLaunchPath:@"/usr/sbin/lsof"];
    [task setArguments:@[@"-i", [NSString stringWithFormat:@":%d", port], @"-nP", @"-t"]];
    
    NSPipe* pipe = [NSPipe pipe];
    [task setStandardOutput:pipe];
    
    [task launch];
    [task waitUntilExit];
    
    NSData* data = [[pipe fileHandleForReading] readDataToEndOfFile];
    NSString* output = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
    
    if ([task terminationStatus] == 0 && [output length] > 0) {
        *pid = [output integerValue];
        return XNU_SUCCESS;
    }
    
    return XNU_ERROR_NOT_FOUND;
}

xnu_result_t xnu_sc_get_interface_names(NSArray** names) {
    if (!names) {
        return XNU_ERROR_INVALID;
    }
    
    SCDynamicStoreRef store = SCDynamicStoreCreate(NULL, CFSTR("netlagx"), NULL, NULL);
    if (!store) {
        return XNU_ERROR_SYSTEM;
    }
    
    CFArrayRef interfaces = SCDynamicStoreCopyKeyList(store, 
        CFSTR("State:/Network/Interface/.*"));
    
    if (!interfaces) {
        CFRelease(store);
        return XNU_ERROR_NOT_FOUND;
    }
    
    NSMutableArray* interfaceNames = [[NSMutableArray alloc] init];
    
    for (CFIndex i = 0; i < CFArrayGetCount(interfaces); i++) {
        CFStringRef key = CFArrayGetValueAtIndex(interfaces, i);
        CFStringRef interfaceName = CFStringCreateWithSubstring(NULL, key,
            CFRange(CFStringGetLength(key) - 1, 1));
        
        [interfaceNames addObject:(NSString*)interfaceName];
        CFRelease(interfaceName);
    }
    
    *names = [interfaceNames retain];
    
    CFRelease(interfaces);
    CFRelease(store);
    
    return XNU_SUCCESS;
}

xnu_result_t xnu_sc_get_interface_info(NSString* if_name, NSDictionary** info) {
    if (!if_name || !info) {
        return XNU_ERROR_INVALID;
    }
    
    SCDynamicStoreRef store = SCDynamicStoreCreate(NULL, CFSTR("netlagx"), NULL, NULL);
    if (!store) {
        return XNU_ERROR_SYSTEM;
    }
    
    CFStringRef key = CFStringCreateWithFormat(NULL, NULL, 
        CFSTR("State:/Network/Interface/%@"), (CFStringRef)if_name);
    
    CFDictionaryRef interfaceInfo = SCDynamicStoreCopyValue(store, key);
    
    if (!interfaceInfo) {
        CFRelease(key);
        CFRelease(store);
        return XNU_ERROR_NOT_FOUND;
    }
    
    *info = (NSDictionary*)interfaceInfo;
    
    CFRelease(key);
    CFRelease(store);
    
    return XNU_SUCCESS;
}

xnu_result_t xnu_io_get_network_interfaces(io_iterator_t* iterator) {
    if (!iterator) {
        return XNU_ERROR_INVALID;
    }
    
    mach_port_t master_port;
    kern_return_t kr = IOMasterPort(MACH_PORT_NULL, &master_port);
    if (kr != KERN_SUCCESS) {
        return XNU_ERROR_SYSTEM;
    }
    
    CFDictionaryRef matching = IOServiceMatching(kIONetworkInterfaceClass);
    if (!matching) {
        return XNU_ERROR_SYSTEM;
    }
    
    kr = IOServiceGetMatchingServices(master_port, matching, iterator);
    if (kr != KERN_SUCCESS) {
        return XNU_ERROR_SYSTEM;
    }
    
    return XNU_SUCCESS;
}

xnu_result_t xnu_io_get_interface_properties(io_service_t service, CFDictionaryRef* properties) {
    if (!properties) {
        return XNU_ERROR_INVALID;
    }
    
    kern_return_t kr = IORegistryEntryCreateCFProperty(service,
        CFSTR(kIOInterfaceName), kCFAllocatorDefault, 0, properties);
    
    if (kr != KERN_SUCCESS) {
        return XNU_ERROR_NOT_FOUND;
    }
    
    return XNU_SUCCESS;
}

xnu_result_t xnu_check_root_privileges(void) {
    return (geteuid() == 0) ? XNU_SUCCESS : XNU_ERROR_PERMISSION;
}

xnu_result_t xnu_set_realtime_priority(pthread_t thread) {
    struct sched_param param;
    param.sched_priority = 47;
    
    if (pthread_setschedparam(thread, SCHED_FIFO, &param) != 0) {
        return XNU_ERROR_PERMISSION;
    }
    
    return XNU_SUCCESS;
}

xnu_result_t xnu_get_perf_counters(uint64_t* packets_processed, uint64_t* bytes_processed) {
    if (!packets_processed || !bytes_processed) {
        return XNU_ERROR_INVALID;
    }
    
    pthread_mutex_lock(&g_perf_mutex);
    *packets_processed = g_packets_processed;
    *bytes_processed = g_bytes_processed;
    pthread_mutex_unlock(&g_perf_mutex);
    
    return XNU_SUCCESS;
}

xnu_result_t xnu_reset_perf_counters(void) {
    pthread_mutex_lock(&g_perf_mutex);
    g_packets_processed = 0;
    g_bytes_processed = 0;
    pthread_mutex_unlock(&g_perf_mutex);
    
    return XNU_SUCCESS;
}

static uint64_t xnu_get_sysctl_int64(const char* name) {
    uint64_t value = 0;
    size_t size = sizeof(value);
    
    if (sysctlbyname(name, &value, &size, NULL, 0) != 0) {
        return 0;
    }
    
    return value;
}

static uint32_t xnu_get_sysctl_int32(const char* name) {
    uint32_t value = 0;
    size_t size = sizeof(value);
    
    if (sysctlbyname(name, &value, &size, NULL, 0) != 0) {
        return 0;
    }
    
    return value;
}

static NSString* xnu_get_sysctl_string(const char* name) {
    size_t size = 0;
    
    if (sysctlbyname(name, NULL, &size, NULL, 0) != 0) {
        return nil;
    }
    
    char* buffer = malloc(size);
    if (!buffer) {
        return nil;
    }
    
    if (sysctlbyname(name, buffer, &size, NULL, 0) != 0) {
        free(buffer);
        return nil;
    }
    
    NSString* result = [NSString stringWithUTF8String:buffer];
    free(buffer);
    
    return result;
}

void xnu_optimize_for_darwin(void) {
    xnu_init_system_info();
    
    pthread_t current_thread = pthread_self();
    xnu_set_realtime_priority(current_thread);
    
    thread_affinity_policy_data_t affinity = {1};
    thread_policy_set(pthread_mach_thread_np(current_thread), 
                     THREAD_AFFINITY_POLICY, 
                     (thread_policy_t)&affinity, 
                     THREAD_AFFINITY_POLICY_COUNT);
}

xnu_result_t xnu_enable_low_latency_mode(void) {
    if (setpriority(PRIO_PROCESS, 0, -20) != 0) {
        return XNU_ERROR_PERMISSION;
    }
    
    size_t size = 0;
    if (sysctlbyname("hw.cpufamily", NULL, &size, NULL, 0) == 0) {
        int value = 1;
        sysctlbyname("kern.sched_affinity", NULL, &size, &value, sizeof(value));
    }
    
    return XNU_SUCCESS;
}
