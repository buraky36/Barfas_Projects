#ifndef COMM_OSDP_H
#define COMM_OSDP_H

#include "driver/uart.h"
#include <stdbool.h>
#include <stdint.h>


#define OSDP_UART_NUM UART_NUM_1
#define OSDP_BAUD_RATE 9600 // Standard OSDP baud is often 9600, or up to 115200

/**
 * @brief Initialize the RS485 UART interface for OSDP communication
 */
void comm_osdp_init(void);

/**
 * @brief Send raw bytes over RS485
 * @param data Data buffer to send
 * @param len Length of data
 */
void comm_osdp_send(const uint8_t *data, uint16_t len);

/**
 * @brief Check for and read data from RS485 interface
 * @param buffer Pointer to store read data
 * @param buffer_len Max length of the buffer
 * @return Number of bytes read
 */
uint16_t comm_osdp_read(uint8_t *buffer, uint16_t buffer_len);

#endif // COMM_OSDP_H
