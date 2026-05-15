#ifndef AUTH_ENGINE_H
#define AUTH_ENGINE_H

#include <stdbool.h>
#include <stdint.h>


typedef enum {
  AUTH_RESULT_DENIED,
  AUTH_RESULT_GRANTED,
  AUTH_RESULT_TIMEOUT
} auth_result_t;

/**
 * @brief Initialize the authentication engine
 */
void auth_engine_init(void);

/**
 * @brief Validate a card UID.
 * Checks server first (if Wi-Fi connected), then local cache if offline.
 */
auth_result_t auth_engine_validate_card(uint32_t card_uid);

/**
 * @brief Validate a PIN code.
 */
auth_result_t auth_engine_validate_pin(uint16_t pin_code);

#endif // AUTH_ENGINE_H
