#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include "Config.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>


class ConfigManager {
public:
  ConfigManager();
  bool begin();
  bool loadConfig(DeviceConfig &config);
  bool saveConfig(const DeviceConfig &config);
  bool resetConfig();

private:
  const char *_filename = CONFIG_FILE_PATH;
};

#endif
