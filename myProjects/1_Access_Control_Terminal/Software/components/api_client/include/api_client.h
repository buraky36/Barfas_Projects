#ifndef API_CLIENT_H
#define API_CLIENT_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define CMD_NFC_ENROLL_START 0x44
#define CMD_NFC_ENROLLED     0x23

// Initialize API Client (Starts Claim process or MQTT)
void api_client_init(void);

// To be called periodically from main loop
void api_client_tick(void);

// Sends a "pass" event to the Onloi API via MQTT.
int api_client_send_pass_event(const char* data_val);

// Sends an NFC enrolled event (0x23)
void api_client_send_nfc_enrolled_event(uint32_t card_id);

// Triggers a time sync request via MQTT.
int api_client_send_get_time(void);

/**
 * @brief Send Lock Opened event to MQTT backend
 */
void api_client_send_lock_opened_event(void);

#endif
