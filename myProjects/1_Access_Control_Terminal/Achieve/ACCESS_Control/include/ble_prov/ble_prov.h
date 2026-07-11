#ifndef BLE_PROV_H
#define BLE_PROV_H

#include <stdbool.h>

/**
 * @brief Initialize and start the BLE Provisioning service using NimBLE.
 *        This will start advertising as "Onloi_SMART_XXXXXX" and expose
 *        the custom GATT characteristics for Wi-Fi and Key pairing.
 *
 * @return true if successfully initialized
 */
bool ble_prov_init(void);

/**
 * @brief Stop the BLE Provisioning service and free resources.
 *        Usually called after successful Wi-Fi connection.
 */
void ble_prov_stop(void);

#endif // BLE_PROV_H
