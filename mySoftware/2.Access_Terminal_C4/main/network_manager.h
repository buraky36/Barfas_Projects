#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include "esp_err.h"
#include <stdbool.h>

/**
 * @brief Initialize Network Manager (Handles Wi-Fi and BLE base initialization)
 */
esp_err_t network_manager_init(void);

/**
 * @brief Start provisioning mode (AP + BLE config)
 */
void network_manager_start_provisioning(void);

/**
 * @brief Connect to Wi-Fi using saved credentials
 */
void network_manager_connect_wifi(void);

/**
 * @brief Check if currently connected to Wi-Fi / Server
 */
bool network_manager_is_connected(void);

#endif // NETWORK_MANAGER_H
