#ifndef HAL_RFID_H
#define HAL_RFID_H

#include <stdint.h>
#include <stdbool.h>

#define RFID_MOSI_PIN 5
#define RFID_MISO_PIN 36
#define RFID_CLK_PIN  15
#define RFID_CS_PIN   0
#define RFID_IRQ_PIN  35

void hal_rfid_init(void);

// Returns true if a card was read, fills uid_out and uid_len_out
bool hal_rfid_check_card(uint8_t *uid_out, uint8_t *uid_len_out);

// Returns true if the IRQ pin is active
bool hal_rfid_has_interrupt(void);

#endif
