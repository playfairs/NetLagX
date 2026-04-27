#include "../../include/netlagx.h"

static lag_config_t global_config;
static volatile sig_atomic_t running = 1;

void signal_handler(int sig) {
    log_message("INFO", "Received signal %d, shutting down...", sig);
    running = 0;
    global_config.running = 0;
}

void print_usage(const char* program_name) {
    printf("NetLagX - Network Lag Switch Tool\n");
    printf("Usage: %s [options]\n\n", program_name);
    printf("Options:\n");
    printf("  -g, --gui          Run GUI interface (default: CLI)\n");
    printf("  -i, --ip <IP>      Target IP address\n");
    printf("  -p, --port <PORT>  Target port\n");
    printf("  -d, --delay <MS>   Delay in milliseconds (default: 100)\n");
    printf("  -m, --mode <MODE>  Mode: off, lag, drop, throttle (default: lag)\n");
    printf("  -r, --drop-rate <RATE>  Packet drop rate 0.0-1.0 (default: 0.1)\n");
    printf("  -t, --throttle-rate <RATE>  Throttle rate 0.0-1.0 (default: 0.5)\n");
    printf("  -h, --help         Show this help message\n\n");
    printf("Examples:\n");
    printf("  %s -i 192.168.1.100 -p 80 -d 200 -m lag\n", program_name);
    printf("  %s -g  # Run GUI mode\n", program_name);
}

int main(int argc, char* argv[]) {
    int use_gui = 0;
    
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    config_load_defaults(&global_config);
    
    if (argc > 1) {
        if (strcmp(argv[1], "-g") == 0 || strcmp(argv[1], "--gui") == 0) {
            use_gui = 1;
        } else if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
    }
    
    if (use_gui) {
        return gui_run(argc, argv);
    } else {
        return cli_run(argc, argv);
    }
}