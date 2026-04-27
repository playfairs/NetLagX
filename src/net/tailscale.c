#include "tailscale.h"
#include "../utils/logger.h"
#include "../utils/time.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <curl/curl.h>
#include <json-c/json.h>

static tailscale_context_t tailscale_ctx = {0};

struct memory_struct {
    char *memory;
    size_t size;
};

static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct memory_struct *mem = (struct memory_struct *)userp;
    
    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if(!ptr) {
        return 0;
    }
    
    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;
    
    return realsize;
}

static int tailscale_request(const char* endpoint, const char* method, 
                           const char* data, char** response) {
    CURL *curl;
    CURLcode res;
    struct memory_struct chunk;
    char url[1024];
    long response_code;
    
    if (!tailscale_ctx.initialized || !tailscale_ctx.authenticated) {
        strcpy(tailscale_ctx.last_error, "Not initialized or authenticated");
        return TAILSCALE_ERROR_AUTH;
    }
    
    curl = curl_easy_init();
    if (!curl) {
        strcpy(tailscale_ctx.last_error, "Failed to initialize CURL");
        return TAILSCALE_ERROR_INIT;
    }
    
    chunk.memory = malloc(1);
    chunk.size = 0;
    
    snprintf(url, sizeof(url), "%s%s", tailscale_ctx.base_url, endpoint);
    
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    
    char auth_header[1024];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", tailscale_ctx.api_key);
    headers = curl_slist_append(headers, auth_header);
    
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "NetLagX/1.0");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, tailscale_ctx.timeout_seconds);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    
    if (strcmp(method, "POST") == 0 && data) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
    } else if (strcmp(method, "DELETE") == 0) {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    }
    
    if (!tailscale_ctx.verify_ssl) {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    }
    
    res = curl_easy_perform(curl);
    
    if (res != CURLE_OK) {
        snprintf(tailscale_ctx.last_error, sizeof(tailscale_ctx.last_error),
                "CURL error: %s", curl_easy_strerror(res));
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        free(chunk.memory);
        return TAILSCALE_ERROR_NETWORK;
    }
    
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    if (response_code >= 400) {
        snprintf(tailscale_ctx.last_error, sizeof(tailscale_ctx.last_error),
                "HTTP error: %ld", response_code);
        free(chunk.memory);
        return TAILSCALE_ERROR_NETWORK;
    }
    
    if (response) {
        *response = chunk.memory;
    } else {
        free(chunk.memory);
    }
    
    return TAILSCALE_SUCCESS;
}

static int parse_device_json(json_object* device_obj, tailscale_device_t* device) {
    json_object *obj;
    
    memset(device, 0, sizeof(tailscale_device_t));
    
    if (json_object_object_get_ex(device_obj, "id", &obj)) {
        strncpy(device->id, json_object_get_string(obj), sizeof(device->id) - 1);
    }
    
    if (json_object_object_get_ex(device_obj, "name", &obj)) {
        strncpy(device->name, json_object_get_string(obj), sizeof(device->name) - 1);
    }
    
    if (json_object_object_get_ex(device_obj, "addresses", &obj)) {
        int len = json_object_array_length(obj);
        device->address_count = len < 16 ? len : 16;
        
        for (int i = 0; i < device->address_count; i++) {
            json_object* addr_obj = json_object_array_get_idx(obj, i);
            strncpy(device->addresses[i], json_object_get_string(addr_obj), 
                   sizeof(device->addresses[i]) - 1);
        }
    }
    
    if (json_object_object_get_ex(device_obj, "os", &obj)) {
        strncpy(device->os, json_object_get_string(obj), sizeof(device->os) - 1);
    }
    
    if (json_object_object_get_ex(device_obj, "tags", &obj)) {
        strncpy(device->tags, json_object_get_string(obj), sizeof(device->tags) - 1);
    }
    
    if (json_object_object_get_ex(device_obj, "hostname", &obj)) {
        strncpy(device->hostname, json_object_get_string(obj), sizeof(device->hostname) - 1);
    }
    
    if (json_object_object_get_ex(device_obj, "online", &obj)) {
        device->is_online = json_object_get_boolean(obj);
    }
    
    if (json_object_object_get_ex(device_obj, "lastSeen", &obj)) {
        device->last_seen = (time_t)json_object_get_int64(obj);
    }
    
    if (json_object_object_get_ex(device_obj, "machineKey", &obj)) {
        strncpy(device->machine_key, json_object_get_string(obj), sizeof(device->machine_key) - 1);
    }
    
    if (json_object_object_get_ex(device_obj, "nodeKey", &obj)) {
        strncpy(device->node_key, json_object_get_string(obj), sizeof(device->node_key) - 1);
    }
    
    if (json_object_object_get_ex(device_obj, "user", &obj)) {
        strncpy(device->user, json_object_get_string(obj), sizeof(device->user) - 1);
    }
    
    if (json_object_object_get_ex(device_obj, "tailscaleIPs", &obj)) {
        json_object* ips_array = json_object_array_get_idx(obj, 0);
        if (ips_array) {
            strncpy(device->tailscale_ip, json_object_get_string(ips_array), 
                   sizeof(device->tailscale_ip) - 1);
        }
    }
    
    return TAILSCALE_SUCCESS;
}

int tailscale_init(const char* api_key, const char* tailnet) {
    if (tailscale_ctx.initialized) {
        return TAILSCALE_SUCCESS;
    }
    
    memset(&tailscale_ctx, 0, sizeof(tailscale_context_t));
    
    if (!api_key || !tailnet) {
        strcpy(tailscale_ctx.last_error, "API key and tailnet are required");
        return TAILSCALE_ERROR_CONFIG;
    }
    
    strncpy(tailscale_ctx.api_key, api_key, sizeof(tailscale_ctx.api_key) - 1);
    strncpy(tailscale_ctx.tailnet, tailnet, sizeof(tailscale_ctx.tailnet) - 1);
    strncpy(tailscale_ctx.base_url, TAILSCALE_API_BASE, sizeof(tailscale_ctx.base_url) - 1);
    
    tailscale_ctx.timeout_seconds = 30;
    tailscale_ctx.verify_ssl = 1;
    tailscale_ctx.auto_refresh = 0;
    tailscale_ctx.refresh_interval = 300;
    
    if (pthread_mutex_init(&tailscale_ctx.mutex, NULL) != 0) {
        strcpy(tailscale_ctx.last_error, "Failed to initialize mutex");
        return TAILSCALE_ERROR_INIT;
    }
    
    curl_global_init(CURL_GLOBAL_DEFAULT);
    
    tailscale_ctx.initialized = 1;
    
    logger_info("Tailscale module initialized for tailnet: %s", tailnet);
    return TAILSCALE_SUCCESS;
}

void tailscale_cleanup(void) {
    if (!tailscale_ctx.initialized) {
        return;
    }
    
    pthread_mutex_destroy(&tailscale_ctx.mutex);
    curl_global_cleanup();
    memset(&tailscale_ctx, 0, sizeof(tailscale_context_t));
    
    logger_info("Tailscale module cleaned up");
}

int tailscale_authenticate(const char* api_key) {
    if (!tailscale_ctx.initialized) {
        return TAILSCALE_ERROR_INIT;
    }
    
    pthread_mutex_lock(&tailscale_ctx.mutex);
    
    if (api_key) {
        strncpy(tailscale_ctx.api_key, api_key, sizeof(tailscale_ctx.api_key) - 1);
    }
    
    char* response = NULL;
    int result = tailscale_request("/api/v2/device", "GET", NULL, &response);
    
    if (result == TAILSCALE_SUCCESS) {
        tailscale_ctx.authenticated = 1;
        logger_info("Tailscale authentication successful");
    } else {
        tailscale_ctx.authenticated = 0;
        logger_error("Tailscale authentication failed: %s", tailscale_ctx.last_error);
    }
    
    if (response) {
        free(response);
    }
    
    pthread_mutex_unlock(&tailscale_ctx.mutex);
    return result;
}

int tailscale_refresh_devices(void) {
    if (!tailscale_ctx.initialized || !tailscale_ctx.authenticated) {
        return TAILSCALE_ERROR_AUTH;
    }
    
    pthread_mutex_lock(&tailscale_ctx.mutex);
    
    char* response = NULL;
    int result = tailscale_request("/api/v2/device", "GET", NULL, &response);
    
    if (result == TAILSCALE_SUCCESS && response) {
        json_object *root = json_tokener_parse(response);
        if (root) {
            tailscale_ctx.device_count = 0;
            
            json_object *devices_array;
            if (json_object_object_get_ex(root, "devices", &devices_array)) {
                int len = json_object_array_length(devices_array);
                tailscale_ctx.device_count = len < TAILSCALE_MAX_DEVICES ? len : TAILSCALE_MAX_DEVICES;
                
                for (int i = 0; i < tailscale_ctx.device_count; i++) {
                    json_object *device_obj = json_object_array_get_idx(devices_array, i);
                    parse_device_json(device_obj, &tailscale_ctx.devices[i]);
                }
            }
            
            json_object_put(root);
            tailscale_ctx.last_refresh = time(NULL);
            logger_info("Refreshed %d Tailscale devices", tailscale_ctx.device_count);
        } else {
            strcpy(tailscale_ctx.last_error, "Failed to parse JSON response");
            result = TAILSCALE_ERROR_PARSE;
        }
    }
    
    if (response) {
        free(response);
    }
    
    pthread_mutex_unlock(&tailscale_ctx.mutex);
    return result;
}

tailscale_device_t* tailscale_find_device_by_name(const char* name) {
    if (!tailscale_ctx.initialized || !name) {
        return NULL;
    }
    
    pthread_mutex_lock(&tailscale_ctx.mutex);
    
    for (int i = 0; i < tailscale_ctx.device_count; i++) {
        if (strcmp(tailscale_ctx.devices[i].name, name) == 0) {
            pthread_mutex_unlock(&tailscale_ctx.mutex);
            return &tailscale_ctx.devices[i];
        }
    }
    
    pthread_mutex_unlock(&tailscale_ctx.mutex);
    return NULL;
}

tailscale_device_t* tailscale_find_device_by_ip(const char* ip) {
    if (!tailscale_ctx.initialized || !ip) {
        return NULL;
    }
    
    pthread_mutex_lock(&tailscale_ctx.mutex);
    
    for (int i = 0; i < tailscale_ctx.device_count; i++) {
        for (int j = 0; j < tailscale_ctx.devices[i].address_count; j++) {
            if (strcmp(tailscale_ctx.devices[i].addresses[j], ip) == 0) {
                pthread_mutex_unlock(&tailscale_ctx.mutex);
                return &tailscale_ctx.devices[i];
            }
        }
    }
    
    pthread_mutex_unlock(&tailscale_ctx.mutex);
    return NULL;
}

tailscale_device_t* tailscale_find_device_by_hostname(const char* hostname) {
    if (!tailscale_ctx.initialized || !hostname) {
        return NULL;
    }
    
    pthread_mutex_lock(&tailscale_ctx.mutex);
    
    for (int i = 0; i < tailscale_ctx.device_count; i++) {
        if (strcmp(tailscale_ctx.devices[i].hostname, hostname) == 0) {
            pthread_mutex_unlock(&tailscale_ctx.mutex);
            return &tailscale_ctx.devices[i];
        }
    }
    
    pthread_mutex_unlock(&tailscale_ctx.mutex);
    return NULL;
}

int tailscale_get_device_list(tailscale_device_t** devices, int* count) {
    if (!tailscale_ctx.initialized || !devices || !count) {
        return TAILSCALE_ERROR_CONFIG;
    }
    
    pthread_mutex_lock(&tailscale_ctx.mutex);
    
    *devices = tailscale_ctx.devices;
    *count = tailscale_ctx.device_count;
    
    pthread_mutex_unlock(&tailscale_ctx.mutex);
    return TAILSCALE_SUCCESS;
}

int tailscale_set_target_by_device(lag_config_t* config, const char* device_name) {
    if (!config || !device_name) {
        return TAILSCALE_ERROR_CONFIG;
    }
    
    tailscale_device_t* device = tailscale_find_device_by_name(device_name);
    if (!device) {
        snprintf(tailscale_ctx.last_error, sizeof(tailscale_ctx.last_error),
                "Device not found: %s", device_name);
        return TAILSCALE_ERROR_NOT_FOUND;
    }
    
    if (device->address_count > 0) {
        pthread_mutex_lock(&config->mutex);
        
        if (config->target_ip) free(config->target_ip);
        config->target_ip = strdup(device->addresses[0]);
        
        pthread_mutex_unlock(&config->mutex);
        
        logger_info("Set lag switch target to device %s (%s)", 
                   device_name, device->addresses[0]);
        return TAILSCALE_SUCCESS;
    }
    
    strcpy(tailscale_ctx.last_error, "Device has no IP addresses");
    return TAILSCALE_ERROR_CONFIG;
}

int tailscale_list_devices(void) {
    if (!tailscale_ctx.initialized) {
        printf("Tailscale not initialized\n");
        return TAILSCALE_ERROR_INIT;
    }
    
    if (tailscale_auto_refresh_check() == TAILSCALE_SUCCESS) {
        tailscale_refresh_devices();
    }
    
    printf("Tailscale Devices (%d):\n", tailscale_ctx.device_count);
    printf("====================\n\n");
    
    for (int i = 0; i < tailscale_ctx.device_count; i++) {
        tailscale_device_t* device = &tailscale_ctx.devices[i];
        
        printf("Name: %s\n", device->name);
        printf("Hostname: %s\n", device->hostname);
        printf("OS: %s\n", device->os);
        printf("Status: %s\n", device->is_online ? "Online" : "Offline");
        
        printf("Addresses: ");
        for (int j = 0; j < device->address_count; j++) {
            printf("%s%s", device->addresses[j], 
                   j < device->address_count - 1 ? ", " : "");
        }
        printf("\n");
        
        if (strlen(device->tags) > 0) {
            printf("Tags: %s\n", device->tags);
        }
        
        if (device->last_seen > 0) {
            printf("Last Seen: %s", time_format_timestamp(device->last_seen));
        }
        
        printf("\n");
    }
    
    return TAILSCALE_SUCCESS;
}

const char* tailscale_get_error(void) {
    return tailscale_ctx.last_error;
}

int tailscale_is_authenticated(void) {
    return tailscale_ctx.authenticated;
}

int tailscale_get_device_count(void) {
    return tailscale_ctx.device_count;
}

void tailscale_set_auto_refresh(int enabled, int interval_seconds) {
    pthread_mutex_lock(&tailscale_ctx.mutex);
    tailscale_ctx.auto_refresh = enabled;
    tailscale_ctx.refresh_interval = interval_seconds;
    pthread_mutex_unlock(&tailscale_ctx.mutex);
}

int tailscale_auto_refresh_check(void) {
    if (!tailscale_ctx.auto_refresh) {
        return TAILSCALE_ERROR_CONFIG;
    }
    
    time_t now = time(NULL);
    if (now - tailscale_ctx.last_refresh >= tailscale_ctx.refresh_interval) {
        return TAILSCALE_SUCCESS;
    }
    
    return TAILSCALE_ERROR_CONFIG;
}
