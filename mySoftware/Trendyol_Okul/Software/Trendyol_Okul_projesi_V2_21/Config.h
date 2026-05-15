#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h> 

// =============================================================
// PIN DEFINITIONS (ESP32-WROOM-32E)
// =============================================================
// I2C
#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22

// UART2 for QR (Hardware Serial)
#define QR_RX_PIN 16 // RX2
#define QR_TX_PIN 17 // TX2

// GPIOs
#define RELAY_PIN  4 
#define BUZZER_PIN 5 

// =============================================================
// DEFAULTS
// =============================================================
#define DEFAULT_AP_SSID_PREFIX "Login_"
#define DEFAULT_AP_PASSWORD "Barfas2015"
#define DEFAULT_WEB_USERNAME "admin"
#define DEFAULT_WEB_PASSWORD "admin123"

#define AP_CHANNEL 1
#define AP_HIDDEN 0
#define AP_MAX_CONN 4

// =============================================================
// SYSTEM CONSTANTS
// =============================================================
#define FIRMWARE_VERSION "3.0.0-ESP32"
#define CONFIG_FILE_PATH "/spiffs/config.json"

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
typedef struct DeviceConfig {
  char deviceId[32];
  char updateDate[20];
  char wifiSsid[32];     // Custom SSID
  char wifiPassword[32]; // Custom WiFi Pass
  char webUsername[32];  // Custom Web User
  char webPassword[32];  // Custom Web Pass
} DeviceConfig;

#endif
