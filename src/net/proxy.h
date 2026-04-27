#ifndef PROXY_H
#define PROXY_H

#include "../../include/netlagx.h"
#include <sys/socket.h>
#include <netinet/in.h>

typedef enum {
    PROXY_MODE_NONE = 0,
    PROXY_MODE_FORWARD = 1,
    PROXY_MODE_REVERSE = 2,
    PROXY_MODE_TRANSPARENT = 3
} proxy_mode_t;

typedef struct {
    proxy_mode_t mode;
    char* upstream_host;
    int upstream_port;
    char* downstream_host;
    int downstream_port;
    int listen_port;
    
    int server_socket;
    int upstream_socket;
    int* client_sockets;
    int max_clients;
    int num_clients;
    
    struct sockaddr_in upstream_addr;
    struct sockaddr_in downstream_addr;
    
    pthread_t proxy_thread;
    pthread_mutex_t mutex;
    int running;
    int initialized;
} proxy_context_t;

typedef struct {
    int client_socket;
    int upstream_socket;
    struct sockaddr_in client_addr;
    struct sockaddr_in upstream_addr;
    time_t connect_time;
    size_t bytes_sent;
    size_t bytes_received;
    int active;
} proxy_connection_t;

typedef enum {
    PROXY_SUCCESS = 0,
    PROXY_ERROR_INIT = -1,
    PROXY_ERROR_BIND = -2,
    PROXY_ERROR_CONNECT = -3,
    PROXY_ERROR_THREAD = -4,
    PROXY_ERROR_CONFIG = -5,
    PROXY_ERROR_MEMORY = -6
} proxy_result_t;

int proxy_init(proxy_context_t* ctx);
void proxy_cleanup(proxy_context_t* ctx);

int proxy_start(proxy_context_t* ctx);
int proxy_stop(proxy_context_t* ctx);

int proxy_set_mode(proxy_context_t* ctx, proxy_mode_t mode);
int proxy_set_upstream(proxy_context_t* ctx, const char* host, int port);
int proxy_set_downstream(proxy_context_t* ctx, const char* host, int port);
int proxy_set_listen_port(proxy_context_t* ctx, int port);

int proxy_add_client(proxy_context_t* ctx, int client_socket);
int proxy_remove_client(proxy_context_t* ctx, int client_socket);

int proxy_forward_packet(proxy_context_t* ctx, const void* data, size_t size, int from_socket);
int proxy_handle_connection(proxy_context_t* ctx, int client_socket);

void* proxy_worker_thread(void* arg);
void* proxy_connection_handler(void* arg);

void proxy_get_stats(proxy_context_t* ctx, size_t* total_connections, size_t* active_connections, 
                    size_t* bytes_forwarded);

#endif