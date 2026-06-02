#include <string.h>
#include "esp_log.h"
#include "esp_mac.h"
#include "mqtt_client.h"
#include "cJSON.h"
#include "ble_manager.h"
#include "mqtt_manager.h"

static const char *TAG = "MQTT_MGR";

static esp_mqtt_client_handle_t client = NULL;
static bool s_mqtt_connected = false;
static char s_device_id[32] = {0};
static char s_pub_topic[128] = {0};
static char s_sub_topic[128] = {0};

// Helper: Convert byte array to hex string
static void bytes_to_hex(const uint8_t *src, size_t src_len, char *dst)
{
    for (size_t i = 0; i < src_len; i++) {
        sprintf(dst + (i * 2), "%02X", src[i]);
    }
}

// Helper: Convert hex string to byte array
static size_t hex_to_bytes(const char *src, uint8_t *dst, size_t dst_max_len)
{
    size_t len = strlen(src);
    size_t bytes_count = 0;
    
    for (size_t i = 0; i < len && bytes_count < dst_max_len; i += 2) {
        unsigned int byte;
        sscanf(src + i, "%2x", &byte);
        dst[bytes_count++] = (uint8_t)byte;
    }
    return bytes_count;
}

// Helper: Parse colon-separated MAC string to bytes
static void mac_str_to_bytes(const char *mac_str, uint8_t *mac_bytes)
{
    unsigned int mac[6];
    sscanf(mac_str, "%02x:%02x:%02x:%02x:%02x:%02x", 
           &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);
    for (int i = 0; i < 6; i++) {
        mac_bytes[i] = (uint8_t)mac[i];
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT connected to broker!");
            s_mqtt_connected = true;
            
            // Subscribe to remote command topic
            int msg_id = esp_mqtt_client_subscribe(client, s_sub_topic, 1);
            ESP_LOGI(TAG, "Subscribed to command topic '%s' (Msg ID: %d)", s_sub_topic, msg_id);
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "MQTT disconnected from broker.");
            s_mqtt_connected = false;
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "Subscription acknowledged (Msg ID: %d)", event->msg_id);
            break;

        case MQTT_EVENT_DATA: {
            ESP_LOGI(TAG, "MQTT incoming message received. Topic: %.*s", event->topic_len, event->topic);
            
            // Null-terminate the payload string
            char *payload = malloc(event->data_len + 1);
            if (payload == NULL) {
                ESP_LOGE(TAG, "Out of memory to allocate payload buffer.");
                break;
            }
            memcpy(payload, event->data, event->data_len);
            payload[event->data_len] = '\0';
            
            ESP_LOGI(TAG, "Payload: %s", payload);

            // Parse incoming JSON command
            cJSON *root = cJSON_Parse(payload);
            if (root != NULL) {
                cJSON *mac_item = cJSON_GetObjectItem(root, "mac");
                cJSON *cmd_item = cJSON_GetObjectItem(root, "command");

                if (cJSON_IsString(mac_item) && cJSON_IsString(cmd_item)) {
                    ble_lock_cmd_t ble_cmd;
                    memset(&ble_cmd, 0, sizeof(ble_lock_cmd_t));

                    mac_str_to_bytes(mac_item->valuestring, ble_cmd.mac);
                    ble_cmd.cmd_len = hex_to_bytes(cmd_item->valuestring, ble_cmd.cmd_data, sizeof(ble_cmd.cmd_data));

                    ESP_LOGI(TAG, "Decoded incoming lock command: MAC=" MACSTR ", CmdLen=%d", 
                             MAC2STR(ble_cmd.mac), ble_cmd.cmd_len);

                    // Push command request to BLE Manager queue
                    if (ble_cmd_queue != NULL) {
                        if (xQueueSend(ble_cmd_queue, &ble_cmd, pdMS_TO_TICKS(1000)) != pdTRUE) {
                            ESP_LOGE(TAG, "Failed to push command to ble_cmd_queue (queue full).");
                        }
                    }
                } else {
                    ESP_LOGE(TAG, "Invalid fields in JSON command payload.");
                }
                cJSON_Delete(root);
            } else {
                ESP_LOGE(TAG, "Failed to parse JSON command payload.");
            }
            
            free(payload);
            break;
        }
        
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGD(TAG, "Publish complete (Msg ID: %d)", event->msg_id);
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT Event error occurred.");
            break;

        default:
            break;
    }
}

// Background task: reads scan queue and publishes as JSON to MQTT
static void mqtt_publisher_task(void *pvParameters)
{
    ble_scan_report_t report;
    ESP_LOGI(TAG, "MQTT Publisher Task started.");

    while (1) {
        if (xQueueReceive(ble_scan_queue, &report, portMAX_DELAY) == pdTRUE) {
            if (!s_mqtt_connected) {
                ESP_LOGD(TAG, "MQTT not connected. Discarding scanned packet.");
                continue;
            }

            // Create JSON document
            cJSON *root = cJSON_CreateObject();
            if (root == NULL) {
                ESP_LOGE(TAG, "Failed to create cJSON scan object.");
                continue;
            }

            char mac_str[18];
            sprintf(mac_str, "%02x:%02x:%02x:%02x:%02x:%02x",
                    report.mac[0], report.mac[1], report.mac[2],
                    report.mac[3], report.mac[4], report.mac[5]);
            
            char hex_data[64] = {0};
            bytes_to_hex(report.data, report.data_len, hex_data);

            cJSON_AddStringToObject(root, "mac", mac_str);
            cJSON_AddNumberToObject(root, "rssi", report.rssi);
            cJSON_AddStringToObject(root, "data", hex_data);
            cJSON_AddBoolToObject(root, "is_onloi", report.is_onloi_beacon);

            char *json_str = cJSON_PrintUnformatted(root);
            if (json_str != NULL) {
                ESP_LOGI(TAG, "[PUBLISH] Sending scan report: %s", json_str);
                esp_mqtt_client_publish(client, s_pub_topic, json_str, 0, 1, 0);
                free(json_str);
            }
            
            cJSON_Delete(root);
        }
    }
}

esp_err_t mqtt_manager_init(const char *uri, uint16_t port, const char *device_id)
{
    ESP_LOGI(TAG, "Initializing MQTT Client...");
    
    strncpy(s_device_id, device_id, sizeof(s_device_id));
    sprintf(s_pub_topic, "/gateway/%s/scan_report", s_device_id);
    sprintf(s_sub_topic, "/gateway/%s/command", s_device_id);

    // Setup MQTT config
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = uri,
        .broker.address.port = port,
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize ESP MQTT Client.");
        return ESP_FAIL;
    }

    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    
    return ESP_OK;
}

esp_err_t mqtt_manager_start(void)
{
    if (client == NULL) {
        ESP_LOGE(TAG, "Cannot start MQTT client (not initialized).");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Starting MQTT connection to broker...");
    esp_err_t err = esp_mqtt_client_start(client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start MQTT client: %s", esp_err_to_name(err));
        return err;
    }

    // Spawn publisher background task
    xTaskCreate(&mqtt_publisher_task, "mqtt_pub_task", 4096, NULL, 5, NULL);
    
    return ESP_OK;
}

esp_err_t mqtt_manager_stop(void)
{
    if (client == NULL) {
        return ESP_OK;
    }
    ESP_LOGI(TAG, "Stopping MQTT Client...");
    esp_err_t err = esp_mqtt_client_stop(client);
    if (err == ESP_OK) {
        s_mqtt_connected = false;
        ESP_LOGI(TAG, "MQTT Client stopped.");
    }
    return err;
}

bool mqtt_manager_is_connected(void)
{
    return s_mqtt_connected;
}
