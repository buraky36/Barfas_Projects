#ifndef MFRC522_H
#define MFRC522_H

#include <stdint.h>
#include <stdbool.h>

bool mfrc522_init(void);
bool mfrc522_check_card(uint32_t *uid);

#endif
