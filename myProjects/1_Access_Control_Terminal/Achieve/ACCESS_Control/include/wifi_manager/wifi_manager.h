#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "esp_err.h"
#include <stdbool.h>


/**
 * @brief Initialize the Wi-Fi manager.
 *        Sets up the ESP32 Wi-Fi station and network interfaces.
 */
void wifi_manager_init(void);

/**
 * @brief Connect to a specific Wi-Fi SSID
 * @param ssid Target SSID string
 * @param password Target password string
 * @return ESP_OK if successfully started connection process
 */
esp_err_t wifi_manager_connect(const char *ssid, const char *password);

/**
 * @brief Check if currently connected to Wi-Fi and got an IP
 */
bool wifi_manager_is_connected(void);

#endif // WIFI_MANAGER_H
