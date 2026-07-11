#ifndef RFID_H
#define RFID_H

#include "pin_manager.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Initialize the SPI bus and the MFRC522 RFID module
 */
void rfid_init(void);

/**
 * @brief Check if an RFID card is currently in range
 * @return true if card present, false otherwise
 */
bool rfid_is_card_present(void);

/**
 * @brief Read the UID of the presented card
 * @param uid Pointer to store the read UID
 * @param uid_len Pointer to store the length of the read UID
 * @return true if successful read, false otherwise
 */
bool rfid_read_uid(uint8_t *uid, uint8_t *uid_len);

/**
 * @brief Starts the FreeRTOS task on Core 0 to monitor the MFRC522
 * asynchronously
 */
void rfid_start_task(void);

#endif // RFID_H
