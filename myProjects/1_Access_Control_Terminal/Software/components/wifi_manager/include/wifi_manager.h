#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

void wifi_manager_init(void);
void wifi_manager_tick(void); // For super-loop polling if needed
void wifi_manager_trigger_time_sync(void); // Safely triggers an API time sync from the main loop

bool wifi_manager_connect(const char *ssid, const char *pass);
void wifi_manager_disconnect(void);
bool wifi_manager_is_connected(void);
void wifi_manager_clear_credentials(void);

// Scan requests
void wifi_manager_request_scan(void);
bool wifi_manager_is_scan_ready(char *json_out, int max_len);
bool wifi_manager_is_scan_done(void);
uint16_t wifi_manager_get_ap_count(void);
bool wifi_manager_get_ap_record(uint16_t index, uint8_t *bssid, int8_t *rssi, uint8_t *primary, char *ssid);

#endif // WIFI_MANAGER_H
