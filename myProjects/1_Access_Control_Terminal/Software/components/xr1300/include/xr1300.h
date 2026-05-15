#ifndef XR1300_H
#define XR1300_H

#include <stdint.h>
#include <stdbool.h>

void xr1300_init(void);

// Non-blocking poll. Returns true when a full QR code string ending with newline is received.
bool xr1300_read_qr(char *qr_string_out, uint16_t max_len);

#endif
