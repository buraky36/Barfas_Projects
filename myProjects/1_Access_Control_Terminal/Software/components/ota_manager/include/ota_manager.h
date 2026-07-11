#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <stdbool.h>

/**
 * @brief Starts the OTA update process in the background.
 *
 * @param url The HTTPS URL to the firmware .bin file.
 * @return true if OTA task started successfully, false otherwise.
 */
bool ota_manager_start(const char *url);

#endif // OTA_MANAGER_H
