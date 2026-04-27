#ifndef PACKET_H
#define PACKET_H

#include "../../include/netlagx.h"
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

typedef struct {
    struct iphdr ip_header;
    union {
        struct tcphdr tcp_header;
        struct udphdr udp_header;
    } transport_header;
    uint8_t payload[MAX_PACKET_SIZE - sizeof(struct iphdr) - sizeof(struct tcphdr)];
} packet_data_t;

typedef enum {
    PACKET_TYPE_TCP = 6,
    PACKET_TYPE_UDP = 17,
    PACKET_TYPE_ICMP = 1,
    PACKET_TYPE_UNKNOWN = 0
} packet_type_t;

typedef struct {
    packet_type_t type;
    size_t header_size;
    size_t payload_size;
    uint16_t checksum;
    int is_fragmented;
    uint16_t fragment_offset;
} packet_info_t;

typedef enum {
    PACKET_SUCCESS = 0,
    PACKET_ERROR_INVALID = -1,
    PACKET_ERROR_PARSE = -2,
    PACKET_ERROR_CHECKSUM = -3,
    PACKET_ERROR_SIZE = -4
} packet_result_t;

int packet_init(void);
void packet_cleanup(void);

int packet_parse(const uint8_t* data, size_t size, packet_t* packet);
int packet_validate(const packet_t* packet);
int packet_get_info(const packet_t* packet, packet_info_t* info);

packet_type_t packet_get_type(const packet_t* packet);
uint16_t packet_calculate_checksum(const packet_t* packet);
int packet_verify_checksum(const packet_t* packet);

int packet_filter_target(const packet_t* packet, const char* target_ip, int target_port);
int packet_should_process(const packet_t* packet, lag_config_t* config);

int packet_create_response(const packet_t* original, packet_t* response, uint8_t* payload, size_t payload_size);
int packet_modify_header(packet_t* packet, const packet_info_t* new_info);

const char* packet_type_to_string(packet_type_t type);
const char* packet_get_source_ip(const packet_t* packet);
const char* packet_get_dest_ip(const packet_t* packet);
uint16_t packet_get_source_port(const packet_t* packet);
uint16_t packet_get_dest_port(const packet_t* packet);

#endif