#ifndef API_CLIENT_H
#define API_CLIENT_H

#include <stdbool.h>
#include <stdint.h>

// Sends a "pass" event to the Onloi API.
// data_val format: "KEYPAD:123456", "RFID:AABBCCDD", "FINGERPRINT:1", "QR:DATA123"
// Returns HTTP Status Code (200, 403, etc.) or negative error code if request failed.
int api_client_send_pass_event(const char* data_val);
int api_client_send_get_time(void);

#endif
