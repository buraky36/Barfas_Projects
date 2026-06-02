#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#define WIFI_SSID_MAX_LEN 32
#define WIFI_PASS_MAX_LEN 64
#define MQTT_URI_MAX_LEN  128
#define DEVICE_ID_MAX_LEN 32

typedef struct {
    char wifi_ssid[WIFI_SSID_MAX_LEN];
    char wifi_pass[WIFI_PASS_MAX_LEN];
    char mqtt_uri[MQTT_URI_MAX_LEN];
    uint16_t mqtt_port;
    char device_id[DEVICE_ID_MAX_LEN];
    bool is_configured;
} gateway_config_t;

/**
 * @brief Initialize NVS storage
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t config_manager_init(void);

/**
 * @brief Read configuration from NVS
 * @param config Pointer to store configuration data
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t config_manager_load(gateway_config_t *config);

/**
 * @brief Write configuration to NVS
 * @param config Pointer to the configuration to save
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t config_manager_save(const gateway_config_t *config);

/**
 * @brief Reset configuration in NVS (erases configuration data)
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t config_manager_reset(void);

#endif // CONFIG_MANAGER_H
