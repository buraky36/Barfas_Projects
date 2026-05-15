#include "wifi_manager.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include <string.h>
#include <stdio.h>
#include "../nv_storage/include/nv_storage.h"
#include "../api_client/include/api_client.h"

static const char *TAG = "WIFI_MGR";

static bool s_connected = false;
static bool s_scan_requested = false;
static bool s_scan_completed = false;
static bool s_pending_get_time = false;
static uint16_t s_ap_count = 0;
static wifi_ap_record_t s_ap_records[20]; // Max 20 APs to save RAM

static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT) {
        if (event_id == WIFI_EVENT_STA_START) {
            ESP_LOGI(TAG, "STA Started");
        } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
            s_connected = false;
            ESP_LOGW(TAG, "Disconnected from AP");
        } else if (event_id == WIFI_EVENT_SCAN_DONE) {
            uint16_t number = 20;
            esp_wifi_scan_get_ap_num(&s_ap_count);
            esp_wifi_scan_get_ap_records(&number, s_ap_records);
            s_ap_count = (s_ap_count > 20) ? 20 : s_ap_count;
            s_scan_completed = true;
            s_scan_requested = false;
            ESP_LOGI(TAG, "Scan done, %d APs found", s_ap_count);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_connected = true;
        
        sys_config_t config;
        nv_storage_get_config(&config);
        if (!config.is_online) {
            config.is_online = true;
            nv_storage_set_config(&config);
            ESP_LOGI(TAG, "Device is now permanently in ONLINE mode.");
        }
        
        // Notify backend that device is online to allow the app to add it
        // Do this in the main loop (tick) to avoid stack overflow in sys_evt task
        s_pending_get_time = true;
    }
}

void wifi_manager_init(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    // esp_event_loop_create_default() should be called in main! 
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, NULL);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}

bool wifi_manager_connect(const char *ssid, const char *pass) {
    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, pass, sizeof(wifi_config.sta.password) - 1);

    ESP_LOGI(TAG, "Connecting to %s", ssid);
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_connect());
    return true;
}

void wifi_manager_disconnect(void) {
    esp_wifi_disconnect();
}

void wifi_manager_clear_credentials(void) {
    esp_wifi_disconnect();
    esp_wifi_restore(); // Clears NVS flash for Wi-Fi configurations
    ESP_LOGI(TAG, "Wi-Fi credentials completely cleared from NVS.");
}

bool wifi_manager_is_connected(void) {
    return s_connected;
}

void wifi_manager_request_scan(void) {
    if (s_scan_requested) return;
    s_scan_requested = true;
    s_scan_completed = false;
    
    wifi_scan_config_t scan_config = {
        .ssid = 0,
        .bssid = 0,
        .channel = 0,
        .show_hidden = false
    };
    esp_wifi_scan_start(&scan_config, false); // false = non-blocking
    ESP_LOGI(TAG, "Starting non-blocking Wi-Fi scan");
}

bool wifi_manager_is_scan_ready(char *json_out, int max_len) {
    if (!s_scan_completed) return false;

    // Build JSON string
    int pos = snprintf(json_out, max_len, "{\"command\":\"getSsids\",\"result\":{");
    for (int i = 0; i < s_ap_count; i++) {
        if (pos >= max_len - 100) break; // Buffer protection
        int channel = s_ap_records[i].primary;
        const char *freq = (channel < 15) ? "2.4" : "5";
        
        pos += snprintf(json_out + pos, max_len - pos, 
            "\"%d\":{\"macAddress\":\"%02x:%02x:%02x:%02x:%02x:%02x\",\"rssi\":%d,\"frequency\":\"%s\",\"name\":\"%s\"}%s",
            i, 
            s_ap_records[i].bssid[0], s_ap_records[i].bssid[1], s_ap_records[i].bssid[2],
            s_ap_records[i].bssid[3], s_ap_records[i].bssid[4], s_ap_records[i].bssid[5],
            s_ap_records[i].rssi,
            freq,
            (char*)s_ap_records[i].ssid,
            (i == s_ap_count - 1) ? "" : ",");
    }
    snprintf(json_out + pos, max_len - pos, "}}");

    s_scan_completed = false; // Reset flag after reading
    return true;
}

void wifi_manager_tick(void) {
    if (s_pending_get_time) {
        s_pending_get_time = false;
        api_client_send_get_time();
    }
}

void wifi_manager_trigger_time_sync(void) {
    s_pending_get_time = true;
}
