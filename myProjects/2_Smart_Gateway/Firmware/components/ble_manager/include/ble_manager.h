#ifndef BLE_MANAGER_H
#define BLE_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#define BLE_MAC_LEN 6
#define BLE_MAX_DATA_LEN 31

typedef struct {
    uint8_t mac[BLE_MAC_LEN];
    uint8_t addr_type; // 0: Public, 1: Random
    int8_t rssi;
    uint8_t data[BLE_MAX_DATA_LEN];
    uint8_t data_len;
    bool is_onloi_beacon;
    char device_code[7]; // e.g., "OKGWX1" + null terminator
    bool claimed;
} ble_scan_report_t;

typedef struct {
    uint8_t mac[BLE_MAC_LEN];
    uint8_t addr_type; // 0: Public, 1: Random
    char service_uuid[37]; // e.g. "0000ffe0-0000-1000-8000-00805f9b34fb"
    char char_uuid[37];
    uint8_t cmd_data[32];
    uint8_t cmd_len;
} ble_lock_cmd_t;


// Queue to send scanned BLE data to MQTT Manager
extern QueueHandle_t ble_scan_queue;

// Queue to receive lock control commands from MQTT Manager
extern QueueHandle_t ble_cmd_queue;

/**
 * @brief Initialize the NimBLE stack
 * @return esp_err_t ESP_OK on success
 */
esp_err_t ble_manager_init(void);

/**
 * @brief Start BLE scanner and listener tasks
 * @return esp_err_t ESP_OK on success
 */
esp_err_t ble_manager_start(void);

/**
 * @brief Stop BLE scanner and listener tasks
 * @return esp_err_t ESP_OK on success
 */
esp_err_t ble_manager_stop(void);

#endif // BLE_MANAGER_H
