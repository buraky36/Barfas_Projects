#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Tuya DPID tiplerini andıran veri tanımlamaları
typedef enum {
  SERVER_EVENT_UNLOCK_FINGERPRINT = 0,
  SERVER_EVENT_UNLOCK_PASSWORD,
  SERVER_EVENT_UNLOCK_CARD,
  SERVER_EVENT_UNLOCK_KEY,
  SERVER_EVENT_UNLOCK_APP,
  SERVER_EVENT_DOOR_OPENED,
  SERVER_EVENT_DOOR_CLOSED,
  SERVER_EVENT_BATTERY_STATE
} server_event_type_t;

typedef enum {
  SERVER_ALARM_WRONG_FINGERPRINT = 0,
  SERVER_ALARM_WRONG_PASSWORD,
  SERVER_ALARM_WRONG_CARD,
  SERVER_ALARM_FORCED_ENTRY
} server_alarm_type_t;

// Cihazın NVS üzerinde tutulacak konfigürasyon yapısı
typedef struct {
  uint32_t hotelId;
  uint32_t roomId;
  uint32_t zoneId;
  char masterSecret[64];
  char authCode[32];
} server_comm_config_t;

/**
 * @brief Initialize server communication module and load config from NVS
 */
void server_comm_init(void);

/**
 * @brief Save connection settings/keys to NVS
 */
bool server_comm_save_config(const server_comm_config_t *config);

/**
 * @brief Load connection settings/keys from NVS
 */
bool server_comm_load_config(server_comm_config_t *config);

/**
 * @brief Send FULL_SETUP request to server
 */
bool server_comm_send_setup(void);

/**
 * @brief Enqueue and send an event directly matching a TUYA DPID behavior
 */
bool server_comm_send_event(server_event_type_t type, const char *data);

/**
 * @brief Send an alarm payload to the server
 */
bool server_comm_send_alarm(server_alarm_type_t alarm_type);

#ifdef __cplusplus
}
#endif
