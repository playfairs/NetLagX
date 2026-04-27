#include "../../../include/netlagx.h"

static int raw_socket = -1;
static delayed_packet_t* delayed_queue = NULL;
static pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;

int packet_intercept(lag_config_t* config) {
    struct sockaddr_in addr;
    char buffer[MAX_PACKET_SIZE];
    struct sockaddr_in from_addr;
    socklen_t from_len = sizeof(from_addr);
    
    raw_socket = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
    if (raw_socket < 0) {
        log_message("ERROR", "Failed to create raw socket: %s", strerror(errno));
        return -1;
    }
    
    int one = 1;
    if (setsockopt(raw_socket, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one)) < 0) {
        log_message("ERROR", "Failed to set IP_HDRINCL: %s", strerror(errno));
        close(raw_socket);
        return -1;
    }
    
    log_message("INFO", "Packet interception started");
    
    while (config->running) {
        ssize_t bytes_received = recvfrom(raw_socket, buffer, MAX_PACKET_SIZE, 0,
                                         (struct sockaddr*)&from_addr, &from_len);
        
        if (bytes_received < 0) {
            if (errno == EINTR) continue;
            log_message("ERROR", "Failed to receive packet: %s", strerror(errno));
            break;
        }
        
        packet_t packet;
        packet.size = bytes_received;
        memcpy(packet.data, buffer, bytes_received);
        packet.src_addr = from_addr;
        packet.timestamp = time(NULL);
        
        if (packet_process(&packet, config) < 0) {
            log_message("ERROR", "Failed to process packet");
        }
    }
    
    close(raw_socket);
    log_message("INFO", "Packet interception stopped");
    return 0;
}

int packet_process(packet_t* packet, lag_config_t* config) {
    if (!packet || !config) return -1;
    
    pthread_mutex_lock(&config->mutex);
    
    if (config->mode == MODE_OFF) {
        pthread_mutex_unlock(&config->mutex);
        return packet_send_delayed(NULL, config);
    }
    
    if (config->target_ip || config->target_port > 0) {
        struct ip* ip_header = (struct ip*)packet->data;
        struct sockaddr_in packet_dst;
        packet_dst.sin_addr = ip_header->ip_dst;
        
        if (config->target_ip && strcmp(inet_ntoa(packet_dst.sin_addr), config->target_ip) != 0) {
            pthread_mutex_unlock(&config->mutex);
            return packet_send_delayed(NULL, config);
        }
        
        if (config->target_port > 0) {
            struct tcphdr* tcp_header = (struct tcphdr*)(packet->data + (ip_header->ip_hl * 4));
            if (ntohs(tcp_header->th_dport) != config->target_port) {
                pthread_mutex_unlock(&config->mutex);
                return packet_send_delayed(NULL, config);
            }
        }
    }
    
    switch (config->mode) {
        case MODE_LAG:
            pthread_mutex_unlock(&config->mutex);
            return packet_add_to_delay_queue(packet, config);
            
        case MODE_DROP:
            if ((float)rand() / RAND_MAX < config->drop_rate) {
                log_message("DEBUG", "Dropping packet");
                pthread_mutex_unlock(&config->mutex);
                return 0;
            }
            pthread_mutex_unlock(&config->mutex);
            return packet_send_delayed(NULL, config);
            
        case MODE_THROTTLE:
            if ((float)rand() / RAND_MAX < config->throttle_rate) {
                pthread_mutex_unlock(&config->mutex);
                return packet_add_to_delay_queue(packet, config);
            }
            pthread_mutex_unlock(&config->mutex);
            return packet_send_delayed(NULL, config);
            
        default:
            pthread_mutex_unlock(&config->mutex);
            return packet_send_delayed(NULL, config);
    }
}

int packet_add_to_delay_queue(packet_t* packet, lag_config_t* config) {
    delayed_packet_t* delayed = malloc(sizeof(delayed_packet_t));
    if (!delayed) return -1;
    
    memcpy(&delayed->packet, packet, sizeof(packet_t));
    delayed->release_time = time(NULL) + (config->delay_ms / 1000);
    delayed->next = NULL;
    
    pthread_mutex_lock(&queue_mutex);
    
    if (!delayed_queue) {
        delayed_queue = delayed;
    } else {
        delayed_packet_t* current = delayed_queue;
        while (current->next) {
            current = current->next;
        }
        current->next = delayed;
    }
    
    pthread_mutex_unlock(&queue_mutex);
    
    return 0;
}

int packet_send_delayed(delayed_packet_t* delayed_packet, lag_config_t* config) {
    if (delayed_packet) {
        int raw_sock = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
        if (raw_sock < 0) {
            log_message("ERROR", "Failed to create socket for sending: %s", strerror(errno));
            return -1;
        }
        
        ssize_t sent = sendto(raw_sock, delayed_packet->packet.data, 
                             delayed_packet->packet.size, 0,
                             (struct sockaddr*)&delayed_packet->packet.dst_addr,
                             sizeof(delayed_packet->packet.dst_addr));
        
        close(raw_sock);
        
        if (sent < 0) {
            log_message("ERROR", "Failed to send packet: %s", strerror(errno));
            return -1;
        }
        
        free(delayed_packet);
        return 0;
    }
    
    pthread_mutex_lock(&queue_mutex);
    delayed_packet_t* current = delayed_queue;
    delayed_packet_t* prev = NULL;
    time_t now = time(NULL);
    
    while (current) {
        if (current->release_time <= now) {
            if (prev) {
                prev->next = current->next;
            } else {
                delayed_queue = current->next;
            }
            
            delayed_packet_t* to_send = current;
            current = current->next;
            
            pthread_mutex_unlock(&queue_mutex);
            packet_send_delayed(to_send, config);
            pthread_mutex_lock(&queue_mutex);
        } else {
            prev = current;
            current = current->next;
        }
    }
    
    pthread_mutex_unlock(&queue_mutex);
    return 0;
}