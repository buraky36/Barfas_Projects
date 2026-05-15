#ifndef QRHANDLER_H
#define QRHANDLER_H

#include <stdbool.h>
#include <stddef.h>
#include "Config.h"

// AES GCM Constants
#define GCM_IV_LEN 12
#define GCM_TAG_LEN 16

bool qr_process(const char *encryptedBase64, const DeviceConfig *config);

#endif
