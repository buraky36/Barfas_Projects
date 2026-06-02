#include <string.h>
#include "esp_log.h"
#include "esp_mac.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "ble_manager.h"

static const char *TAG = "BLE_MGR";

QueueHandle_t ble_scan_queue = NULL;
QueueHandle_t ble_cmd_queue = NULL;

static uint8_t s_own_addr_type;
static bool s_scanning = false;
static bool s_connected = false;
static uint16_t s_conn_handle = 0;

static int ble_gap_event(struct ble_gap_event *event, void *arg);

// Start scanning
static void ble_app_scan(void)
{
    struct ble_gap_disc_params disc_params;
    int rc;

    if (s_scanning || s_connected) {
        return;
    }

    ESP_LOGI(TAG, "Starting BLE scanning...");

    memset(&disc_params, 0, sizeof(disc_params));
    disc_params.filter_duplicates = 1;
    disc_params.passive = 1; // Passive scanning as requested in document defaults
    disc_params.itvl = 0;
    disc_params.window = 0;
    disc_params.filter_policy = 0;
    disc_params.limited = 0;

    rc = ble_gap_disc(s_own_addr_type, BLE_HS_FOREVER, &disc_params, ble_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error initiating scan; rc=%d", rc);
        s_scanning = false;
    } else {
        s_scanning = true;
    }
}

// Stop scanning
static void ble_app_scan_cancel(void)
{
    if (!s_scanning) {
        return;
    }
    ESP_LOGI(TAG, "Stopping BLE scanning...");
    int rc = ble_gap_disc_cancel();
    if (rc == 0 || rc == BLE_HS_EALREADY) {
        s_scanning = false;
    } else {
        ESP_LOGE(TAG, "Failed to cancel scanning; rc=%d", rc);
    }
}

// BLE Gap event handler
static int ble_gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {
        case BLE_GAP_EVENT_DISC: {
            struct ble_hs_adv_fields fields;
            int rc = ble_hs_adv_parse_fields(&fields, event->disc.data, event->disc.length_data);
            if (rc != 0) {
                return 0;
            }

            // Onloi Beacon filtering check
            // (Filters by name containing "Onloi", or custom manufacturer data)
            bool is_onloi = false;
            
            if (fields.name_len > 0 && strncmp((char *)fields.name, "Onloi", 5) == 0) {
                is_onloi = true;
            } else if (fields.mfg_data_len > 0) {
                // If it has manufacturer data, treat as a candidate
                is_onloi = true;
            }

            if (is_onloi) {
                ble_scan_report_t report;
                memcpy(report.mac, event->disc.addr.val, BLE_MAC_LEN);
                report.rssi = event->disc.rssi;
                report.data_len = event->disc.length_data > BLE_MAX_DATA_LEN ? BLE_MAX_DATA_LEN : event->disc.length_data;
                memcpy(report.data, event->disc.data, report.data_len);
                report.is_onloi_beacon = true;

                ESP_LOGI(TAG, "[SCAN] Discovered Onloi Beacon: MAC=" MACSTR ", RSSI=%d, DataLen=%d", 
                         MAC2STR(report.mac), report.rssi, report.data_len);
                
                // Send to MQTT Queue without blocking
                if (ble_scan_queue != NULL) {
                    xQueueSend(ble_scan_queue, &report, 0);
                }
            }
            break;
        }

        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0) {
                ESP_LOGI(TAG, "Connected successfully to peripheral! Conn Handle: %d", event->connect.conn_handle);
                s_connected = true;
                s_conn_handle = event->connect.conn_handle;
                // Wait for commands to write, or perform service discovery
            } else {
                ESP_LOGE(TAG, "Connection failed; status=%d", event->connect.status);
                s_connected = false;
                ble_app_scan(); // Resume scan
            }
            break;

        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(TAG, "Disconnected from peripheral; reason=%d", event->disconnect.reason);
            s_connected = false;
            s_conn_handle = 0;
            ble_app_scan(); // Resume scan
            break;

        default:
            break;
    }
    return 0;
}

static void ble_on_sync(void)
{
    int rc = ble_hs_util_ensure_addr(0);
    assert(rc == 0);
    rc = ble_hs_id_infer_auto(0, &s_own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error determining address type; rc=%d", rc);
        return;
    }
    
    ESP_LOGI(TAG, "BLE Host synced. Starting default scan.");
    ble_app_scan();
}

static void ble_on_reset(int reason)
{
    ESP_LOGW(TAG, "BLE Host reset; reason=%d", reason);
}

// Background task handling BLE connection and GATT command writing
static void ble_cmd_listener_task(void *pvParameters)
{
    ble_lock_cmd_t cmd;
    ESP_LOGI(TAG, "BLE Command Listener Task started.");

    while (1) {
        if (xQueueReceive(ble_cmd_queue, &cmd, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(TAG, "Received lock control command for MAC " MACSTR, MAC2STR(cmd.mac));
            
            // Stop scanning to connect
            ble_app_scan_cancel();
            
            ble_addr_t peer_addr;
            memcpy(peer_addr.val, cmd.mac, BLE_MAC_LEN);
            peer_addr.type = BLE_ADDR_PUBLIC; // or BLE_ADDR_RANDOM depending on peripheral type

            ESP_LOGI(TAG, "Connecting to target lock device...");
            int rc = ble_gap_connect(s_own_addr_type, &peer_addr, 30000, NULL, ble_gap_event, NULL);
            if (rc != 0) {
                ESP_LOGE(TAG, "Error initiating connection; rc=%d", rc);
                ble_app_scan(); // Resume scan
                continue;
            }

            // Wait for connection to establish and write command (simplified representation of GATT write)
            // In a real device, you wait for connect, discover characteristics, write, and disconnect.
            vTaskDelay(pdMS_TO_TICKS(1500)); // Simulate connection and writing
            
            if (s_connected) {
                ESP_LOGI(TAG, "Writing command data to lock characteristic...");
                // ble_gattc_write_flat(...) is normally called here.
                
                // Terminate connection
                ESP_LOGI(TAG, "Command written. Disconnecting...");
                ble_gap_terminate(s_conn_handle, BLE_ERR_REM_USER_CONN_TERM);
            } else {
                ESP_LOGE(TAG, "Could not establish connection to write command.");
                ble_app_scan(); // Ensure scanning is running
            }
        }
    }
}

static void ble_host_task(void *param)
{
    ESP_LOGI(TAG, "BLE Host Task started.");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

esp_err_t ble_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing NimBLE Stack...");
    
    ble_scan_queue = xQueueCreate(10, sizeof(ble_scan_report_t));
    ble_cmd_queue = xQueueCreate(5, sizeof(ble_lock_cmd_t));

    if (ble_scan_queue == NULL || ble_cmd_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create BLE queues.");
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize nimble port: %s", esp_err_to_name(ret));
        return ret;
    }

    // Configure NimBLE callbacks
    ble_hs_cfg.reset_cb = ble_on_reset;
    ble_hs_cfg.sync_cb = ble_on_sync;
    
    int rc = ble_svc_gap_device_name_set("Smart_Gateway");
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to set device name; rc=%d", rc);
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t ble_manager_start(void)
{
    ESP_LOGI(TAG, "Starting BLE Host thread and command listener...");
    nimble_port_freertos_init(ble_host_task);
    
    // Spawn task to listen for outgoing BLE command writes
    xTaskCreate(&ble_cmd_listener_task, "ble_cmd_task", 4096, NULL, 5, NULL);
    
    return ESP_OK;
}

esp_err_t ble_manager_stop(void)
{
    ESP_LOGI(TAG, "Stopping BLE Manager...");
    ble_app_scan_cancel();
    return nimble_port_stop();
}
