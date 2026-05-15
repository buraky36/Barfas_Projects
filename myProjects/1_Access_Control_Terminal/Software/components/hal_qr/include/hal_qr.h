#ifndef HAL_QR_H
#define HAL_QR_H

#include <stdint.h>
#include <stdbool.h>

void hal_qr_init(void);
bool hal_qr_read(char *qr_out, uint16_t max_len);

#endif
