#ifndef KEYPAD_H
#define KEYPAD_H

#include "pin_manager.h"
#include <stdbool.h>
#include <stdint.h>


// Note: I2C frequency is kept local since it's an operational parameter
#define TSM12_I2C_FREQ_HZ 100000

/**
 * @brief Initialize the I2C bus and the TSM12 Touch IC
 */
void keypad_init(void);

/**
 * @brief Check if a touch event has occurred
 * @return true if touch detected
 */
bool keypad_is_touched(void);

/**
 * @brief Read the raw 12-bit key status from TSM12
 * @param keys Pointer to store the 16-bit word (only 12 bits used)
 * @return true if successful read
 */
bool keypad_read(uint16_t *keys);

/**
 * @brief Read the touched key value
 * @return The key digit (0-9, 10 for *, 11 for #) or 0xFF if no key
 */
/**
 * @brief Starts the FreeRTOS task on Core 0 to monitor the keypad
 * asynchronously
 */
void keypad_start_task(void);

#endif // KEYPAD_H
