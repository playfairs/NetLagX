#include "socket.h"
#include "../utils/logger.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

static socket_context_t server_ctx = {0};
static socket_context_t client_ctx = {0};
static int socket_module_initialized = 0;

int socket_init(void) {
    if (socket_module_initialized) {
        return SOCKET_SUCCESS;
    }
    
    memset(&server_ctx, 0, sizeof(server_ctx));
    memset(&client_ctx, 0, sizeof(client_ctx));
    
    server_ctx.socket_fd = -1;
    client_ctx.socket_fd = -1;
    
    socket_module_initialized = 1;
    logger_info("Socket module initialized");
    return SOCKET_SUCCESS;
}

void socket_cleanup(void) {
    if (!socket_module_initialized) {
        return;
    }
    
    if (server_ctx.socket_fd >= 0) {
        close(server_ctx.socket_fd);
    }
    if (client_ctx.socket_fd >= 0) {
        close(client_ctx.socket_fd);
    }
    
    memset(&server_ctx, 0, sizeof(server_ctx));
    memset(&client_ctx, 0, sizeof(client_ctx));
    
    socket_module_initialized = 0;
    logger_info("Socket module cleaned up");
}

int socket_create_raw(int protocol) {
    int sock = socket(AF_INET, SOCK_RAW, protocol);
    if (sock < 0) {
        logger_error("Failed to create raw socket: %s", strerror(errno));
        return SOCKET_ERROR_CREATE;
    }
    
    int one = 1;
    if (setsockopt(sock, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one)) < 0) {
        logger_error("Failed to set IP_HDRINCL: %s", strerror(errno));
        close(sock);
        return SOCKET_ERROR_OPTION;
    }
    
    if (socket_set_buffer_size(sock, MAX_PACKET_SIZE * 2, MAX_PACKET_SIZE * 2) != SOCKET_SUCCESS) {
        logger_warning("Failed to set socket buffer sizes");
    }
    
    logger_debug("Created raw socket with protocol %d", protocol);
    return sock;
}

int socket_create_tcp_server(int port, int max_connections) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        logger_error("Failed to create TCP server socket: %s", strerror(errno));
        return SOCKET_ERROR_CREATE;
    }
    
    int opt = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        logger_warning("Failed to set SO_REUSEADDR: %s", strerror(errno));
    }
    
    server_ctx.local_addr.sin_family = AF_INET;
    server_ctx.local_addr.sin_addr.s_addr = INADDR_ANY;
    server_ctx.local_addr.sin_port = htons(port);
    
    if (bind(sock, (struct sockaddr*)&server_ctx.local_addr, sizeof(server_ctx.local_addr)) < 0) {
        logger_error("Failed to bind TCP server socket to port %d: %s", port, strerror(errno));
        close(sock);
        return SOCKET_ERROR_BIND;
    }
    
    if (listen(sock, max_connections) < 0) {
        logger_error("Failed to listen on TCP server socket: %s", strerror(errno));
        close(sock);
        return SOCKET_ERROR_LISTEN;
    }
    
    server_ctx.socket_fd = sock;
    server_ctx.is_bound = 1;
    snprintf(server_ctx.description, sizeof(server_ctx.description), "TCP Server:%d", port);
    
    logger_info("TCP server listening on port %d (max connections: %d)", port, max_connections);
    return sock;
}

int socket_create_tcp_client(const char* host, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        logger_error("Failed to create TCP client socket: %s", strerror(errno));
        return SOCKET_ERROR_CREATE;
    }
    
    struct hostent* he = gethostbyname(host);
    if (!he) {
        logger_error("Failed to resolve hostname %s: %s", host, hstrerror(h_errno));
        close(sock);
        return SOCKET_ERROR_CONNECT;
    }
    
    client_ctx.remote_addr.sin_family = AF_INET;
    client_ctx.remote_addr.sin_port = htons(port);
    memcpy(&client_ctx.remote_addr.sin_addr, he->h_addr_list[0], he->h_length);
    
    if (connect(sock, (struct sockaddr*)&client_ctx.remote_addr, sizeof(client_ctx.remote_addr)) < 0) {
        logger_error("Failed to connect to %s:%d: %s", host, port, strerror(errno));
        close(sock);
        return SOCKET_ERROR_CONNECT;
    }
    
    client_ctx.socket_fd = sock;
    client_ctx.is_connected = 1;
    snprintf(client_ctx.description, sizeof(client_ctx.description), "TCP Client:%s:%d", host, port);
    
    logger_info("TCP client connected to %s:%d", host, port);
    return sock;
}

int socket_create_udp_server(int port) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        logger_error("Failed to create UDP server socket: %s", strerror(errno));
        return SOCKET_ERROR_CREATE;
    }
    
    int opt = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        logger_warning("Failed to set SO_REUSEADDR: %s", strerror(errno));
    }
    
    server_ctx.local_addr.sin_family = AF_INET;
    server_ctx.local_addr.sin_addr.s_addr = INADDR_ANY;
    server_ctx.local_addr.sin_port = htons(port);
    
    if (bind(sock, (struct sockaddr*)&server_ctx.local_addr, sizeof(server_ctx.local_addr)) < 0) {
        logger_error("Failed to bind UDP server socket to port %d: %s", port, strerror(errno));
        close(sock);
        return SOCKET_ERROR_BIND;
    }
    
    server_ctx.socket_fd = sock;
    server_ctx.is_bound = 1;
    snprintf(server_ctx.description, sizeof(server_ctx.description), "UDP Server:%d", port);
    
    logger_info("UDP server listening on port %d", port);
    return sock;
}

int socket_set_nonblocking(int sock) {
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0) {
        logger_error("Failed to get socket flags: %s", strerror(errno));
        return SOCKET_ERROR_OPTION;
    }
    
    if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) < 0) {
        logger_error("Failed to set non-blocking mode: %s", strerror(errno));
        return SOCKET_ERROR_OPTION;
    }
    
    logger_debug("Socket %d set to non-blocking mode", sock);
    return SOCKET_SUCCESS;
}

int socket_set_tcp_options(int sock, int no_delay, int keep_alive) {
    if (no_delay) {
        int flag = 1;
        if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag)) < 0) {
            logger_warning("Failed to set TCP_NODELAY: %s", strerror(errno));
            return SOCKET_ERROR_OPTION;
        }
    }
    
    if (keep_alive) {
        int flag = 1;
        if (setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &flag, sizeof(flag)) < 0) {
            logger_warning("Failed to set SO_KEEPALIVE: %s", strerror(errno));
            return SOCKET_ERROR_OPTION;
        }
        
        int keepalive_time = 60;
        int keepalive_intvl = 10;
        int keepalive_probes = 3;
        
        #ifdef TCP_KEEPIDLE
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, &keepalive_time, sizeof(keepalive_time));
        #endif
        #ifdef TCP_KEEPINTVL
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, &keepalive_intvl, sizeof(keepalive_intvl));
        #endif
        #ifdef TCP_KEEPCNT
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, &keepalive_probes, sizeof(keepalive_probes));
        #endif
    }
    
    return SOCKET_SUCCESS;
}

int socket_set_buffer_size(int sock, int recv_size, int send_size) {
    if (recv_size > 0) {
        if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &recv_size, sizeof(recv_size)) < 0) {
            logger_warning("Failed to set receive buffer size: %s", strerror(errno));
            return SOCKET_ERROR_OPTION;
        }
    }
    
    if (send_size > 0) {
        if (setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &send_size, sizeof(send_size)) < 0) {
            logger_warning("Failed to set send buffer size: %s", strerror(errno));
            return SOCKET_ERROR_OPTION;
        }
    }
    
    return SOCKET_SUCCESS;
}

ssize_t socket_send_packet(int sock, const void* data, size_t size, const struct sockaddr_in* dest_addr) {
    ssize_t sent;
    socklen_t addr_len = dest_addr ? sizeof(*dest_addr) : 0;
    
    sent = sendto(sock, data, size, 0, (const struct sockaddr*)dest_addr, addr_len);
    if (sent < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;
        }
        logger_error("Failed to send packet: %s", strerror(errno));
        return SOCKET_ERROR_SEND;
    }
    
    if (sent != (ssize_t)size) {
        logger_warning("Partial send: %zd of %zu bytes", sent, size);
    }
    
    return sent;
}

ssize_t socket_receive_packet(int sock, void* buffer, size_t buffer_size, struct sockaddr_in* src_addr) {
    ssize_t received;
    socklen_t addr_len = src_addr ? sizeof(*src_addr) : 0;
    
    received = recvfrom(sock, buffer, buffer_size, 0, (struct sockaddr*)src_addr, &addr_len);
    if (received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;
        }
        logger_error("Failed to receive packet: %s", strerror(errno));
        return SOCKET_ERROR_RECV;
    }
    
    return received;
}

int socket_get_local_address(int sock, struct sockaddr_in* addr) {
    socklen_t addr_len = sizeof(*addr);
    if (getsockname(sock, (struct sockaddr*)addr, &addr_len) < 0) {
        logger_error("Failed to get local address: %s", strerror(errno));
        return SOCKET_ERROR_INVALID;
    }
    return SOCKET_SUCCESS;
}

int socket_get_remote_address(int sock, struct sockaddr_in* addr) {
    socklen_t addr_len = sizeof(*addr);
    if (getpeername(sock, (struct sockaddr*)addr, &addr_len) < 0) {
        logger_error("Failed to get remote address: %s", strerror(errno));
        return SOCKET_ERROR_INVALID;
    }
    return SOCKET_SUCCESS;
}

const char* socket_address_to_string(const struct sockaddr_in* addr, char* buffer, size_t buffer_size) {
    if (!addr || !buffer) return "invalid";
    
    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr->sin_addr, ip_str, INET_ADDRSTRLEN);
    
    snprintf(buffer, buffer_size, "%s:%d", ip_str, ntohs(addr->sin_port));
    return buffer;
}

int socket_accept_connection(int server_sock, struct sockaddr_in* client_addr) {
    socklen_t addr_len = sizeof(*client_addr);
    int client_sock = accept(server_sock, (struct sockaddr*)client_addr, &addr_len);
    
    if (client_sock < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;
        }
        logger_error("Failed to accept connection: %s", strerror(errno));
        return SOCKET_ERROR_CONNECT;
    }
    
    char client_str[32];
    socket_address_to_string(client_addr, client_str, sizeof(client_str));
    logger_info("Accepted connection from %s", client_str);
    
    return client_sock;
}

int socket_close(int sock) {
    if (sock < 0) {
        return SOCKET_SUCCESS;
    }
    
    if (close(sock) < 0) {
        logger_error("Failed to close socket %d: %s", sock, strerror(errno));
        return SOCKET_ERROR_INVALID;
    }
    
    logger_debug("Socket %d closed", sock);
    return SOCKET_SUCCESS;
}

int socket_select(int max_fd, fd_set* read_fds, fd_set* write_fds, fd_set* except_fds, struct timeval* timeout) {
    int result = select(max_fd + 1, read_fds, write_fds, except_fds, timeout);
    
    if (result < 0) {
        if (errno == EINTR) {
            return 0;
        }
        logger_error("select() failed: %s", strerror(errno));
        return SOCKET_ERROR_INVALID;
    }
    
    return result;
}