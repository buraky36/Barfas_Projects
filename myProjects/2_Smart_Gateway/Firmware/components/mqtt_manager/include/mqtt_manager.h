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
 * @brief Stop MQTT client
 * @return esp_err_t ESP_OK on success
 */
esp_err_t mqtt_manager_stop(void);

/**
 * @brief Check if MQTT is connected
 * @return true if connected
 */
bool mqtt_manager_is_connected(void);

/**
 * @brief Publish a notification from a BLE device
 */
void mqtt_manager_publish_notification(const uint8_t *mac, const uint8_t *data, size_t data_len);

#endif // MQTT_MANAGER_H
