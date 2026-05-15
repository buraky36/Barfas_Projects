#include "api_client.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "cJSON.h"
#include "nv_storage.h"
#include "esp_mac.h"
#include "esp_crt_bundle.h"
#include <string.h>
#include <stdlib.h>
static const char *TAG = "API_CLIENT";
static const char *API_URL = "https://api.onloi.com/api/device/runtime";

static void generate_device_id(char *out_id, size_t max_len) {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_BT);
    uint32_t low = ((uint32_t)mac[3] << 16) | ((uint32_t)mac[4] << 8) | mac[5];
    snprintf(out_id, max_len, "SMART_%06lX", (unsigned long)low);
}

static esp_err_t _http_event_handle(esp_http_client_event_t *evt) {
    switch(evt->event_id) {
        default: break;
        case HTTP_EVENT_ERROR:
            ESP_LOGI(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_ON_DATA:
            break;
    }
    return ESP_OK;
}

int api_client_send_pass_event(const char* data_val) {
    char deviceKey[64] = {0};
    char pairKey[64] = {0};
    if (!nv_storage_get_api_keys(deviceKey, pairKey)) {
        ESP_LOGE(TAG, "No API keys provisioned. Call BLE Pairing first.");
        return -1;
    }

    char device_id[32];
    generate_device_id(device_id, sizeof(device_id));

    cJSON *root = cJSON_CreateObject();
    cJSON *args = cJSON_CreateObject();
    cJSON_AddStringToObject(args, "command", "open");
    cJSON_AddStringToObject(args, "pairKey", pairKey);
    cJSON_AddStringToObject(args, "deviceKey", deviceKey);
    cJSON_AddStringToObject(args, "deviceId", device_id);
    cJSON_AddNumberToObject(args, "createdAt", 0);
    cJSON_AddStringToObject(args, "data", data_val);
    cJSON_AddItemToObject(root, "args", args);
    cJSON_AddStringToObject(root, "action", "run");
    cJSON_AddStringToObject(root, "name", "device");

    char *post_data = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (!post_data) return -2;

    ESP_LOGI(TAG, "Sending HTTP POST: %s", post_data);

    esp_http_client_config_t config = {
        .url = API_URL,
        .event_handler = _http_event_handle,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        free(post_data);
        return -3;
    }

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    esp_err_t err = esp_http_client_perform(client);
    int status_code = -1;

    if (err == ESP_OK) {
        status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "HTTPS Status = %d, content_length = %lld",
                 status_code, esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    free(post_data);
    return status_code;
}

int api_client_send_get_time(void) {
    char deviceKey[64] = {0};
    char pairKey[64] = {0};
    if (!nv_storage_get_api_keys(deviceKey, pairKey)) {
        ESP_LOGE(TAG, "No API keys provisioned. Cannot get time.");
        return -1;
    }

    char device_id[32];
    generate_device_id(device_id, sizeof(device_id));

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "action", "run");
    cJSON_AddStringToObject(root, "name", "device");
    
    cJSON *args = cJSON_CreateObject();
    cJSON_AddStringToObject(args, "command", "getTime");
    cJSON_AddStringToObject(args, "deviceKey", deviceKey);
    cJSON_AddStringToObject(args, "pairKey", pairKey);
    cJSON_AddStringToObject(args, "deviceId", device_id);

    cJSON_AddItemToObject(root, "args", args);

    char *post_data = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (!post_data) return -2;

    ESP_LOGI(TAG, "Sending HTTP POST (getTime): %s", post_data);

    esp_http_client_config_t config = {
        .url = API_URL,
        .event_handler = _http_event_handle,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        free(post_data);
        return -3;
    }

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    esp_err_t err = esp_http_client_perform(client);
    int status_code = -1;

    if (err == ESP_OK) {
        status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "HTTPS Status (getTime) = %d", status_code);
        
        // Optionally, we could parse the response to set the system time here.
        // For now, the main goal is to notify the server that the device is online
        // so the mobile app can successfully add the device.
    } else {
        ESP_LOGE(TAG, "HTTP POST (getTime) failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    free(post_data);
    return status_code;
}
