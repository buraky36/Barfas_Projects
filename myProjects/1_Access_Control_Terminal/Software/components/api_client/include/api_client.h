#ifndef API_CLIENT_H
#define API_CLIENT_H

#include <stdbool.h>
#include <stdint.h>

// Initialize API Client (Starts Claim process or MQTT)
void api_client_init(void);

// To be called periodically from main loop
void api_client_tick(void);

// Sends a "pass" event to the Onloi API via MQTT.
// data_val format: "KEYPAD:123456", "RFID:AABBCCDD", "QR:DATA123"
// Returns 200 on success (simulated since MQTT is async), or negative error.
int api_client_send_pass_event(const char* data_val);

// Triggers a time sync request via MQTT.
int api_client_send_get_time(void);

#endif
