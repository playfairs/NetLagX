#ifndef SOCKET_H
#define SOCKET_H

#include "../../include/netlagx.h"
#include <sys/socket.h>
#include <netinet/in.h>

typedef struct {
    int socket_fd;
    struct sockaddr_in local_addr;
    struct sockaddr_in remote_addr;
    int is_bound;
    int is_connected;
    int is_nonblocking;
    char description[64];
} socket_context_t;

typedef enum {
    SOCKET_SUCCESS = 0,
    SOCKET_ERROR_CREATE = -1,
    SOCKET_ERROR_BIND = -2,
    SOCKET_ERROR_CONNECT = -3,
    SOCKET_ERROR_LISTEN = -4,
    SOCKET_ERROR_SEND = -5,
    SOCKET_ERROR_RECV = -6,
    SOCKET_ERROR_OPTION = -7,
    SOCKET_ERROR_INVALID = -8
} socket_result_t;

int socket_init(void);
void socket_cleanup(void);

int socket_create_raw(int protocol);
int socket_create_tcp_server(int port, int max_connections);
int socket_create_tcp_client(const char* host, int port);
int socket_create_udp_server(int port);

int socket_set_nonblocking(int sock);
int socket_set_tcp_options(int sock, int no_delay, int keep_alive);
int socket_set_buffer_size(int sock, int recv_size, int send_size);

ssize_t socket_send_packet(int sock, const void* data, size_t size, const struct sockaddr_in* dest_addr);
ssize_t socket_receive_packet(int sock, void* buffer, size_t buffer_size, struct sockaddr_in* src_addr);

int socket_get_local_address(int sock, struct sockaddr_in* addr);
int socket_get_remote_address(int sock, struct sockaddr_in* addr);
const char* socket_address_to_string(const struct sockaddr_in* addr, char* buffer, size_t buffer_size);

int socket_accept_connection(int server_sock, struct sockaddr_in* client_addr);
int socket_close(int sock);

int socket_select(int max_fd, fd_set* read_fds, fd_set* write_fds, fd_set* except_fds, struct timeval* timeout);

#endif