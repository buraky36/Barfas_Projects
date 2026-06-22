#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include "esp_err.h"

/**
 * @brief Initializes the OTA manager.
 * @return esp_err_t ESP_OK on success
 */
esp_err_t ota_manager_init(void);

/**
 * @brief Starts an OTA update from the given URL.
 *        This function spawns a background task.
 * @param url The HTTP/HTTPS URL to the firmware binary.
 * @return esp_err_t ESP_OK if task was spawned successfully
 */
esp_err_t ota_manager_start_update(const char *url);

#endif // OTA_MANAGER_H
