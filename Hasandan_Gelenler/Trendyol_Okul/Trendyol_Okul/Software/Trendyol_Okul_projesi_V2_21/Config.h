#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// =============================================================
// PIN DEFINITIONS
// =============================================================
#define I2C_SDA_PIN 4 // D2 (GPIO4)
#define I2C_SCL_PIN 5 // D1 (GPIO5)

#define RELAY_PIN  15  //  (GPIO15)
#define BUZZER_PIN 14 // D5 (GPIO14)

#define QR_RX_PIN 13 // D7 (GPIO13)
#define QR_TX_PIN 12 // D6 (GPIO12)

// =============================================================
// DEFAULTS
// =============================================================
#define DEFAULT_AP_SSID_PREFIX "Login_"
#define DEFAULT_AP_PASSWORD "Barfas2015"
#define DEFAULT_WEB_USERNAME "admin"
#define DEFAULT_WEB_PASSWORD "admin123"

#define AP_CHANNEL 1
#define AP_HIDDEN false
#define AP_MAX_CONN 4

// =============================================================
// SYSTEM CONSTANTS
// =============================================================
#define FIRMWARE_VERSION "2.2.1"
#define CONFIG_FILE_PATH "/config.json"

// =============================================================
// SECURITY CONSTANTS
// =============================================================
static const uint8_t AES_KEY[32] = {'X', '/', 'F', 'J', 'l', 'K', 'H', 'J',
                                    'u', 'c', 'O', 'p', '4', 'j', '1', 'P',
                                    'h', 'A', '2', 'r', 'P', '6', 'q', 'o',
                                    '7', 'P', '8', 'N', 'k', 'J', 'E', 'E'};

// =============================================================
// DATA STRUCTURES
// =============================================================
struct DeviceConfig {
  char deviceId[32];
  char updateDate[20];
  char wifiSsid[32];     // Custom SSID
  char wifiPassword[32]; // Custom WiFi Pass
  char webUsername[32];  // Custom Web User
  char webPassword[32];  // Custom Web Pass

  DeviceConfig() {
    // Defaults are applied in ConfigManager if file doesn't exist
    // or here for safety
    strcpy(deviceId, "00000");
    strcpy(updateDate, "2026-01-01 00:00:00");

    // Generate Default SSID with ChipID
    char defaultSsid[32];
    snprintf(defaultSsid, sizeof(defaultSsid), "%s%06X", DEFAULT_AP_SSID_PREFIX,
             ESP.getChipId());

    strcpy(wifiSsid, defaultSsid);
    strcpy(wifiPassword, DEFAULT_AP_PASSWORD);
    strcpy(webUsername, DEFAULT_WEB_USERNAME);
    strcpy(webPassword, DEFAULT_WEB_PASSWORD);
  }
};

#endif
