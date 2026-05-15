#ifndef TSM12_H
#define TSM12_H

#include <stdint.h>
#include <stdbool.h>

void tsm12_init(void);
bool tsm12_read_keys(uint16_t *keys_mask);

#endif
