#include <string.h>
#include "esp_log.h"
#include "esp_mac.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "ble_manager.h"
extern void mqtt_manager_publish_notification(const uint8_t *mac, const uint8_t *data, size_t data_len);

static const char *TAG = "BLE_MGR";

QueueHandle_t ble_scan_queue = NULL;
QueueHandle_t ble_cmd_queue = NULL;

static uint8_t s_own_addr_type;
static bool s_scanning = false;
static bool s_connected = false;
static uint16_t s_conn_handle = 0;

static int ble_gap_event(struct ble_gap_event *event, void *arg);

// -- NEW GLOBAL STATE FOR GATT CLIENT --
static SemaphoreHandle_t s_gatt_semaphore = NULL;
static ble_lock_cmd_t s_current_cmd;
static bool s_gatt_op_success = false;
static bool s_svc_found = false;
static bool s_char_found = false;

static int ble_on_write(uint16_t conn_handle, const struct ble_gatt_error *error, struct ble_gatt_attr *attr, void *arg)
{
    ESP_LOGI(TAG, "Write complete; status=%d", error->status);
    if (error->status == 0) {
        s_gatt_op_success = true;
    }
    xSemaphoreGive(s_gatt_semaphore);
    return 0;
}

static int ble_on_disc_char(uint16_t conn_handle, const struct ble_gatt_error *error, const struct ble_gatt_chr *chr, void *arg)
{
    if (error->status == 0) {
        ESP_LOGI(TAG, "Found characteristic. Val handle: %d", chr->val_handle);
        s_char_found = true;
        // Write to it
        int rc = ble_gattc_write_flat(conn_handle, chr->val_handle, s_current_cmd.cmd_data, s_current_cmd.cmd_len, ble_on_write, NULL);
        if (rc != 0) {
            ESP_LOGE(TAG, "Failed to initiate write; rc=%d", rc);
            xSemaphoreGive(s_gatt_semaphore);
        }
    } else if (error->status == BLE_HS_EDONE) {
        if (!s_char_found) {
            ESP_LOGE(TAG, "Characteristic not found in service.");
            xSemaphoreGive(s_gatt_semaphore);
        }
    } else {
        ESP_LOGE(TAG, "Characteristic discovery failed; status=%d", error->status);
        xSemaphoreGive(s_gatt_semaphore);
    }
    return 0;
}

static int ble_on_disc_svc(uint16_t conn_handle, const struct ble_gatt_error *error, const struct ble_gatt_svc *service, void *arg)
{
    if (error->status == 0) {
        ESP_LOGI(TAG, "Found service. Start handle: %d, End handle: %d", service->start_handle, service->end_handle);
        s_svc_found = true;
        s_char_found = false; // Reset for char discovery
        // Start char discovery
        ble_uuid_any_t char_uuid;
        int rc = ble_uuid_from_str(&char_uuid, s_current_cmd.char_uuid);
        if (rc == 0) {
            rc = ble_gattc_disc_chrs_by_uuid(conn_handle, service->start_handle, service->end_handle, &char_uuid.u, ble_on_disc_char, NULL);
            if (rc != 0) {
                ESP_LOGE(TAG, "Failed to initiate char discovery; rc=%d", rc);
                xSemaphoreGive(s_gatt_semaphore);
            }
        } else {
            ESP_LOGE(TAG, "Invalid Char UUID string: %s", s_current_cmd.char_uuid);
            xSemaphoreGive(s_gatt_semaphore);
        }
    } else if (error->status == BLE_HS_EDONE) {
        if (!s_svc_found) {
            ESP_LOGE(TAG, "Service not found.");
            xSemaphoreGive(s_gatt_semaphore);
        }
    } else {
        ESP_LOGE(TAG, "Service discovery failed; status=%d", error->status);
        xSemaphoreGive(s_gatt_semaphore);
    }
    return 0;
}


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

            // Forward ALL discovered devices to MQTT for global lock support
            if (1) {
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
                
                // Start Service Discovery
                s_svc_found = false;
                ble_uuid_any_t svc_uuid;
                int rc = ble_uuid_from_str(&svc_uuid, s_current_cmd.service_uuid);
                if (rc == 0) {
                    rc = ble_gattc_disc_svc_by_uuid(s_conn_handle, &svc_uuid.u, ble_on_disc_svc, NULL);
                    if (rc != 0) {
                        ESP_LOGE(TAG, "Failed to initiate service discovery; rc=%d", rc);
                        xSemaphoreGive(s_gatt_semaphore);
                    }
                } else {
                    ESP_LOGE(TAG, "Invalid Service UUID string: %s", s_current_cmd.service_uuid);
                    xSemaphoreGive(s_gatt_semaphore);
                }
            } else {
                ESP_LOGE(TAG, "Connection failed; status=%d", event->connect.status);
                s_connected = false;
                xSemaphoreGive(s_gatt_semaphore);
                ble_app_scan(); // Resume scan
            }
            break;

        case BLE_GAP_EVENT_NOTIFY_RX: {
            ESP_LOGI(TAG, "Notification received from handle %d, length %d", event->notify_rx.attr_handle, OS_MBUF_PKTLEN(event->notify_rx.om));
            uint8_t data[128];
            uint16_t len = OS_MBUF_PKTLEN(event->notify_rx.om);
            if (len > sizeof(data)) len = sizeof(data);
            os_mbuf_copydata(event->notify_rx.om, 0, len, data);
            
            mqtt_manager_publish_notification(s_current_cmd.mac, data, len);
            break;
        }

        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(TAG, "Disconnected from peripheral; reason=%d", event->disconnect.reason);
            s_connected = false;
            s_conn_handle = 0;
            xSemaphoreGive(s_gatt_semaphore);
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
            
            // Setup global state for this operation
            memcpy(&s_current_cmd, &cmd, sizeof(ble_lock_cmd_t));
            s_gatt_op_success = false;
            // Empty the semaphore just in case it was given previously by an erroneous state
            xSemaphoreTake(s_gatt_semaphore, 0);

            ble_addr_t peer_addr;
            memcpy(peer_addr.val, cmd.mac, BLE_MAC_LEN);
            peer_addr.type = cmd.addr_type; // Use provided address type (0=Public, 1=Random)

            ESP_LOGI(TAG, "Connecting to target lock device...");
            int rc = ble_gap_connect(s_own_addr_type, &peer_addr, 30000, NULL, ble_gap_event, NULL);
            if (rc != 0) {
                ESP_LOGE(TAG, "Error initiating connection; rc=%d", rc);
                ble_app_scan(); // Resume scan
                continue;
            }

            // Wait for GATT operation to complete (timeout e.g. 10 seconds)
            if (xSemaphoreTake(s_gatt_semaphore, pdMS_TO_TICKS(10000)) == pdTRUE) {
                if (s_gatt_op_success) {
                    ESP_LOGI(TAG, "GATT Write Operation completed successfully.");
                } else {
                    ESP_LOGE(TAG, "GATT Write Operation failed.");
                }
            } else {
                ESP_LOGE(TAG, "GATT Operation timed out.");
            }

            // Terminate connection if still connected
            if (s_connected) {
                ble_gap_terminate(s_conn_handle, BLE_ERR_REM_USER_CONN_TERM);
                // Give it a moment to disconnect, disconnect event will resume scan
                vTaskDelay(pdMS_TO_TICKS(500));
            } else {
                ble_gap_conn_cancel(); // Cancel pending connection attempt
                ble_app_scan(); // Resume scan if already disconnected
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
    s_gatt_semaphore = xSemaphoreCreateBinary();

    if (ble_scan_queue == NULL || ble_cmd_queue == NULL || s_gatt_semaphore == NULL) {
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
