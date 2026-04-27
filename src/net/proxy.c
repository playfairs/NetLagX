#include "proxy.h"
#include "socket.h"
#include "../utils/logger.h"
#include "../utils/time.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>

#define MAX_PROXY_CLIENTS 100

static proxy_context_t* global_proxy_ctx = NULL;

int proxy_init(proxy_context_t* ctx) {
    if (!ctx) {
        return PROXY_ERROR_CONFIG;
    }
    
    memset(ctx, 0, sizeof(proxy_context_t));
    
    ctx->mode = PROXY_MODE_NONE;
    ctx->listen_port = 8080;
    ctx->max_clients = MAX_PROXY_CLIENTS;
    ctx->server_socket = -1;
    ctx->upstream_socket = -1;
    
    ctx->client_sockets = malloc(sizeof(int) * ctx->max_clients);
    if (!ctx->client_sockets) {
        return PROXY_ERROR_MEMORY;
    }
    
    for (int i = 0; i < ctx->max_clients; i++) {
        ctx->client_sockets[i] = -1;
    }
    
    if (pthread_mutex_init(&ctx->mutex, NULL) != 0) {
        free(ctx->client_sockets);
        return PROXY_ERROR_INIT;
    }
    
    ctx->initialized = 1;
    global_proxy_ctx = ctx;
    
    logger_info("Proxy module initialized");
    return PROXY_SUCCESS;
}

void proxy_cleanup(proxy_context_t* ctx) {
    if (!ctx || !ctx->initialized) {
        return;
    }
    
    if (ctx->running) {
        proxy_stop(ctx);
    }
    
    if (ctx->server_socket >= 0) {
        close(ctx->server_socket);
    }
    
    if (ctx->upstream_socket >= 0) {
        close(ctx->upstream_socket);
    }
    
    for (int i = 0; i < ctx->max_clients; i++) {
        if (ctx->client_sockets[i] >= 0) {
            close(ctx->client_sockets[i]);
        }
    }
    
    if (ctx->upstream_host) {
        free(ctx->upstream_host);
    }
    
    if (ctx->downstream_host) {
        free(ctx->downstream_host);
    }
    
    free(ctx->client_sockets);
    pthread_mutex_destroy(&ctx->mutex);
    
    memset(ctx, 0, sizeof(proxy_context_t));
    global_proxy_ctx = NULL;
    
    logger_info("Proxy module cleaned up");
}

int proxy_start(proxy_context_t* ctx) {
    if (!ctx || !ctx->initialized) {
        return PROXY_ERROR_CONFIG;
    }
    
    if (ctx->running) {
        logger_warning("Proxy is already running");
        return PROXY_SUCCESS;
    }
    
    if (ctx->mode == PROXY_MODE_NONE) {
        logger_error("Cannot start proxy: mode not set");
        return PROXY_ERROR_CONFIG;
    }
    
    ctx->server_socket = socket_create_tcp_server(ctx->listen_port, ctx->max_clients);
    if (ctx->server_socket < 0) {
        logger_error("Failed to create proxy server socket");
        return PROXY_ERROR_BIND;
    }
    
    if (ctx->mode == PROXY_MODE_FORWARD || ctx->mode == PROXY_MODE_REVERSE) {
        if (!ctx->upstream_host || ctx->upstream_port <= 0) {
            logger_error("Upstream host/port not configured for proxy mode");
            return PROXY_ERROR_CONFIG;
        }
        
        ctx->upstream_socket = socket_create_tcp_client(ctx->upstream_host, ctx->upstream_port);
        if (ctx->upstream_socket < 0) {
            logger_error("Failed to connect to upstream server");
            return PROXY_ERROR_CONNECT;
        }
    }
    
    ctx->running = 1;
    
    if (pthread_create(&ctx->proxy_thread, NULL, proxy_worker_thread, ctx) != 0) {
        ctx->running = 0;
        logger_error("Failed to create proxy worker thread");
        return PROXY_ERROR_THREAD;
    }
    
    logger_info("Proxy started on port %d (mode: %d)", ctx->listen_port, ctx->mode);
    return PROXY_SUCCESS;
}

int proxy_stop(proxy_context_t* ctx) {
    if (!ctx || !ctx->running) {
        return PROXY_SUCCESS;
    }
    
    logger_info("Stopping proxy...");
    
    ctx->running = 0;
    
    pthread_join(ctx->proxy_thread, NULL);
    
    if (ctx->server_socket >= 0) {
        close(ctx->server_socket);
        ctx->server_socket = -1;
    }
    
    if (ctx->upstream_socket >= 0) {
        close(ctx->upstream_socket);
        ctx->upstream_socket = -1;
    }
    
    for (int i = 0; i < ctx->max_clients; i++) {
        if (ctx->client_sockets[i] >= 0) {
            close(ctx->client_sockets[i]);
            ctx->client_sockets[i] = -1;
        }
    }
    
    ctx->num_clients = 0;
    
    logger_info("Proxy stopped");
    return PROXY_SUCCESS;
}

int proxy_set_mode(proxy_context_t* ctx, proxy_mode_t mode) {
    if (!ctx || !ctx->initialized) {
        return PROXY_ERROR_CONFIG;
    }
    
    pthread_mutex_lock(&ctx->mutex);
    ctx->mode = mode;
    pthread_mutex_unlock(&ctx->mutex);
    
    logger_info("Proxy mode set to %d", mode);
    return PROXY_SUCCESS;
}

int proxy_set_upstream(proxy_context_t* ctx, const char* host, int port) {
    if (!ctx || !ctx->initialized || !host || port <= 0) {
        return PROXY_ERROR_CONFIG;
    }
    
    pthread_mutex_lock(&ctx->mutex);
    
    if (ctx->upstream_host) {
        free(ctx->upstream_host);
    }
    
    ctx->upstream_host = strdup(host);
    ctx->upstream_port = port;
    
    pthread_mutex_unlock(&ctx->mutex);
    
    logger_info("Proxy upstream set to %s:%d", host, port);
    return PROXY_SUCCESS;
}

int proxy_set_downstream(proxy_context_t* ctx, const char* host, int port) {
    if (!ctx || !ctx->initialized || !host || port <= 0) {
        return PROXY_ERROR_CONFIG;
    }
    
    pthread_mutex_lock(&ctx->mutex);
    
    if (ctx->downstream_host) {
        free(ctx->downstream_host);
    }
    
    ctx->downstream_host = strdup(host);
    ctx->downstream_port = port;
    
    pthread_mutex_unlock(&ctx->mutex);
    
    logger_info("Proxy downstream set to %s:%d", host, port);
    return PROXY_SUCCESS;
}

int proxy_set_listen_port(proxy_context_t* ctx, int port) {
    if (!ctx || !ctx->initialized || port <= 0 || port > 65535) {
        return PROXY_ERROR_CONFIG;
    }
    
    pthread_mutex_lock(&ctx->mutex);
    ctx->listen_port = port;
    pthread_mutex_unlock(&ctx->mutex);
    
    logger_info("Proxy listen port set to %d", port);
    return PROXY_SUCCESS;
}

int proxy_add_client(proxy_context_t* ctx, int client_socket) {
    if (!ctx || client_socket < 0) {
        return PROXY_ERROR_CONFIG;
    }
    
    pthread_mutex_lock(&ctx->mutex);
    
    int slot = -1;
    for (int i = 0; i < ctx->max_clients; i++) {
        if (ctx->client_sockets[i] < 0) {
            slot = i;
            break;
        }
    }
    
    if (slot < 0) {
        pthread_mutex_unlock(&ctx->mutex);
        logger_warning("Proxy client limit reached");
        return PROXY_ERROR_MEMORY;
    }
    
    ctx->client_sockets[slot] = client_socket;
    ctx->num_clients++;
    
    pthread_mutex_unlock(&ctx->mutex);
    
    logger_info("Added proxy client (socket: %d, total: %d)", client_socket, ctx->num_clients);
    return slot;
}

int proxy_remove_client(proxy_context_t* ctx, int client_socket) {
    if (!ctx || client_socket < 0) {
        return PROXY_ERROR_CONFIG;
    }
    
    pthread_mutex_lock(&ctx->mutex);
    
    for (int i = 0; i < ctx->max_clients; i++) {
        if (ctx->client_sockets[i] == client_socket) {
            close(ctx->client_sockets[i]);
            ctx->client_sockets[i] = -1;
            ctx->num_clients--;
            
            pthread_mutex_unlock(&ctx->mutex);
            logger_info("Removed proxy client (socket: %d, total: %d)", client_socket, ctx->num_clients);
            return i;
        }
    }
    
    pthread_mutex_unlock(&ctx->mutex);
    return PROXY_ERROR_CONFIG;
}

int proxy_forward_packet(proxy_context_t* ctx, const void* data, size_t size, int from_socket) {
    if (!ctx || !data || size == 0) {
        return PROXY_ERROR_CONFIG;
    }
    
    ssize_t sent = 0;
    
    pthread_mutex_lock(&ctx->mutex);
    
    if (from_socket == ctx->server_socket) {
        if (ctx->upstream_socket >= 0) {
            sent = socket_send_packet(ctx->upstream_socket, data, size, NULL);
        }
    } else if (from_socket == ctx->upstream_socket) {
        for (int i = 0; i < ctx->max_clients; i++) {
            if (ctx->client_sockets[i] >= 0 && ctx->client_sockets[i] != from_socket) {
                sent = socket_send_packet(ctx->client_sockets[i], data, size, NULL);
                break;
            }
        }
    } else {
        for (int i = 0; i < ctx->max_clients; i++) {
            if (ctx->client_sockets[i] >= 0 && ctx->client_sockets[i] != from_socket) {
                sent = socket_send_packet(ctx->client_sockets[i], data, size, NULL);
            }
        }
    }
    
    pthread_mutex_unlock(&ctx->mutex);
    
    return (sent > 0) ? PROXY_SUCCESS : PROXY_ERROR_CONFIG;
}

int proxy_handle_connection(proxy_context_t* ctx, int client_socket) {
    if (!ctx || client_socket < 0) {
        return PROXY_ERROR_CONFIG;
    }
    
    proxy_connection_t* conn = malloc(sizeof(proxy_connection_t));
    if (!conn) {
        return PROXY_ERROR_MEMORY;
    }
    
    memset(conn, 0, sizeof(proxy_connection_t));
    conn->client_socket = client_socket;
    conn->connect_time = time_get_current();
    conn->active = 1;
    
    if (socket_get_remote_address(client_socket, &conn->client_addr) != SOCKET_SUCCESS) {
        free(conn);
        return PROXY_ERROR_CONFIG;
    }
    
    if (ctx->upstream_socket >= 0) {
        memcpy(&conn->upstream_addr, &ctx->upstream_addr, sizeof(conn->upstream_addr));
    }
    
    pthread_t handler_thread;
    if (pthread_create(&handler_thread, NULL, proxy_connection_handler, conn) != 0) {
        free(conn);
        return PROXY_ERROR_THREAD;
    }
    
    pthread_detach(handler_thread);
    
    return PROXY_SUCCESS;
}

void* proxy_worker_thread(void* arg) {
    proxy_context_t* ctx = (proxy_context_t*)arg;
    fd_set read_fds;
    struct timeval timeout;
    int max_fd;
    
    logger_info("Proxy worker thread started");
    
    while (ctx->running) {
        FD_ZERO(&read_fds);
        FD_SET(ctx->server_socket, &read_fds);
        max_fd = ctx->server_socket;
        
        pthread_mutex_lock(&ctx->mutex);
        for (int i = 0; i < ctx->max_clients; i++) {
            if (ctx->client_sockets[i] >= 0) {
                FD_SET(ctx->client_sockets[i], &read_fds);
                if (ctx->client_sockets[i] > max_fd) {
                    max_fd = ctx->client_sockets[i];
                }
            }
        }
        
        if (ctx->upstream_socket >= 0) {
            FD_SET(ctx->upstream_socket, &read_fds);
            if (ctx->upstream_socket > max_fd) {
                max_fd = ctx->upstream_socket;
            }
        }
        pthread_mutex_unlock(&ctx->mutex);
        
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int activity = select(max_fd + 1, &read_fds, NULL, NULL, &timeout);
        
        if (activity < 0) {
            if (errno == EINTR) continue;
            logger_error("Proxy select error: %s", strerror(errno));
            break;
        }
        
        if (activity == 0) continue;
        
        if (FD_ISSET(ctx->server_socket, &read_fds)) {
            struct sockaddr_in client_addr;
            int client_sock = socket_accept_connection(ctx->server_socket, &client_addr);
            
            if (client_sock > 0) {
                if (proxy_add_client(ctx, client_sock) >= 0) {
                    proxy_handle_connection(ctx, client_sock);
                } else {
                    close(client_sock);
                }
            }
        }
        
        pthread_mutex_lock(&ctx->mutex);
        for (int i = 0; i < ctx->max_clients; i++) {
            if (ctx->client_sockets[i] >= 0 && FD_ISSET(ctx->client_sockets[i], &read_fds)) {
                char buffer[4096];
                ssize_t received = socket_receive_packet(ctx->client_sockets[i], buffer, sizeof(buffer), NULL);
                
                if (received <= 0) {
                    proxy_remove_client(ctx, ctx->client_sockets[i]);
                } else {
                    proxy_forward_packet(ctx, buffer, received, ctx->client_sockets[i]);
                }
            }
        }
        pthread_mutex_unlock(&ctx->mutex);
    }
    
    logger_info("Proxy worker thread stopped");
    return NULL;
}

void* proxy_connection_handler(void* arg) {
    proxy_connection_t* conn = (proxy_connection_t*)arg;
    
    if (!conn) {
        return NULL;
    }
    
    char buffer[4096];
    ssize_t received;
    
    while (conn->active && global_proxy_ctx && global_proxy_ctx->running) {
        received = socket_receive_packet(conn->client_socket, buffer, sizeof(buffer), NULL);
        
        if (received <= 0) {
            break;
        }
        
        conn->bytes_received += received;
        
        if (global_proxy_ctx->upstream_socket >= 0) {
            ssize_t sent = socket_send_packet(global_proxy_ctx->upstream_socket, buffer, received, NULL);
            if (sent > 0) {
                conn->bytes_sent += sent;
            }
        }
    }
    
    if (global_proxy_ctx) {
        proxy_remove_client(global_proxy_ctx, conn->client_socket);
    }
    
    free(conn);
    return NULL;
}

void proxy_get_stats(proxy_context_t* ctx, size_t* total_connections, size_t* active_connections, 
                    size_t* bytes_forwarded) {
    if (!ctx) return;
    
    pthread_mutex_lock(&ctx->mutex);
    
    if (total_connections) *total_connections = ctx->num_clients;
    if (active_connections) *active_connections = ctx->num_clients;
    if (bytes_forwarded) *bytes_forwarded = 0;
    
    pthread_mutex_unlock(&ctx->mutex);
}