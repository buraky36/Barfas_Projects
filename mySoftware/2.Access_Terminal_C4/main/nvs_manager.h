#ifndef NVS_MANAGER_H
#define NVS_MANAGER_H

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>


/**
 * @brief Initialize the NVS flash storage
 * @return esp_err_t ESP_OK on success
 */
esp_err_t nvs_manager_init(void);

/**
 * @brief Check if the device has passed factory testing
 * @return true if passed, false otherwise (or if not set)
 */
bool nvs_manager_is_factory_tested(void);

/**
 * @brief Set the factory tested flag in NVS
 * @param passed true if passed, false if failed
 * @return esp_err_t ESP_OK on success
 */
esp_err_t nvs_manager_set_factory_tested(bool passed);

/**
 * @brief Get authentication interval in seconds
 */
uint16_t nvs_manager_get_auth_interval(void);

/**
 * @brief Get maximum failed attempts before lock/alarm
 */
uint16_t nvs_manager_get_max_failed_attempts(void);

#endif // NVS_MANAGER_H
