#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include "Config.h"
#include <stdbool.h>

bool config_begin(void);
bool config_load(DeviceConfig *config);
bool config_save(const DeviceConfig *config);
bool config_reset(void);
void config_set_defaults(DeviceConfig *config);

#endif
