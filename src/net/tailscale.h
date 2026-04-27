#ifndef TAILSCALE_H
#define TAILSCALE_H

#include "../../include/netlagx.h"
#include <time.h>

#define TAILSCALE_API_BASE "https://api.tailscale.com"
#define TAILSCALE_MAX_DEVICES 100
#define TAILSCALE_MAX_ROUTES 50

typedef struct {
    char id[64];
    char name[256];
    char addresses[16][64];
    int address_count;
    char os[64];
    char tags[256];
    char hostname[256];
    int is_online;
    time_t last_seen;
    char machine_key[128];
    char node_key[128];
    char user[128];
    char tailscale_ip[64];
    char public_ip[64];
    char location[128];
    char version[64];
} tailscale_device_t;

typedef struct {
    char prefix[64];
    char node[64];
    char advertised_route[64];
    char enabled[32];
    char next_hop[64];
    char description[256];
} tailscale_route_t;

typedef struct {
    char id[64];
    char name[256];
    char dns_names[16][256];
    int dns_name_count;
    char tags[256];
    char description[512];
    char acl[1024];
    char created_at[64];
    char expires_at[64];
} tailscale_tag_t;

typedef struct {
    char api_key[512];
    char tailnet[256];
    char base_url[512];
    int timeout_seconds;
    int verify_ssl;
    
    tailscale_device_t devices[TAILSCALE_MAX_DEVICES];
    int device_count;
    
    tailscale_route_t routes[TAILSCALE_MAX_ROUTES];
    int route_count;
    
    char last_error[512];
    int authenticated;
    time_t last_refresh;
    int auto_refresh;
    int refresh_interval;
    
    pthread_mutex_t mutex;
    int initialized;
} tailscale_context_t;

typedef enum {
    TAILSCALE_SUCCESS = 0,
    TAILSCALE_ERROR_INIT = -1,
    TAILSCALE_ERROR_AUTH = -2,
    TAILSCALE_ERROR_NETWORK = -3,
    TAILSCALE_ERROR_PARSE = -4,
    TAILSCALE_ERROR_CONFIG = -5,
    TAILSCALE_ERROR_NOT_FOUND = -6
} tailscale_result_t;

int tailscale_init(const char* api_key, const char* tailnet);
void tailscale_cleanup(void);

int tailscale_authenticate(const char* api_key);
int tailscale_refresh_devices(void);
int tailscale_refresh_routes(void);
int tailscale_refresh_status(void);

tailscale_device_t* tailscale_find_device_by_name(const char* name);
tailscale_device_t* tailscale_find_device_by_ip(const char* ip);
tailscale_device_t* tailscale_find_device_by_hostname(const char* hostname);

int tailscale_get_device_list(tailscale_device_t** devices, int* count);
int tailscale_get_route_list(tailscale_route_t** routes, int* count);

int tailscale_set_target_by_device(lag_config_t* config, const char* device_name);
int tailscale_set_target_by_ip(lag_config_t* config, const char* ip);

int tailscale_list_devices(void);
int tailscale_list_routes(void);
int tailscale_device_status(const char* device_name);

int tailscale_enable_device_routing(const char* device_name);
int tailscale_disable_device_routing(const char* device_name);
int tailscale_advertise_route(const char* prefix, const char* node);

const char* tailscale_get_error(void);
int tailscale_is_authenticated(void);
int tailscale_get_device_count(void);

void tailscale_set_auto_refresh(int enabled, int interval_seconds);
int tailscale_auto_refresh_check(void);

#endif
