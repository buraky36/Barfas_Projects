#ifndef QR_READER_H
#define QR_READER_H

#include "driver/uart.h"
#include "pin_manager.h"
#include <stdbool.h>
#include <stdint.h>

// Operational parameter kept local
#define QR_UART_BAUD_RATE 9600 // Common typical baud rate for XR1300

/**
 * @brief Initialize the UART interface for XR1300 QR Reader
 */
void qr_reader_init(void);

/**
 * @brief Starts the FreeRTOS task on Core 0 to monitor the UART for incoming QR
 * data
 */
void qr_reader_start_task(void);

#endif // QR_READER_H
