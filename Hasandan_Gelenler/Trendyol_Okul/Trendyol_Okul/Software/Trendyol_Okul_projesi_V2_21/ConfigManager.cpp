#include "ConfigManager.h"

ConfigManager::ConfigManager() {}

bool ConfigManager::begin() {
  Serial.println("[Config] Initializing LittleFS...");
  if (!LittleFS.begin()) {
    Serial.println("[Config] ERROR: LittleFS Mount Failed");
    return false;
  }
  Serial.println("[Config] LittleFS Mounted Successfully");
  return true;
}

bool ConfigManager::loadConfig(DeviceConfig &config) {
  Serial.printf("[Config] Loading config from %s\n", _filename);

  // If file doesn't exist, the struct constructor already set defaults
  if (!LittleFS.exists(_filename)) {
    Serial.println("[Config] Config file does not exist, using defaults.");
    return false;
  }

  File file = LittleFS.open(_filename, "r");
  if (!file) {
    Serial.println("[Config] ERROR: Failed to open config file for reading");
    return false;
  }

  StaticJsonDocument<1024> doc; // Increased size
  DeserializationError error = deserializeJson(doc, file);

  file.close();

  if (error) {
    Serial.printf("[Config] ERROR: Failed to parse config file: %s\n",
                  error.c_str());
    return false;
  }

  strlcpy(config.deviceId, doc["device_id"] | "00000", sizeof(config.deviceId));
  strlcpy(config.updateDate, doc["update_date"] | "2026-01-01 00:00:00",
          sizeof(config.updateDate));

  // Load WiFi Settings
  if (doc.containsKey("wifi_ssid"))
    strlcpy(config.wifiSsid, doc["wifi_ssid"], sizeof(config.wifiSsid));
  if (doc.containsKey("wifi_pass"))
    strlcpy(config.wifiPassword, doc["wifi_pass"], sizeof(config.wifiPassword));

  // Load Web Settings
  if (doc.containsKey("web_user"))
    strlcpy(config.webUsername, doc["web_user"], sizeof(config.webUsername));
  if (doc.containsKey("web_pass"))
    strlcpy(config.webPassword, doc["web_pass"], sizeof(config.webPassword));

  Serial.println("[Config] Configuration loaded.");
  return true;
}

bool ConfigManager::saveConfig(const DeviceConfig &config) {
  Serial.printf("[Config] Saving configuration to %s...\n", _filename);

  File file = LittleFS.open(_filename, "w");
  if (!file) {
    Serial.println("[Config] ERROR: Failed to open config file for writing");
    return false;
  }

  StaticJsonDocument<1024> doc;
  doc["device_id"] = config.deviceId;
  doc["update_date"] = config.updateDate;

  doc["wifi_ssid"] = config.wifiSsid;
  doc["wifi_pass"] = config.wifiPassword;

  doc["web_user"] = config.webUsername;
  doc["web_pass"] = config.webPassword;

  if (serializeJson(doc, file) == 0) {
    Serial.println("[Config] ERROR: Failed to write to file");
    file.close();
    return false;
  }

  file.close();
  Serial.println("[Config] Configuration saved successfully.");
  return true;
}

bool ConfigManager::resetConfig() {
  Serial.println("[Config] Resetting/Deleting config file...");
  return LittleFS.remove(_filename);
}
