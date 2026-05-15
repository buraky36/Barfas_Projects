#ifndef COMM_WIEGAND_H
#define COMM_WIEGAND_H

#include <stdbool.h>
#include <stdint.h>


/**
 * @brief Initialize Wiegand interface for input and output
 */
void comm_wiegand_init(void);

/**
 * @brief Send 26-bit Wiegand data
 * @param facility_code Facility code (8-bit)
 * @param card_number Card number (16-bit)
 */
void comm_wiegand_send_26(uint8_t facility_code, uint16_t card_number);

/**
 * @brief Send 34-bit Wiegand data
 * @param data 32-bit data (Facility + Card)
 */
void comm_wiegand_send_34(uint32_t data);

/**
 * @brief Read available Wiegand data (input)
 * @param data Pointer to store the read data
 * @param bits Pointer to store the number of bits read (26 or 34)
 * @return true if new data was read, false otherwise
 */
bool comm_wiegand_read(uint32_t *data, uint8_t *bits);

#endif // COMM_WIEGAND_H
