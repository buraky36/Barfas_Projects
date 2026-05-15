#ifndef PROTOCOL_HANDLER_H
#define PROTOCOL_HANDLER_H

#include "esp_err.h"

/**
 * @brief Initialize OSDP (RS-485) and Wiegand protocol handlers
 */
esp_err_t protocol_handler_init(void);

/**
 * @brief Send Wiegand data to the Main Controller
 * @param data Array of bytes to send
 * @param bit_length Total number of bits (26 or 34 typically)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t protocol_handler_send_wiegand(const uint8_t *data,
                                        uint8_t bit_length);

/**
 * @brief Send OSDP message to the Main Controller
 * @param cmd The OSDP command byte
 * @param payload Payload bytes
 * @param len Payload length
 * @return esp_err_t ESP_OK on success
 */
esp_err_t protocol_handler_send_osdp(uint8_t cmd, const uint8_t *payload,
                                     uint8_t len);

#endif // PROTOCOL_HANDLER_H
