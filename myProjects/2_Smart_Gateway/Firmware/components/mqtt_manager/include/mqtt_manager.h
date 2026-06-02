#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <stdbool.h>
#include "esp_err.h"

/**
 * @brief Initialize the MQTT client configuration
 * @param uri MQTT broker URI
 * @param port MQTT broker Port
 * @param device_id ID of this gateway device
 * @return esp_err_t ESP_OK on success
 */
esp_err_t mqtt_manager_init(const char *uri, uint16_t port, const char *device_id);

/**
 * @brief Start the MQTT client connection and publishing tasks
 * @return esp_err_t ESP_OK on success
 */
esp_err_t mqtt_manager_start(void);

/**
 * @brief Stop the MQTT client and terminate publishing tasks
 * @return esp_err_t ESP_OK on success
 */
esp_err_t mqtt_manager_stop(void);

/**
 * @brief Check if MQTT is currently connected to the broker
 * @return true if connected, false otherwise
 */
bool mqtt_manager_is_connected(void);

#endif // MQTT_MANAGER_H
