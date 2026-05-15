#ifndef HAL_WIEGAND_H
#define HAL_WIEGAND_H

#include <stdint.h>
#include <stdbool.h>

#define WIEGAND_D0_PIN 22
#define WIEGAND_D1_PIN 23

// Initialize the Wiegand interface (sets GPIO pins to input)
void hal_wiegand_init(void);

// Set direction: true = Output (HIGH), false = Input (LOW)
void hal_wiegand_set_direction(bool is_output);

// For Reader Mode: Output Wiegand data (26 or 34 bits)
void hal_wiegand_write(uint32_t data, uint8_t bit_len, bool parity_en);

// For Controller Mode: Read Wiegand data
bool hal_wiegand_available(void);
uint32_t hal_wiegand_read(uint8_t *bit_len_out);

#endif
