#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdbool.h>

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

#endif // WIFI_MANAGER_H
