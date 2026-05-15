#ifndef HAL_TOUCH_H
#define HAL_TOUCH_H

#include <stdint.h>
#include <stdbool.h>

#define TOUCH_SDA_PIN 32
#define TOUCH_SCL_PIN 33
#define TOUCH_INT_PIN 14
#define TOUCH_OUT_PIN 27

void hal_touch_init(void);

bool hal_touch_has_interrupt(void);

bool hal_touch_read_key(char *key_out);

#endif
