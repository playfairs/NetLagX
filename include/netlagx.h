#ifndef NETLAGX_H
#define NETLAGX_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>
#include <errno.h>

#define MAX_PACKET_SIZE 65535
#define DEFAULT_DELAY_MS 100
#define MAX_DELAY_MS 5000
#define BUFFER_SIZE 1024

typedef enum {
    MODE_OFF = 0,
    MODE_LAG = 1,
    MODE_DROP = 2,
    MODE_THROTTLE = 3
} lag_mode_t;

typedef struct {
    lag_mode_t mode;
    int delay_ms;
    float drop_rate;
    float throttle_rate;
    int target_port;
    char* target_ip;
    int running;
    pthread_mutex_t mutex;
} lag_config_t;

typedef struct {
    char data[MAX_PACKET_SIZE];
    size_t size;
    struct sockaddr_in src_addr;
    struct sockaddr_in dst_addr;
    time_t timestamp;
} packet_t;

typedef struct {
    packet_t packet;
    struct delayed_packet* next;
    time_t release_time;
} delayed_packet_t;

int netlagx_init(lag_config_t* config);
int netlagx_start(lag_config_t* config);
int netlagx_stop(lag_config_t* config);
void netlagx_cleanup(lag_config_t* config);

int packet_intercept(lag_config_t* config);
int packet_process(packet_t* packet, lag_config_t* config);
int packet_add_to_delay_queue(packet_t* packet, lag_config_t* config);
int packet_send_delayed(delayed_packet_t* delayed_packet, lag_config_t* config);

int cli_run(int argc, char* argv[]);
int gui_run(int argc, char* argv[]);

void config_load_defaults(lag_config_t* config);
void config_print(lag_config_t* config);
int config_parse_args(lag_config_t* config, int argc, char* argv[]);

void log_message(const char* level, const char* format, ...);

#endif