#ifndef API_CLIENT_H
#define API_CLIENT_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define CMD_NFC_ENROLL_START 0x44
#define CMD_NFC_ENROLL_READY 0x22
#define CMD_NFC_ENROLLED     0x23
#define CMD_REMOTE_OPEN      0x40
#define CMD_REMOTE_CLOSE     0x4C
#define CMD_OFFLINE_LOG_BATCH 0x35
#define CMD_LOCAL_FACTORY_RESET 0x61
#define CMD_OTA_START        0x60

// Initialize API Client (Starts Claim process or MQTT)
void api_client_init(void);

// To be called periodically from main loop
void api_client_tick(void);

// Sends a "pass" event to the Onloi API via MQTT.
int api_client_send_pass_event(const char *credential);
void api_client_send_nfc_enroll_ready_event(void);
void api_client_send_lock_opened_event(void);
void api_client_send_lock_closed_event(void);
void api_client_reset_claim_status(void);
void api_client_send_offline_logs(void);
void api_client_send_local_factory_reset_event(void);

// Sends an NFC enrolled event (0x23)
void api_client_send_nfc_enrolled_event(uint32_t card_id);

// Sends an NFC enroll ready event (0x22)
void api_client_send_nfc_enroll_ready_event(void);

// Triggers a time sync request via MQTT.
int api_client_send_get_time(void);

/**
 * @brief Send Lock Opened event to MQTT backend
 */
void api_client_send_lock_opened_event(void);

/**
 * @brief Reset MQTT claim status and clear from NVS. Used when a new Provision token arrives over BLE.
 */
void api_client_reset_claim_status(void);

#endif
