#include "esp_bt.h"
#include "esp_idf_version.h"
#include "mqtt_client.h"
#include "esp_crt_bundle.h"
#include <ArduinoJson.h>
#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <mbedtls/md.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

// Sunucu Ayarları
#define CLAIM_URL "https://api.onloi.com/v1/devices/claim"

// BLE Servis ve Karakteristik UUID'leri (Nordic UART standardı)
#define SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E" // Mobil yazma yapar
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E" // ESP32 notify yapar

// Test Modu
#define ENABLE_SERIAL_TEST

#ifdef ENABLE_SERIAL_TEST
#define DEBUG_PRINT(...) Serial.print(__VA_ARGS__)
#define DEBUG_PRINTLN(...) Serial.println(__VA_ARGS__)
#define DEBUG_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
#define DEBUG_PRINT(...)
#define DEBUG_PRINTLN(...)
#define DEBUG_PRINTF(...)
#endif

// Global Cihaz Bilgileri
String device_id = "Smart2026";
String device_code = "UNKNOWN";
String ble_name;

// Onloi Protokol Tanımları
#define SOF_BYTE 0x4F
#define VER_BYTE 0x02

// MSG_TYPE
#define MSG_TYPE_REQUEST 0x01
#define MSG_TYPE_RESPONSE 0x02
#define MSG_TYPE_EVENT 0x03
#define MSG_TYPE_COMMAND 0x04
#define MSG_TYPE_ACK 0x05

// CMD Kodları
#define CMD_HANDSHAKE 0x01
#define CMD_PROVISION 0x02
#define CMD_GET_SSIDS 0x03
#define CMD_CONNECT_WIFI 0x04
#define CMD_LOCK_OPENED 0x10
#define CMD_PIN_ENTERED 0x12
#define CMD_NFC_SCANNED 0x13
#define CMD_QR_SCANNED 0x14
#define CMD_FINGERPRINT_SCANNED 0x15
#define CMD_BUTTON_UNLOCK_REQ 0x16
#define CMD_SESSION_START 0x30
#define CMD_BATTERY_REPORT 0x32
#define CMD_TIME_SYNC 0x33
#define CMD_REGISTRATION 0x34
#define CMD_OFFLINE_LOG_BATCH 0x35
#define CMD_REMOTE_OPEN 0x40
#define CMD_FP_ENROLL_START 0x46
#define CMD_CARD_ENROLL_START 0x47
#define CMD_FP_ENROLLED 0x26
#define CMD_CARD_ENROLLED 0x27
#define CMD_FACTORY_RESET 0x50
#define CMD_ACK 0x05

// Legacy MCU Haberleşme (0x55AA)
#define Header 0x55AA
#define Version 0x00
#define Wifi_Status_Data_Len 1
#define Wifi_Reset1 0x03
#define Wifi_Reset2 0x04
#define Log_State 0x05
#define GMT_Time 0x10
#define Temporary_Pass 0xDD

// STM32 UART Protokolü (0xA5)
#define STM32_CMD_BATTERY_REPORT 0x04
#define STM32_CMD_EVENT_LOG 0x20
#define STM32_CMD_FACTORY_RESET 0xFF
#define STM32_CMD_WIFI_RESET 0x0F

#define STM32_CMD_SET_SOUND 0x05
#define STM32_CMD_SET_LANGUAGE 0x06
#define STM32_CMD_FIND_DEVICE 0x07
#define STM32_CMD_QUERY_SETTINGS 0x0A
#define STM32_CMD_QUERY_BATTERY 0x0B
#define STM32_CMD_QUERY_TIME 0x0D
#define STM32_CMD_RESPONSE 0x0E

#define STM32_RESP_OK 0x00
#define STM32_RESP_ERROR 0x01
#define STM32_RESP_SERVER_ERR 0x02
#define STM32_RESP_NO_WIFI 0x03
#define STM32_RESP_SERVER_REJ 0x04

// UART Pinleri
#define RX_PIN 16
#define TX_PIN 17
#define BAUD_RATE 115200
#define STREAM_SIZE 256

HardwareSerial SerialUART(1);

static uint8_t stream_buf[STREAM_SIZE];
static size_t stream_len = 0;

// Unified buffers for STM32 connection
static uint8_t stm32_buffer[128];
static size_t stm32_buffer_len = 0;

// BLE Durumları
BLECharacteristic *pCharacteristicTX;
Preferences preferences;
bool wifiConnected = false;
bool deviceConnected = false;
bool shouldStopBLE = false;
bool bleMode = false;

// BLE Alım Tamponu
static uint8_t ble_rx_buffer[512];
static size_t ble_rx_len = 0;

// MQTT Global Değişkenleri
esp_mqtt_client_handle_t mqtt_client = NULL;
bool mqtt_connected = false;
uint16_t mqtt_seq = 0;

// MCU Erişim Talebi Takip Değişkenleri
static uint8_t pending_mcu_cmd = 0;
static uint16_t pending_mcu_seq = 0;
static unsigned long pending_mcu_time = 0;

// MQTT Konuları (Topic'ler)
String topic_cmd;
String topic_reply;
String topic_config;
String topic_lifecycle;
String topic_telemetry;
String topic_event;
String topic_log;
String topic_ack;

// NVS Tercih Değişkenleri
String mqtt_host;
uint16_t mqtt_port = 8883;
String mqtt_client_id;
String mqtt_user;
String mqtt_pass;
String device_secret;
uint16_t session_epoch = 1;

// Zaman ve Durum Yönetimi
time_t serverEpoch = 0;
unsigned long long serverMillis = 0;
bool Remote_Open = false;
unsigned long remoteOpenTimestamp = 0;
const unsigned long REMOTE_OPEN_LOCK_MS = 15000;
uint32_t remote_open_request_id = 0;
uint32_t active_fp_enroll_request_id = 0;
uint32_t active_nfc_enroll_request_id = 0;
bool door_opened_confirmed = false;

WiFiClientSecure client;

// Legacy Frame Struct
typedef struct {
  uint16_t header;
  uint8_t version;
  uint8_t command;
  uint16_t length;
  uint8_t *data;
  uint8_t checksum;
} Frame;

// Prototipler
void resetWiFi();
bool connectToWiFi(const char *ssid, const char *password);
bool loadWiFiCredentials(String &ssid, String &pass);
void saveWiFiCredentials(const char *ssid, const char *pass);
void clearWiFiCredentials();
void factoryReset();
void startBLE();
bool parseNextFrame(Frame *out);
void processIncoming(const uint8_t *buf, size_t len);
void sendStatusUART(uint8_t status);
void sendNetworkStatus(uint8_t statusCode);
void handleSTM32Commands();
void sendSTM32Packet(uint8_t cmd, const uint8_t *payload, uint8_t payload_len);
void sendSTM32SoundSetting(uint8_t enable);
void sendSTM32LanguageSetting(uint8_t english);
void sendSTM32FindDevice();
void sendSTM32QuerySettings();
void sendSTM32QueryBattery();

// NVS Onloi Ayarları
bool loadOnloiConfig() {
  preferences.begin("onloi", true);
  bool claimed = preferences.getUChar("claimed", 0) == 1;
  if (claimed) {
    mqtt_host = preferences.getString("mqtt_host", "");
    mqtt_port = preferences.getUShort("mqtt_port", 8883);
    mqtt_client_id = preferences.getString("mqtt_client_id", "");
    mqtt_user = preferences.getString("mqtt_user", "");
    mqtt_pass = preferences.getString("mqtt_pass", "");
    device_secret = preferences.getString("device_secret", "");
    session_epoch = preferences.getUShort("session_epoch", 1);
  }
  preferences.end();
  return claimed;
}

void saveOnloiConfig(const String &host, uint16_t port, const String &clientId, const String &user, const String &pass, const String &secret, uint16_t epoch) {
  preferences.begin("onloi", false);
  preferences.putUChar("claimed", 1);
  preferences.putString("mqtt_host", host);
  preferences.putUShort("mqtt_port", port);
  preferences.putString("mqtt_client_id", clientId);
  preferences.putString("mqtt_user", user);
  preferences.putString("mqtt_pass", pass);
  preferences.putString("device_secret", secret);
  preferences.putUShort("session_epoch", epoch);
  preferences.end();
}

void saveProvisionToken(const String &token) {
  preferences.begin("onloi", false);
  preferences.putString("prov_token", token);
  preferences.end();
}

String loadProvisionToken() {
  preferences.begin("onloi", true);
  String token = preferences.getString("prov_token", "");
  preferences.end();
  return token;
}

void clearOnloiConfig() {
  preferences.begin("onloi", false);
  preferences.clear();
  preferences.end();
}

void queryDeviceModelFromMCU() {
  preferences.begin("onloi", false);
  String saved_model = preferences.getString("model", "");
  
  if (saved_model.length() > 0) {
    device_code = saved_model;
    ble_name = device_code;
    DEBUG_PRINTF("Kayitli cihaz modeli hafizadan yuklendi: %s\n", device_code.c_str());
    preferences.end();
    return;
  }
  
  DEBUG_PRINTLN("Cihaz modeli hafizada yok, MCU'dan talep ediliyor...");
  
  // A5 89 01 8A 5A
  uint8_t query_packet[] = {0xA5, 0x89, 0x01, 0x8A, 0x5A};
  
  for (int retry = 1; retry <= 10; retry++) {
    DEBUG_PRINTF("Model sorgusu gonderiliyor (Deneme %d/10)...\n", retry);
    
    // Clear RX buffer first
    while (SerialUART.available()) {
      SerialUART.read();
    }
    
    // Send packet
    SerialUART.write(query_packet, sizeof(query_packet));
    
    // Wait up to 250ms for response
    unsigned long start = millis();
    bool got_response = false;
    uint8_t rx_buf[64];
    size_t rx_len = 0;
    
    while (millis() - start < 250) {
      if (SerialUART.available()) {
        uint8_t b = SerialUART.read();
        if (rx_len == 0 && b != 0xA5) {
          continue; // Wait for SOF
        }
        rx_buf[rx_len++] = b;
        
        // Minimum packet length is 5
        if (rx_len >= 5) {
          uint8_t cmd = rx_buf[1];
          uint8_t protocol_len = rx_buf[2];
          uint8_t payload_len = protocol_len - 1;
          
          if (rx_len >= (size_t)(5 + payload_len)) {
            // Check checksum and EOF
            uint16_t sum = cmd + protocol_len;
            for (int i = 0; i < payload_len; i++) {
              sum += rx_buf[3 + i];
            }
            
            if ((sum & 0xFF) == rx_buf[3 + payload_len] && 
                rx_buf[4 + payload_len] == 0x5A) {
              
              if (cmd == 0x89) {
                // Valid model response!
                char model_str[32] = {0};
                memcpy(model_str, rx_buf + 3, payload_len);
                device_code = String(model_str);
                ble_name = device_code;
                
                // Save to NVS
                preferences.putString("model", device_code);
                DEBUG_PRINTF("Cihaz modeli basariyla alindi ve hafizaya kaydedildi: %s\n", device_code.c_str());
                got_response = true;
                break;
              }
            }
            // If it was another command or failed verification, reset buffer to find next 0xA5
            rx_len = 0;
          }
        }
      }
      delay(1);
    }
    
    if (got_response) {
      preferences.end();
      return;
    }
  }
  
  // If we reach here, all 10 retries failed
  device_code = "UNKNOWN";
  ble_name = "UNKNOWN";
  DEBUG_PRINTLN("MCU'dan model yaniti alinamadi! Cihaz modeli UNKNOWN olarak devam ediyor.");
  preferences.end();
}

// Cihaz Bilgisi Hazırlama
void initDeviceId() {
  uint64_t chipid = ESP.getEfuseMac();
  char idStr[32];
  sprintf(idStr, "%llu", chipid);
  device_id = String(idStr);

  preferences.begin("onloi", true); // Open in read-only mode
  device_code = preferences.getString("model", "UNKNOWN");
  preferences.end();

  ble_name = device_code;
  
  DEBUG_PRINT("Unique Device ID: ");
  DEBUG_PRINTLN(device_id);
  DEBUG_PRINT("Device Model Code: ");
  DEBUG_PRINTLN(device_code);
  DEBUG_PRINT("BLE Name: ");
  DEBUG_PRINTLN(ble_name);
}

// CRC16-CCITT
uint16_t calculate_crc16(const uint8_t *data, size_t length) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < length; i++) {
    crc ^= (data[i] << 8);
    for (int j = 0; j < 8; j++) {
      if (crc & 0x8000) {
        crc = (crc << 1) ^ 0x1021;
      } else {
        crc <<= 1;
      }
    }
  }
  return crc;
}

// HMAC-SHA256
bool verify_hmac(const uint8_t *data, size_t data_len, const uint8_t *expected_auth_tag) {
  uint8_t hash[32];
  mbedtls_md_context_t ctx;
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);

  size_t key_len = device_secret.length();
  uint8_t key[64];
  if (key_len == 64) {
    for (size_t i = 0; i < 32; i++) {
      String hexPart = device_secret.substring(i * 2, i * 2 + 2);
      key[i] = (uint8_t)strtol(hexPart.c_str(), NULL, 16);
    }
    key_len = 32;
  } else {
    memcpy(key, device_secret.c_str(), key_len);
  }

  mbedtls_md_hmac_starts(&ctx, key, key_len);
  mbedtls_md_hmac_update(&ctx, data, data_len);
  mbedtls_md_hmac_finish(&ctx, hash);
  mbedtls_md_free(&ctx);

  return memcmp(hash, expected_auth_tag, 8) == 0;
}

// MQTT Gönderim
void publish_mqtt_frame(uint8_t msg_type, uint8_t cmd, const uint8_t *payload, uint16_t payload_len, const String &topic, bool retain = false) {
  if (!mqtt_connected || mqtt_client == NULL) {
    DEBUG_PRINTF("[MQTT TX HATA] Sunucu baglantisi yok! CMD: 0x%02X, Topic: %s verisi gonderilemedi.\n", cmd, topic.c_str());
    return;
  }

  uint8_t frame[512];
  frame[0] = SOF_BYTE;
  frame[1] = VER_BYTE;
  frame[2] = msg_type;
  frame[3] = cmd;

  uint16_t seq = mqtt_seq++;
  frame[4] = (seq >> 8) & 0xFF;
  frame[5] = seq & 0xFF;

  frame[6] = (payload_len >> 8) & 0xFF;
  frame[7] = payload_len & 0xFF;

  frame[8] = 0x00; // STATUS

  struct timeval tv;
  gettimeofday(&tv, NULL);
  uint64_t ts = 0;
  if (tv.tv_sec > 1600000000) {
    ts = (uint64_t)tv.tv_sec * 1000ULL + (tv.tv_usec / 1000ULL);
  }
  for (int i = 0; i < 8; i++) {
    frame[9 + i] = (ts >> (56 - i * 8)) & 0xFF;
  }

  if (payload_len > 0 && payload != NULL) {
    memcpy(frame + 17, payload, payload_len);
  }

  uint16_t crc = calculate_crc16(frame, 17 + payload_len);
  frame[17 + payload_len] = (crc >> 8) & 0xFF;
  frame[17 + payload_len + 1] = crc & 0xFF;

  size_t total_len = 19 + payload_len;
  esp_mqtt_client_publish(mqtt_client, topic.c_str(), (const char*)frame, total_len, 1, retain);
  
  DEBUG_PRINTF("[MQTT TX] Topic: %s, CMD: 0x%02X, Seq: %d, Len: %d\n", topic.c_str(), cmd, seq, total_len);
}

void send_mqtt_ack(uint32_t requestId, uint8_t ackPhase, uint8_t cmdEcho, uint8_t detailCode) {
  uint8_t payload[7];
  payload[0] = (requestId >> 24) & 0xFF;
  payload[1] = (requestId >> 16) & 0xFF;
  payload[2] = (requestId >> 8) & 0xFF;
  payload[3] = requestId & 0xFF;
  payload[4] = ackPhase;
  payload[5] = cmdEcho;
  payload[6] = detailCode;

  publish_mqtt_frame(MSG_TYPE_ACK, CMD_ACK, payload, 7, topic_ack);
}

// MQTT İşleyicileri
void handle_mqtt_command(uint8_t cmd, uint16_t seq, const uint8_t *payload, uint16_t payload_len) {
  DEBUG_PRINTF("[MQTT CMD] CMD: 0x%02X, Len: %d\n", cmd, payload_len);

  uint32_t requestId = 0;
  if (payload_len >= 4) {
    requestId = (payload[0] << 24) | (payload[1] << 16) | (payload[2] << 8) | payload[3];
  }

  switch (cmd) {
    case CMD_REMOTE_OPEN: {
      DEBUG_PRINTLN("Sunucudan UZAKTAN AÇMA Komutu Alındı");
      send_mqtt_ack(requestId, 0x01, CMD_REMOTE_OPEN, 0x00); // RECEIVED
      
      remote_open_request_id = requestId;
      Remote_Open = true;
      remoteOpenTimestamp = millis();

      // STM32'ye UART üzerinden A5 40 01 41 5A şeklinde kapı açma isteği gönder
      sendSTM32Packet(0x40, NULL, 0);
      
      send_mqtt_ack(requestId, 0x02, CMD_REMOTE_OPEN, 0x00); // EXECUTING
      break;
    }
    case CMD_FACTORY_RESET: {
      DEBUG_PRINTLN("Sunucudan FABRİKA AYARLARINA SIFIRLAMA Komutu Alındı");
      send_mqtt_ack(requestId, 0x03, CMD_FACTORY_RESET, 0x00); // COMPLETED
      delay(1000);
      clearOnloiConfig();
      clearWiFiCredentials();
      ESP.restart();
      break;
    }
    case CMD_FP_ENROLL_START: {
      DEBUG_PRINTLN("Sunucudan PARMAK İZİ KAYDET Komutu Alındı");
      active_fp_enroll_request_id = requestId;
      send_mqtt_ack(requestId, 0x01, CMD_FP_ENROLL_START, 0x00);
      
      uint8_t ev_payload[4];
      ev_payload[0] = (requestId >> 24) & 0xFF;
      ev_payload[1] = (requestId >> 16) & 0xFF;
      ev_payload[2] = (requestId >> 8) & 0xFF;
      ev_payload[3] = requestId & 0xFF;
      publish_mqtt_frame(MSG_TYPE_EVENT, 0x24, ev_payload, 4, topic_event); // FP_ENROLL_READY

      // MCU'ya 0x46 komutunu gönder (A5 46 01 47 5A)
      sendSTM32Packet(CMD_FP_ENROLL_START, NULL, 0);
      break;
    }
    case CMD_CARD_ENROLL_START: {
      DEBUG_PRINTLN("Sunucudan KART KAYDET Komutu Alındı");
      active_nfc_enroll_request_id = requestId;
      send_mqtt_ack(requestId, 0x01, CMD_CARD_ENROLL_START, 0x00);

      // MCU'ya 0x47 komutunu gönder (A5 47 01 48 5A)
      sendSTM32Packet(CMD_CARD_ENROLL_START, NULL, 0);
      break;
    }
    default:
      break;
  }
}

void handle_mqtt_response(uint8_t cmd, uint16_t seq, uint8_t status, const uint8_t *payload, uint16_t payload_len) {
  DEBUG_PRINTF("[MQTT RESP] CMD: 0x%02X, Status: 0x%02X\n", cmd, status);

  switch (cmd) {
    case CMD_PIN_ENTERED:
    case CMD_NFC_SCANNED:
    case CMD_BUTTON_UNLOCK_REQ:
    case CMD_FINGERPRINT_SCANNED:
    case CMD_QR_SCANNED: {
      if (pending_mcu_cmd == cmd && seq == pending_mcu_seq) {
        uint8_t status_code = (status == 0x00) ? 0x00 : 0x01; // 0x00: onay, 0x01: red
        sendSTM32Packet(cmd, &status_code, 1);
        pending_mcu_cmd = 0; // Clear pending
      } else {
        // Fallback to legacy/original behavior
        if (status == 0x00) {
          DEBUG_PRINTLN("Erişim İzni Sunucu Tarafından Onaylandı! Kilit Açılıyor.");
          uint8_t status_open = 0x01;
          sendSTM32Packet(0x40, &status_open, 1);
        } else {
          DEBUG_PRINTLN("Erişim Sunucu Tarafından Reddedildi!");
          uint8_t status_deny = 0x00;
          sendSTM32Packet(0x40, &status_deny, 1);
        }
      }
      break;
    }
    case CMD_TIME_SYNC: {
      if (status == 0x00 && payload_len >= 8) {
        uint64_t unixMs = 0;
        for (int i = 0; i < 8; i++) {
          unixMs = (unixMs << 8) | payload[i];
        }
        serverEpoch = (time_t)(unixMs / 1000ULL);
        serverMillis = unixMs;

        struct timeval tv = {.tv_sec = serverEpoch, .tv_usec = 0};
        settimeofday(&tv, nullptr);
        DEBUG_PRINTF("Zaman Senkronizasyonu Başarılı: %s", ctime(&serverEpoch));

        uint8_t t_buf[4];
        uint32_t t_now = (uint32_t)serverEpoch;
        t_buf[0] = (t_now >> 24) & 0xFF;
        t_buf[1] = (t_now >> 16) & 0xFF;
        t_buf[2] = (t_now >> 8) & 0xFF;
        t_buf[3] = t_now & 0xFF;
        sendSTM32Packet(STM32_CMD_QUERY_TIME, t_buf, 4);
      }
      break;
    }
    case CMD_OFFLINE_LOG_BATCH: {
      if (status == 0x00 && payload_len >= 3) {
        uint16_t accepted = (payload[0] << 8) | payload[1];
        uint8_t err = payload[2];
        DEBUG_PRINTF("Çevrimdışı loglar sunucuya yazıldı. Adet: %d, Hata: %d\n", accepted, err);
        if (err == 0x00) {
          uint8_t resp_data[2] = {STM32_CMD_EVENT_LOG, STM32_RESP_OK};
          sendSTM32Packet(STM32_CMD_RESPONSE, resp_data, 2); // FIFO temizleme komutu
        }
      }
      break;
    }
    default:
      break;
  }
}

void process_mqtt_message(const char *topic, size_t topic_len, const char *data, size_t data_len) {
  if (data_len < 19) return;

  const uint8_t *frame = (const uint8_t *)data;
  if (frame[0] != SOF_BYTE || frame[1] != VER_BYTE) return;

  uint16_t payload_len = (frame[6] << 8) | frame[7];
  uint8_t msg_type = frame[2];
  uint8_t cmd = frame[3];
  uint16_t seq = (frame[4] << 8) | frame[5];
  uint8_t status = frame[8];

  size_t total_expected = 19 + payload_len;
  if (msg_type == MSG_TYPE_COMMAND) {
    total_expected += 8; // Komut paketlerinde 8 baytlık HMAC imzası eklenir
  }

  if (data_len < total_expected) return;

  uint16_t calculated;
  uint16_t received;

  if (msg_type == MSG_TYPE_COMMAND) {
    calculated = calculate_crc16(frame, 17 + payload_len + 8);
    received = (frame[17 + payload_len + 8] << 8) | frame[17 + payload_len + 8 + 1];
  } else {
    calculated = calculate_crc16(frame, 17 + payload_len);
    received = (frame[17 + payload_len] << 8) | frame[17 + payload_len + 1];
  }

  if (calculated != received) {
    DEBUG_PRINTF("[MQTT RX HATA] CRC Hatası! Alınan Uzunluk: %d, Beklenen Uzunluk: %d\n", data_len, total_expected);
    DEBUG_PRINTF("[MQTT RX HATA] Hesaplanan CRC: 0x%04X, Gelen CRC: 0x%04X\n", calculated, received);
    DEBUG_PRINT("[MQTT RX HATA] Gelen Baytlar: ");
    for (size_t i = 0; i < data_len; i++) {
      DEBUG_PRINTF("%02X ", frame[i]);
    }
    DEBUG_PRINTLN();
    return;
  }

  if (msg_type == MSG_TYPE_COMMAND) {
    // Komut paketlerinde payload_len doğrudan gerçek payload uzunluğudur.
    // HMAC imzası (8 bayt) ise payload'un hemen ardındadır.
    const uint8_t *auth_tag_ptr = frame + 17 + payload_len;

    bool verified = verify_hmac(frame, 17 + payload_len, auth_tag_ptr);
    if (!verified) {
      DEBUG_PRINTLN("MQTT Komut HMAC Doğrulaması Başarısız! (GEÇİCİ OLARAK BYPASS EDİLDİ)");
      ////////////////////////////////////////////////  hmac doğrulama kapatıldı.
      /*
      uint32_t reqId = 0;
      if (payload_len >= 4) {
        reqId = (frame[17] << 24) | (frame[18] << 16) | (frame[19] << 8) | frame[20];
      }
      send_mqtt_ack(reqId, 0x04, cmd, 0x01); // FAILED (Unauthorized)
      return;
      */
    }

    handle_mqtt_command(cmd, seq, frame + 17, payload_len);
  } else {
    handle_mqtt_response(cmd, seq, status, frame + 17, payload_len);
  }
}

void on_mqtt_connect() {
  uint8_t reg_payload[3];
  reg_payload[0] = (session_epoch >> 8) & 0xFF;
  reg_payload[1] = session_epoch & 0xFF;
  reg_payload[2] = 0x00; // status OK
  publish_mqtt_frame(MSG_TYPE_EVENT, CMD_REGISTRATION, reg_payload, 3, topic_lifecycle, true);

  uint8_t sess_payload[3];
  sess_payload[0] = 0x01; // wakeReason button
  sess_payload[1] = 85;   // default 85%
  sess_payload[2] = 0x00;
  publish_mqtt_frame(MSG_TYPE_EVENT, CMD_SESSION_START, sess_payload, 3, topic_lifecycle, true);

  publish_mqtt_frame(MSG_TYPE_REQUEST, CMD_TIME_SYNC, NULL, 0, topic_cmd);
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
  esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
  switch (event->event_id) {
    case MQTT_EVENT_CONNECTED:
      DEBUG_PRINTLN("[MQTT] Sunucu baglantisi BASARILI! Abonelikler olusturuluyor...");
      mqtt_connected = true;
      esp_mqtt_client_subscribe(mqtt_client, topic_cmd.c_str(), 1);
      esp_mqtt_client_subscribe(mqtt_client, topic_reply.c_str(), 1);
      esp_mqtt_client_subscribe(mqtt_client, topic_config.c_str(), 1);
      on_mqtt_connect();
      break;
    case MQTT_EVENT_DISCONNECTED:
      DEBUG_PRINTLN("[MQTT] Sunucu baglantisi KESILDI veya BAGLANILAMADI!");
      mqtt_connected = false;
      break;
    case MQTT_EVENT_DATA:
      process_mqtt_message(event->topic, event->topic_len, event->data, event->data_len);
      break;
    case MQTT_EVENT_ERROR:
      DEBUG_PRINTLN("[MQTT] Hata olayi tetiklendi!");
      break;
    default:
      break;
  }
}

void start_mqtt() {
  if (mqtt_client != NULL) return;

  topic_cmd = "onloi/v1/d/" + device_id + "/cmd";
  topic_reply = "onloi/v1/d/" + device_id + "/reply";
  topic_config = "onloi/v1/d/" + device_id + "/config";
  topic_lifecycle = "onloi/v1/d/" + device_id + "/lifecycle";
  topic_telemetry = "onloi/v1/d/" + device_id + "/telemetry";
  topic_event = "onloi/v1/d/" + device_id + "/event";
  topic_log = "onloi/v1/d/" + device_id + "/log";
  topic_ack = "onloi/v1/d/" + device_id + "/ack";

  esp_mqtt_client_config_t mqtt_cfg = {};

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  mqtt_cfg.broker.address.hostname = mqtt_host.c_str();
  mqtt_cfg.broker.address.port = mqtt_port;
  if (mqtt_port == 8883 || mqtt_port == 8884 || mqtt_port == 443) {
    mqtt_cfg.broker.address.transport = MQTT_TRANSPORT_OVER_SSL;
    mqtt_cfg.broker.verification.crt_bundle_attach = esp_crt_bundle_attach;
  } else {
    mqtt_cfg.broker.address.transport = MQTT_TRANSPORT_OVER_TCP;
  }

  mqtt_cfg.credentials.client_id = mqtt_client_id.c_str();
  mqtt_cfg.credentials.username = mqtt_user.c_str();
  mqtt_cfg.credentials.authentication.password = mqtt_pass.c_str();
  mqtt_cfg.session.keepalive = 30;
  mqtt_cfg.session.disable_clean_session = false;
  mqtt_cfg.session.last_will.topic = topic_lifecycle.c_str();
  mqtt_cfg.session.last_will.msg = "";
  mqtt_cfg.session.last_will.msg_len = 0;
  mqtt_cfg.session.last_will.qos = 1;
  mqtt_cfg.session.last_will.retain = true;
#else
  mqtt_cfg.host = mqtt_host.c_str();
  mqtt_cfg.port = mqtt_port;
  if (mqtt_port == 8883 || mqtt_port == 8884 || mqtt_port == 443) {
    mqtt_cfg.transport = MQTT_TRANSPORT_OVER_SSL;
    mqtt_cfg.crt_bundle_attach = esp_crt_bundle_attach;
  } else {
    mqtt_cfg.transport = MQTT_TRANSPORT_OVER_TCP;
  }
  mqtt_cfg.client_id = mqtt_client_id.c_str();
  mqtt_cfg.username = mqtt_user.c_str();
  mqtt_cfg.password = mqtt_pass.c_str();
  mqtt_cfg.keepalive = 30;
  mqtt_cfg.disable_clean_session = 0;
  mqtt_cfg.lwt_topic = topic_lifecycle.c_str();
  mqtt_cfg.lwt_msg = "";
  mqtt_cfg.lwt_msg_len = 0;
  mqtt_cfg.lwt_qos = 1;
  mqtt_cfg.lwt_retain = 1;
#endif

  DEBUG_PRINTF("[MQTT] Sunucuya baglanma baslatiliyor... Host: %s, Port: %d\n", mqtt_host.c_str(), mqtt_port);
  DEBUG_PRINTF("[MQTT DETAY] ClientID: %s, User: %s, Pass: %s, Secret: %s\n",
               mqtt_client_id.c_str(), mqtt_user.c_str(), mqtt_pass.c_str(), device_secret.c_str());
  mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
  esp_mqtt_client_register_event(mqtt_client, (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
  esp_mqtt_client_start(mqtt_client);
}

// HTTPS Claim İşlemi
bool execute_claim() {
  String prov_token = loadProvisionToken();
  DEBUG_PRINTF("Claim için yüklenen provision token: %s\n", prov_token.c_str());
  if (prov_token.length() == 0) {
    DEBUG_PRINTLN("Claim için geçerli bir provision token bulunamadı.");
    return false;
  }

  StaticJsonDocument<256> doc;
  doc["deviceId"] = device_id;
  doc["deviceCode"] = device_code;
  doc["provisioningToken"] = prov_token;

  String body;
  serializeJson(doc, body);
  DEBUG_PRINTF("Claim istek gövdesi (Body): %s\n", body.c_str());

  HTTPClient http;
  http.begin(client, CLAIM_URL);
  http.addHeader("Content-Type", "application/json");

  DEBUG_PRINTF("HTTPS Claim isteği gönderiliyor... Hedef URL: %s\n", CLAIM_URL);
  int code = http.POST(body);
  String payload = http.getString();
  http.end();

  DEBUG_PRINTF("Claim HTTP Kodu: %d, Yanıt: %s\n", code, payload.c_str());

  if (code == 200) {
    StaticJsonDocument<1024> res;
    DeserializationError err = deserializeJson(res, payload);
    if (!err) {
      String host = res["mqtt"]["host"];
      uint16_t port = res["mqtt"]["port"];
      String clientId = res["mqtt"]["clientId"];
      String username = res["mqtt"]["username"];
      String password = res["mqtt"]["password"];
      String secret = res["deviceSecret"];
      uint16_t epoch = res["sessionEpoch"];

      saveOnloiConfig(host, port, clientId, username, password, secret, epoch);
      saveProvisionToken(""); // Tokenı temizle
      DEBUG_PRINTLN("[CLAIM SUCCESS] Sunucudan MQTT bilgileri alindi ve kaydedildi!");
      DEBUG_PRINTF("[CLAIM DETAY] Host: %s, Port: %d, ClientID: %s, User: %s, Pass: %s, Secret: %s, Epoch: %d\n",
                   host.c_str(), port, clientId.c_str(), username.c_str(), password.c_str(), secret.c_str(), epoch);
      return true;
    } else {
      DEBUG_PRINTLN("[CLAIM ERROR] Sunucu yaniti JSON parse hatasi!");
    }
  } else {
    DEBUG_PRINTF("[CLAIM ERROR] HTTP POST hatasi veya gecersiz token! Kod: %d, Yanit: %s\n", code, payload.c_str());
  }
  return false;
}

// BLE Binary Komut İşleme
void handle_ble_command(uint8_t cmd, uint16_t seq, const uint8_t *payload, uint16_t payload_len) {
  DEBUG_PRINTF("[BLE RX] CMD: 0x%02X, Seq: %d, Len: %d\n", cmd, seq, payload_len);
  switch (cmd) {
    case CMD_HANDSHAKE: {
      uint8_t resp_payload[24] = {0};
      memcpy(resp_payload, device_id.c_str(), min((size_t)16, device_id.length()));
      
      char code_buf[8] = {0};
      strncpy(code_buf, device_code.c_str(), sizeof(code_buf) - 1);
      memcpy(resp_payload + 16, code_buf, 8);
      
      send_ble_response(CMD_HANDSHAKE, seq, 0x00, resp_payload, 24);
      break;
    }
    case CMD_PROVISION: {
      if (payload_len >= 32) {
        char token_hex[65];
        for (int i = 0; i < 32; i++) {
          sprintf(token_hex + (i * 2), "%02X", payload[i]);
        }
        saveProvisionToken(String(token_hex));
        DEBUG_PRINTF("Provision token kaydedildi: %s\n", token_hex);

        uint8_t result = 0x00; // Başarılı
        send_ble_response(CMD_PROVISION, seq, 0x00, &result, 1);
      } else {
        uint8_t result = 0x01; // Başarısız
        send_ble_response(CMD_PROVISION, seq, 0x00, &result, 1);
      }
      break;
    }
    case CMD_GET_SSIDS: {
      DEBUG_PRINTLN("Wi-Fi ağ taraması yapılıyor...");
      WiFi.mode(WIFI_STA);
      WiFi.disconnect(true);
      delay(100);
      int n = WiFi.scanNetworks();
      DEBUG_PRINTF("Tarama bitti: %d ağ bulundu\n", n);

      uint8_t scan_payload[512];
      uint8_t count = min(n, 10); // Maksimum 10 ağ dönecek
      scan_payload[0] = 0;
      size_t offset = 1;
      uint8_t added_count = 0;

      for (int i = 0; i < count; i++) {
        String ssid = WiFi.SSID(i);
        uint8_t ssid_len = min((size_t)32, ssid.length());

        // Taşma kontrolü: BSSID(6) + RSSI(1) + Channel(1) + Len(1) + SSID(ssid_len)
        if (offset + 9 + ssid_len > sizeof(scan_payload)) {
          break;
        }

        uint8_t bssid[6];
        uint8_t *bssid_ptr = WiFi.BSSID(i);
        if (bssid_ptr != NULL) {
          memcpy(bssid, bssid_ptr, 6);
        } else {
          memset(bssid, 0, 6);
        }
        memcpy(scan_payload + offset, bssid, 6);
        offset += 6;

        int8_t rssi = WiFi.RSSI(i);
        scan_payload[offset++] = (uint8_t)rssi;

        int channel = WiFi.channel(i);
        scan_payload[offset++] = (channel < 15) ? 0x01 : 0x02; // 2.4 vs 5

        scan_payload[offset++] = ssid_len;

        memcpy(scan_payload + offset, ssid.c_str(), ssid_len);
        offset += ssid_len;

        added_count++;

        DEBUG_PRINTF("[WiFi Tarama] Ag #%d: SSID: \"%s\", RSSI: %d dBm, Kanal: %d, BSSID: %02X:%02X:%02X:%02X:%02X:%02X\n",
                     i + 1, ssid.c_str(), rssi, channel,
                     bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
      }
      scan_payload[0] = added_count;

      send_ble_response(CMD_GET_SSIDS, seq, 0x00, scan_payload, offset);
      WiFi.scanDelete();
      break;
    }
    case CMD_CONNECT_WIFI: {
      if (payload_len >= 8) {
        uint8_t ssid_len = payload[0];
        char ssid[33] = {0};
        memcpy(ssid, payload + 1, min((int)ssid_len, 32));

        size_t pass_offset = 1 + ssid_len + 6;
        uint8_t pass_len = payload[pass_offset];
        char pass[65] = {0};
        memcpy(pass, payload + pass_offset + 1, min((int)pass_len, 64));

        DEBUG_PRINTF("Bağlanılıyor: SSID=%s, Sifre=%s\n", ssid, pass);
        if (connectToWiFi(ssid, pass)) {
          wifiConnected = true;
          saveWiFiCredentials(ssid, pass);

          uint8_t result = 0x00; // Başarılı
          send_ble_response(CMD_CONNECT_WIFI, seq, 0x00, &result, 1);
          shouldStopBLE = true;
        } else {
          uint8_t result = 0x01; // Başarısız
          send_ble_response(CMD_CONNECT_WIFI, seq, 0x00, &result, 1);
        }
      }
      break;
    }
    default:
      break;
  }
}

void send_ble_response(uint8_t cmd, uint16_t seq, uint8_t status, const uint8_t *payload, uint16_t payload_len) {
  if (payload_len > 490) {
    payload_len = 490; // Bellek sınırını aşmasını engelle
  }
  uint8_t resp[512];
  resp[0] = SOF_BYTE;
  resp[1] = VER_BYTE;
  resp[2] = MSG_TYPE_RESPONSE;
  resp[3] = cmd;
  resp[4] = (seq >> 8) & 0xFF;
  resp[5] = seq & 0xFF;
  resp[6] = (payload_len >> 8) & 0xFF;
  resp[7] = payload_len & 0xFF;
  resp[8] = status;

  uint64_t ts = 0;
  struct timeval tv;
  gettimeofday(&tv, NULL);
  if (tv.tv_sec > 1600000000) {
    ts = (uint64_t)tv.tv_sec * 1000ULL + (tv.tv_usec / 1000ULL);
  }
  for (int i = 0; i < 8; i++) {
    resp[9 + i] = (ts >> (56 - i * 8)) & 0xFF;
  }

  if (payload_len > 0 && payload != NULL) {
    memcpy(resp + 17, payload, payload_len);
  }

  uint16_t crc = calculate_crc16(resp, 17 + payload_len);
  resp[17 + payload_len] = (crc >> 8) & 0xFF;
  resp[17 + payload_len + 1] = crc & 0xFF;

  size_t total_len = 19 + payload_len;

  DEBUG_PRINT("[BLE TX FRAME] ");
  for (size_t i = 0; i < total_len; i++) {
    DEBUG_PRINTF("%02X ", resp[i]);
  }
  DEBUG_PRINTLN();

  const size_t MTU_CHUNK = 19;
  for (size_t pos = 0; pos < total_len; pos += MTU_CHUNK) {
    size_t chunkSize = min(MTU_CHUNK, total_len - pos);
    pCharacteristicTX->setValue(resp + pos, chunkSize);
    pCharacteristicTX->notify();
    delay(50);
  }
}

class MyCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pChar) override {
    String rx = pChar->getValue();
    size_t len = rx.length();

    DEBUG_PRINT("[BLE RX RAW CHUNK] ");
    for (size_t i = 0; i < len; i++) {
      DEBUG_PRINTF("%02X ", (uint8_t)rx[i]);
    }
    DEBUG_PRINTLN();

    if (ble_rx_len + len > sizeof(ble_rx_buffer)) {
      ble_rx_len = 0;
    }
    memcpy(ble_rx_buffer + ble_rx_len, rx.c_str(), len);
    ble_rx_len += len;

    while (ble_rx_len >= 19) {
      size_t sof_idx = 0;
      while (sof_idx < ble_rx_len && ble_rx_buffer[sof_idx] != SOF_BYTE) {
        sof_idx++;
      }
      if (sof_idx > 0) {
        memmove(ble_rx_buffer, ble_rx_buffer + sof_idx, ble_rx_len - sof_idx);
        ble_rx_len -= sof_idx;
        continue;
      }

      if (ble_rx_buffer[1] != VER_BYTE) {
        memmove(ble_rx_buffer, ble_rx_buffer + 1, --ble_rx_len);
        continue;
      }

      uint16_t payload_len = (ble_rx_buffer[6] << 8) | ble_rx_buffer[7];
      size_t total_expected = 19 + payload_len;

      if (ble_rx_len < total_expected) {
        break;
      }

      uint16_t calculated = calculate_crc16(ble_rx_buffer, 17 + payload_len);
      uint16_t received = (ble_rx_buffer[17 + payload_len] << 8) | ble_rx_buffer[17 + payload_len + 1];

      if (calculated != received) {
        DEBUG_PRINTLN("[BLE] CRC Uyuşmazlığı!");
        memmove(ble_rx_buffer, ble_rx_buffer + 1, --ble_rx_len);
        continue;
      }

      DEBUG_PRINT("[BLE RX FRAME] ");
      for (size_t i = 0; i < total_expected; i++) {
        DEBUG_PRINTF("%02X ", ble_rx_buffer[i]);
      }
      DEBUG_PRINTLN();

      uint8_t cmd = ble_rx_buffer[3];
      uint16_t seq = (ble_rx_buffer[4] << 8) | ble_rx_buffer[5];
      uint8_t *payload = ble_rx_buffer + 17;

      handle_ble_command(cmd, seq, payload, payload_len);

      memmove(ble_rx_buffer, ble_rx_buffer + total_expected, ble_rx_len - total_expected);
      ble_rx_len -= total_expected;
    }
  }
};

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) override {
    deviceConnected = true;
    DEBUG_PRINTLN("🔌 BLE bağlandı.");
    delay(100);
  }

  void onDisconnect(BLEServer *pServer) override {
    deviceConnected = false;
    DEBUG_PRINTLN("🔌 BLE bağlantısı kesildi.");
    BLEDevice::startAdvertising();
  }
};

void startBLE() {
  BLEDevice::init(ble_name.c_str());
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);

  // RX karakteristiği oluştur
  BLECharacteristic *pCharacteristicRX = pService->createCharacteristic(
      CHARACTERISTIC_UUID_RX, BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
  pCharacteristicRX->setCallbacks(new MyCallbacks());

  // TX karakteristiği oluştur (ESP32'den mobil uygulamaya giden bildirimler için)
  pCharacteristicTX = pService->createCharacteristic(
      CHARACTERISTIC_UUID_TX, BLECharacteristic::PROPERTY_NOTIFY);
  pCharacteristicTX->addDescriptor(new BLE2902());

  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();

  BLEAdvertisementData oAdvertisementData = BLEAdvertisementData();
  oAdvertisementData.setFlags(0x06);
  oAdvertisementData.setCompleteServices(BLEUUID(SERVICE_UUID));
  pAdvertising->setAdvertisementData(oAdvertisementData);

  BLEAdvertisementData oScanResponseData = BLEAdvertisementData();
  oScanResponseData.setName(ble_name.c_str());
  pAdvertising->setScanResponseData(oScanResponseData);

  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMaxPreferred(0x12);

  BLEDevice::startAdvertising();
  bleMode = true;
  DEBUG_PRINTLN("BLE Ayarlandı ve Reklam Yayını Başlatıldı!");
}

bool connectToWiFi(const char *ssid, const char *password) {
  DEBUG_PRINT("WiFi Bağlanılıyor: ");
  DEBUG_PRINTLN(ssid);
  WiFi.begin(ssid, password);
  unsigned long startTime = millis();

  while (WiFi.status() != WL_CONNECTED && (millis() - startTime) < 15000) {
    delay(500);
    DEBUG_PRINT(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    DEBUG_PRINTLN("\nWiFi Bağlantısı Başarılı!");
    DEBUG_PRINT("Yerel IP: ");
    DEBUG_PRINTLN(WiFi.localIP());
    bleMode = false;
    return true;
  } else {
    DEBUG_PRINTLN("\nWiFi Bağlantısı Başarısız!");
    return false;
  }
}

bool loadWiFiCredentials(String &ssid, String &pass) {
  preferences.begin("wifi", true);
  ssid = preferences.getString("ssid", "");
  pass = preferences.getString("pass", "");
  preferences.end();
  return ssid.length() > 0 && pass.length() > 0;
}

void saveWiFiCredentials(const char *ssid, const char *pass) {
  preferences.begin("wifi", false);
  preferences.putString("ssid", ssid);
  preferences.putString("pass", pass);
  preferences.end();
}

void clearWiFiCredentials() {
  preferences.begin("wifi", false);
  preferences.clear();
  preferences.end();
}

void factoryReset() {
  clearWiFiCredentials();
  clearOnloiConfig();
  ESP.restart();
}

// 55AA Eski Protokol Ayrıştırıcı
bool parseNextFrame(Frame *out) {
  if (stream_len < 7) return false;

  size_t i;
  for (i = 0; i + 1 < stream_len; ++i) {
    if (stream_buf[i] == 0x55 && stream_buf[i + 1] == 0xAA)
      break;
  }
  if (i + 1 >= stream_len) {
    stream_len = 0;
    return false;
  }
  if (i) {
    memmove(stream_buf, stream_buf + i, stream_len - i);
    stream_len -= i;
  }
  if (stream_len < 7) return false;

  out->header = (stream_buf[0] << 8) | stream_buf[1];
  out->version = stream_buf[2];
  out->command = stream_buf[3];
  out->length = (stream_buf[4] << 8) | stream_buf[5];

  size_t full_len = 6 + out->length + 1;
  if (stream_len < full_len) return false;

  if (out->command == 0xDD) {
    if (out->length) {
      out->data = (uint8_t *)malloc(out->length + 1);
      for (size_t j = 0; j < out->length; ++j) {
        out->data[j] = stream_buf[6 + j] % 10;
      }
    } else {
      out->data = NULL;
    }
  } else {
    if (out->length) {
      out->data = (uint8_t *)malloc(out->length);
      memcpy(out->data, stream_buf + 6, out->length);
    } else {
      out->data = NULL;
    }
  }
  out->checksum = stream_buf[6 + out->length];

  uint32_t sum = 0;
  for (size_t j = 0; j < 6 + out->length; ++j)
    sum += stream_buf[j];
  if ((uint8_t)(sum & 0xFF) != out->checksum) {
    free(out->data);
    memmove(stream_buf, stream_buf + 2, --stream_len);
    return parseNextFrame(out);
  }

  memmove(stream_buf, stream_buf + full_len, stream_len - full_len);
  stream_len -= full_len;
  return true;
}

void processIncoming(const uint8_t *buf, size_t len) {
  if (stream_len + len > STREAM_SIZE) stream_len = 0;
  memcpy(stream_buf + stream_len, buf, len);
  stream_len += len;
}

void sendStatusUART(uint8_t status) {
  uint8_t packet[8] = {
      0x55, 0xAA, 0x00, 0xDD, 0x00, 0x01, status, 0x00
  };
  if (status == 0x01) {
    packet[7] = 0xDE;
  } else if (status == 0x00) {
    packet[7] = 0xDD;
  } else if (status == 0x05) {
    packet[7] = 0xE2;
  }

  DEBUG_PRINT(">>> [MCU TX RAW (55AA)] ");
  for (size_t i = 0; i < sizeof(packet); ++i) {
    DEBUG_PRINTF("%02X ", packet[i]);
  }
  DEBUG_PRINTLN();
  SerialUART.write(packet, sizeof(packet));
}

void sendNetworkStatus(uint8_t statusCode) {
  uint8_t buf[8];
  buf[0] = 0x55;
  buf[1] = 0xAA;
  buf[2] = Version;
  buf[3] = 0x02; // Wifi_Status
  buf[4] = 0x00;
  buf[5] = 0x01;
  buf[6] = statusCode;

  uint16_t sum = 0;
  for (int i = 0; i <= 6; ++i)
    sum += buf[i];
  buf[7] = sum & 0xFF;

  DEBUG_PRINT(">>> [MCU TX RAW (55AA)] ");
  for (size_t i = 0; i < sizeof(buf); ++i) {
    DEBUG_PRINTF("%02X ", buf[i]);
  }
  DEBUG_PRINTLN();
  SerialUART.write(buf, sizeof(buf));
}

void setup() {
  Serial.begin(115200);
  delay(100);

  WiFi.mode(WIFI_STA);
  initDeviceId();
  WiFi.setHostname(ble_name.c_str());

  pinMode(RX_PIN, INPUT_PULLUP);
  SerialUART.begin(BAUD_RATE, SERIAL_8N1, RX_PIN, TX_PIN);
  DEBUG_PRINTLN("UART1 Echo hazir. TX=17, RX=16, Baud=115200");

  // Query model name from MCU if not already known
  if (device_code == "UNKNOWN") {
    queryDeviceModelFromMCU();
    WiFi.setHostname(ble_name.c_str());
  }

  client.setInsecure(); // Geliştirme/test kolaylığı için TLS doğrulaması kapalı

  // Onloi ayarlarını yükle
  bool claimed = loadOnloiConfig();

  String savedSsid, savedPass;
  if (loadWiFiCredentials(savedSsid, savedPass)) {
    if (connectToWiFi(savedSsid.c_str(), savedPass.c_str())) {
      wifiConnected = true;
      
      // Eğer Claim yapılmamışsa, yap
      if (!claimed) {
        if (execute_claim()) {
          claimed = loadOnloiConfig(); // Tekrar yükle
        }
      }
      
      if (claimed) {
        start_mqtt();
      }

////////////////////////////////////////////////////// sil yukarıyı ekle
// YENİ - Test modu: claim olmadan direkt bağlan
//mqtt_host = "192.168.1.223";  // buraya PC'nin IP'sini yaz
//mqtt_port = 1883;
//mqtt_client_id = "onloi_" + device_id;
//mqtt_user = "";
//mqtt_pass = "";
//device_secret = "";

// Transport SSL yerine TCP yap
//start_mqtt();
////////////////////////////////////////////////////////

    }
  } else {
    startBLE();
  }
}

void loop() {
#ifdef ENABLE_SERIAL_TEST
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    if (input.length() > 0) {
      if (input.startsWith("0x") || input.startsWith("0X")) {
        input = input.substring(2);
      }
      input.replace(" ", "");
      for (int i = 0; i < input.length(); i += 2) {
        String hexByte = input.substring(i, i + 2);
        uint8_t rawByte = (uint8_t)strtol(hexByte.c_str(), NULL, 16);
        SerialUART.write(rawByte);
        DEBUG_PRINTF("RAW TX (Test): %02X\n", rawByte);
      }
    }
  }
#endif

  unsigned long now = millis();

  // UZAKTAN AÇMA Zaman aşımı kontrolü
  if (Remote_Open && (now - remoteOpenTimestamp >= REMOTE_OPEN_LOCK_MS)) {
    Remote_Open = false;
    DEBUG_PRINTLN("Uzaktan açma süresi doldu.");
  }

  // MCU Erişim İsteği Zaman Aşımı Kontrolü (15 sn)
  if (pending_mcu_cmd != 0 && (now - pending_mcu_time >= 15000)) {
    DEBUG_PRINTF("[TIMEOUT] MCU komut zamanaşımı: 0x%02X\n", pending_mcu_cmd);
    uint8_t timeout_code = 0x02; // STATUS 0x02 = timeout
    sendSTM32Packet(pending_mcu_cmd, &timeout_code, 1);
    pending_mcu_cmd = 0;
  }

  // BLE Kapatma ve yeniden başlatma isteği
  if (shouldStopBLE) {
    delay(1000);
    DEBUG_PRINTLN("Wi-Fi bilgileri kaydedildi, cihaz yeniden başlatılıyor...");
    ESP.restart();
  }

  // Read all available bytes from SerialUART and feed both buffers (no block, no timeout)
  static unsigned long last_rx_time = 0;
  while (SerialUART.available()) {
    uint8_t c = SerialUART.read();
    
    // Group bytes by time delay to print on the same line (50ms gap starts a new line)
    unsigned long current_time = millis();
    if (current_time - last_rx_time > 50) {
      DEBUG_PRINT("\n[MCU RX RAW]: ");
    }
    DEBUG_PRINTF("%02X ", c);
    last_rx_time = current_time;
    
    // Feed to new protocol buffer (A5)
    if (stm32_buffer_len < sizeof(stm32_buffer)) {
      stm32_buffer[stm32_buffer_len++] = c;
    }
    
    // Feed to legacy protocol buffer (55 AA)
    if (stream_len < STREAM_SIZE) {
      stream_buf[stream_len++] = c;
    }
  }

  // UART Okuma ve STM32 ile haberleşme
  handleSTM32Commands();

  // Eski 55AA paketlerini yeni MQTT olaylarına köprüleme
  Frame f;
  while (parseNextFrame(&f)) {
    switch (f.command) {
      case Wifi_Reset2: {
        DEBUG_PRINTLN("WIFI RESET2 komutu tetiklendi!");
        resetWiFi();
        break;
      }
      case Log_State: {
        if (mqtt_connected) {
          uint8_t log_type = f.data[0];
          if (log_type == 0x31) { // Buton basımı ile açma talebi
            if (!Remote_Open) {
              uint8_t btn_payload[5] = {0x01, 0, 0, 0, 0};
              publish_mqtt_frame(MSG_TYPE_EVENT, CMD_BUTTON_UNLOCK_REQ, btn_payload, 5, topic_event);
            }
          } else if (log_type == 0x05) { // RFID Kart okundu
            uint8_t nfc_payload[9] = {4, 0x00, 0x00, 0x00, 0x00, 0, 0, 0, 0};
            if (f.length >= 5) {
              memcpy(nfc_payload + 1, f.data + 1, 4);
            }
            publish_mqtt_frame(MSG_TYPE_EVENT, CMD_NFC_SCANNED, nfc_payload, 9, topic_event);
          } else if (log_type == 0x01) { // Parmak izi okundu
            uint8_t fp_payload[6] = {0, 0, 0, 0, 0, 0};
            if (f.length >= 3) {
              fp_payload[0] = f.data[1];
              fp_payload[1] = f.data[2];
            }
            publish_mqtt_frame(MSG_TYPE_EVENT, CMD_FINGERPRINT_SCANNED, fp_payload, 6, topic_event);
          }
        }
        break;
      }
      case Temporary_Pass: {
        String Pass_Value;
        for (size_t i = 0; i < f.length; i++) {
          Pass_Value += char('0' + f.data[i]);
        }
        DEBUG_PRINTF("Şifre girişi yapıldı: %s\n", Pass_Value.c_str());

        if (mqtt_connected) {
          uint8_t pin_payload[15];
          uint8_t pin_len = min((size_t)10, Pass_Value.length());
          pin_payload[0] = pin_len;
          memcpy(pin_payload + 1, Pass_Value.c_str(), pin_len);
          uint32_t reqId = mqtt_seq;
          pin_payload[1 + pin_len] = (reqId >> 24) & 0xFF;
          pin_payload[1 + pin_len + 1] = (reqId >> 16) & 0xFF;
          pin_payload[1 + pin_len + 2] = (reqId >> 8) & 0xFF;
          pin_payload[1 + pin_len + 3] = reqId & 0xFF;

          publish_mqtt_frame(MSG_TYPE_EVENT, CMD_PIN_ENTERED, pin_payload, 1 + pin_len + 4, topic_event);
        } else {
          uint8_t status_deny = 0x00;
          sendSTM32Packet(0x40, &status_deny, 1);
        }
        break;
      }
      default:
        break;
    }
    free(f.data);
  }
}

void resetWiFi() {
  clearWiFiCredentials();
  clearOnloiConfig();
  WiFi.disconnect(true);
  ESP.restart();
}

// STM32 A5 Haberleşmesi
void handleSTM32Commands() {
  // stm32_buffer and stm32_buffer_len are now global variables

  if (stm32_buffer_len == 0) return;

  size_t start_idx = 0;
  while (start_idx < stm32_buffer_len && stm32_buffer[start_idx] != 0xA5) {
    start_idx++;
  }

  if (start_idx > 0) {
    stm32_buffer_len -= start_idx;
    if (stm32_buffer_len > 0)
      memmove(stm32_buffer, stm32_buffer + start_idx, stm32_buffer_len);
  }

  if (stm32_buffer_len < 5) return;

  uint8_t cmd = stm32_buffer[1];
  uint8_t protocol_len = stm32_buffer[2];
  if (protocol_len == 0) {
    stm32_buffer_len--;
    if (stm32_buffer_len > 0)
      memmove(stm32_buffer, stm32_buffer + 1, stm32_buffer_len);
    return;
  }

  uint8_t payload_len = protocol_len - 1;
  if (stm32_buffer_len < (size_t)(5 + payload_len)) return;

  uint16_t checksum = cmd + protocol_len;
  for (int i = 0; i < payload_len; i++) {
    checksum += stm32_buffer[3 + i];
  }

  if ((checksum & 0xFF) == stm32_buffer[3 + payload_len] &&
      stm32_buffer[4 + payload_len] == 0x5A) {



    const uint8_t *payload = &stm32_buffer[3];
    uint8_t len = payload_len;

    switch (cmd) {
      case STM32_CMD_BATTERY_REPORT: {
        if (len >= 1) {
          uint8_t prcnt = payload[0];
          DEBUG_PRINTF("🔋 Pil Seviyesi: %d%%\n", prcnt);

          if (mqtt_connected) {
            DEBUG_PRINTLN("[STM32 -> MQTT] Pil seviyesi sunucuya gonderiliyor...");
            publish_mqtt_frame(MSG_TYPE_EVENT, CMD_BATTERY_REPORT, &prcnt, 1, topic_telemetry);
            uint8_t resp_data[2] = {STM32_CMD_BATTERY_REPORT, STM32_RESP_OK};
            sendSTM32Packet(STM32_CMD_RESPONSE, resp_data, 2);
          } else {
            DEBUG_PRINTLN("[STM32 -> MQTT] Pil seviyesi sunucuya GONDERILEMEDI (MQTT Baglantisi Yok)!");
            uint8_t resp_data[2] = {STM32_CMD_BATTERY_REPORT, STM32_RESP_NO_WIFI};
            sendSTM32Packet(STM32_CMD_RESPONSE, resp_data, 2);
          }
        }
        break;
      }
      case STM32_CMD_QUERY_BATTERY: {
        if (len >= 2) {
          uint8_t prcnt = payload[0];
          DEBUG_PRINTF("🔋 Pil Seviyesi Sorgusu: %d%%\n", prcnt);
          if (mqtt_connected) {
            DEBUG_PRINTLN("[STM32 -> MQTT] Pil sorgu seviyesi sunucuya gonderiliyor...");
            publish_mqtt_frame(MSG_TYPE_EVENT, CMD_BATTERY_REPORT, &prcnt, 1, topic_telemetry);
          } else {
            DEBUG_PRINTLN("[STM32 -> MQTT] Pil sorgu seviyesi sunucuya GONDERILEMEDI (MQTT Baglantisi Yok)!");
          }
        }
        break;
      }
      case 0x11: { // CMD_LOCK_OPENED
        DEBUG_PRINTLN("🚪 Kapi Acildi Bildirimi alindi.");
        door_opened_confirmed = true;
        if (mqtt_connected) {
          DEBUG_PRINTLN("[STM32 -> MQTT] Kilit acilma olayi sunucuya bildiriliyor...");
          uint8_t opened_payload[4];
          opened_payload[0] = Remote_Open ? 0x05 : 0x02; // remote vs card
          opened_payload[1] = 0x00;
          opened_payload[2] = 0x00;
          opened_payload[3] = Remote_Open ? 0x02 : 0x01; // trigger remote vs local
          publish_mqtt_frame(MSG_TYPE_EVENT, CMD_LOCK_OPENED, opened_payload, 4, topic_event);

          if (Remote_Open) {
            DEBUG_PRINTLN("[MQTT REMOTE] Uzaktan acma komutu basariyla tamamlandi. ACK gonderiliyor.");
            send_mqtt_ack(remote_open_request_id, 0x03, CMD_REMOTE_OPEN, 0x00); // REMOTE COMPLETED
            Remote_Open = false;
          }
        } else {
          DEBUG_PRINTLN("[STM32 -> MQTT] Kilit acilma olayi sunucuya GONDERILEMEDI (MQTT Baglantisi Yok)!");
        }
        break;
      }
      case STM32_CMD_QUERY_SETTINGS: {
        if (len >= 2) {
          DEBUG_PRINTF("Ayarlar: Ses=%d, Dil=%d\n", payload[0], payload[1]);
        }
        break;
      }
      case STM32_CMD_QUERY_TIME: {
        DEBUG_PRINTLN("STM32 zaman senkronizasyonu talep etti.");
        if (mqtt_connected) {
          DEBUG_PRINTLN("[STM32 -> MQTT] Sunucudan zaman senkronizasyon verisi talep ediliyor...");
          publish_mqtt_frame(MSG_TYPE_REQUEST, CMD_TIME_SYNC, NULL, 0, topic_cmd);
        } else {
          DEBUG_PRINTLN("[STM32 -> MQTT] Zaman senkronizasyonu sunucuya GONDERILEMEDI (MQTT Baglantisi Yok)!");
          uint8_t resp_data[2] = {STM32_CMD_QUERY_TIME, STM32_RESP_NO_WIFI};
          sendSTM32Packet(STM32_CMD_RESPONSE, resp_data, 2);
        }
        break;
      }
      case STM32_CMD_EVENT_LOG: {
        if (len >= 7) {
          uint8_t total_count = payload[6];
          int log_count = (len - 7) / 9;
          DEBUG_PRINTF("\nCevrimdisi log sayisi: %d\n", log_count);

          if (log_count > 0) {
            if (mqtt_connected) {
              DEBUG_PRINTF("[STM32 -> MQTT] %d adet cevrimdisi log sunucuya aktariliyor...\n", log_count);
              uint8_t log_payload[256];
              log_payload[0] = (uint8_t)log_count;
              size_t offset = 1;

              for (int i = 0; i < log_count; i++) {
                const uint8_t *log_entry = &payload[7 + (i * 9)];
                uint8_t event_type = log_entry[1];
                uint32_t user_id = (log_entry[2] << 16) | (log_entry[3] << 8) | log_entry[4];
                uint32_t unix_time = (log_entry[5] << 24) | (log_entry[6] << 16) | (log_entry[7] << 8) | log_entry[8];

                uint64_t ts_ms = (uint64_t)unix_time * 1000ULL;
                for (int j = 0; j < 8; j++) {
                  log_payload[offset++] = (ts_ms >> (56 - j * 8)) & 0xFF;
                }

                log_payload[offset++] = 0x02; // accessMethod = NFC
                uint16_t localId = user_id & 0xFFFF;
                log_payload[offset++] = (localId >> 8) & 0xFF;
                log_payload[offset++] = localId & 0xFF;
                log_payload[offset++] = (event_type == 0x01) ? 0x00 : 0x01; // result
              }

              publish_mqtt_frame(MSG_TYPE_EVENT, CMD_OFFLINE_LOG_BATCH, log_payload, offset, topic_log);
            } else {
              DEBUG_PRINTLN("[STM32 -> MQTT] Cevrimdisi loglar sunucuya GONDERILEMEDI (MQTT Baglantisi Yok)!");
              uint8_t err_data[2] = {STM32_CMD_EVENT_LOG, STM32_RESP_NO_WIFI};
              sendSTM32Packet(STM32_CMD_RESPONSE, err_data, 2);
            }
          }
        }
        break;
      }
      case STM32_CMD_FACTORY_RESET: {
        DEBUG_PRINTLN("STM32'den Fabrika Ayarlarına Dönüş Sinyali Alındı");
        if (mqtt_connected) {
          // Sıfırlama olayını sunucuya gönder
          publish_mqtt_frame(MSG_TYPE_EVENT, CMD_FACTORY_RESET, NULL, 0, topic_event);
        }
        delay(1000);
        resetWiFi();
        break;
      }
      case STM32_CMD_WIFI_RESET: {
        DEBUG_PRINTLN("STM32'den Wi-Fi Sıfırlama Sinyali Alındı");
        clearWiFiCredentials();
        delay(500);
        ESP.restart();
        break;
      }
      case 0x12: { // CMD_PIN_ENTERED (Şifre Girildi)
        DEBUG_PRINTLN("STM32'den Şifre Girildi (0x12) komutu alindi.");
        if (mqtt_connected) {
          uint8_t pin_payload[32];
          uint8_t pin_len = (len < 10) ? len : 10;
          pin_payload[0] = pin_len;
          memcpy(pin_payload + 1, payload, pin_len);
          uint32_t reqId = mqtt_seq;
          pin_payload[1 + pin_len] = (reqId >> 24) & 0xFF;
          pin_payload[1 + pin_len + 1] = (reqId >> 16) & 0xFF;
          pin_payload[1 + pin_len + 2] = (reqId >> 8) & 0xFF;
          pin_payload[1 + pin_len + 3] = reqId & 0xFF;

          pending_mcu_cmd = cmd;
          pending_mcu_seq = mqtt_seq;
          pending_mcu_time = millis();

          publish_mqtt_frame(MSG_TYPE_EVENT, CMD_PIN_ENTERED, pin_payload, 1 + pin_len + 4, topic_event);
        } else {
          DEBUG_PRINTLN("MQTT baglantisi yok, red yaniti gonderiliyor.");
          uint8_t err_code = 0x02; // STATUS 0x02 = timeout/hata
          sendSTM32Packet(cmd, &err_code, 1);
        }
        break;
      }
      case 0x13: { // CMD_NFC_SCANNED (NFC Kart Okundu)
        DEBUG_PRINTLN("STM32'den NFC Okundu (0x13) komutu alindi.");
        if (mqtt_connected) {
          uint8_t nfc_payload[32];
          
          // Convert ASCII hex representation back to raw bytes
          uint8_t raw_len = 0;
          for (int i = 0; i < len - 1 && raw_len < 16; i += 2) {
            char high_char = (char)payload[i];
            char low_char = (char)payload[i+1];
            
            uint8_t high = (high_char >= '0' && high_char <= '9') ? (high_char - '0') :
                           (high_char >= 'a' && high_char <= 'f') ? (high_char - 'a' + 10) :
                           (high_char >= 'A' && high_char <= 'F') ? (high_char - 'A' + 10) : 0;
            uint8_t low = (low_char >= '0' && low_char <= '9') ? (low_char - '0') :
                          (low_char >= 'a' && low_char <= 'f') ? (low_char - 'a' + 10) :
                          (low_char >= 'A' && low_char <= 'F') ? (low_char - 'A' + 10) : 0;
                          
            nfc_payload[1 + raw_len] = (high << 4) | low;
            raw_len++;
          }
          
          nfc_payload[0] = raw_len;

          pending_mcu_cmd = cmd;
          pending_mcu_seq = mqtt_seq;
          pending_mcu_time = millis();

          publish_mqtt_frame(MSG_TYPE_EVENT, CMD_NFC_SCANNED, nfc_payload, 1 + raw_len, topic_event);
        } else {
          DEBUG_PRINTLN("MQTT baglantisi yok, red yaniti gonderiliyor.");
          uint8_t err_code = 0x02; // STATUS 0x02 = timeout/hata
          sendSTM32Packet(cmd, &err_code, 1);
        }
        break;
      }
      case 0x14: { // CMD_QR_SCANNED (QR Kod Okundu)
        DEBUG_PRINTLN("STM32'den QR Okundu (0x14) komutu alindi.");
        if (mqtt_connected) {
          uint8_t qr_payload[256];
          uint16_t qr_len = (len < 200) ? len : 200;
          qr_payload[0] = (qr_len >> 8) & 0xFF;
          qr_payload[1] = qr_len & 0xFF;
          memcpy(qr_payload + 2, payload, qr_len);

          pending_mcu_cmd = cmd;
          pending_mcu_seq = mqtt_seq;
          pending_mcu_time = millis();

          publish_mqtt_frame(MSG_TYPE_EVENT, CMD_QR_SCANNED, qr_payload, 2 + qr_len, topic_event);
        } else {
          DEBUG_PRINTLN("MQTT baglantisi yok, red yaniti gonderiliyor.");
          uint8_t err_code = 0x02; // STATUS 0x02 = timeout/hata
          sendSTM32Packet(cmd, &err_code, 1);
        }
        break;
      }
      case 0x15: { // CMD_FINGERPRINT_SCANNED (Parmak Izi Okundu)
        DEBUG_PRINTLN("STM32'den Parmak Izi Okundu (0x15) komutu alindi.");
        if (mqtt_connected) {
          uint8_t fp_payload[2] = {0, 0};
          if (len >= 2) {
            fp_payload[0] = payload[0];
            fp_payload[1] = payload[1];
          } else if (len == 1) {
            fp_payload[1] = payload[0];
          }

          pending_mcu_cmd = cmd;
          pending_mcu_seq = mqtt_seq;
          pending_mcu_time = millis();

          publish_mqtt_frame(MSG_TYPE_EVENT, CMD_FINGERPRINT_SCANNED, fp_payload, 2, topic_event);
        } else {
          DEBUG_PRINTLN("MQTT baglantisi yok, red yaniti gonderiliyor.");
          uint8_t err_code = 0x02; // STATUS 0x02 = timeout/hata
          sendSTM32Packet(cmd, &err_code, 1);
        }
        break;
      }
      case 0x16: { // CMD_BUTTON_UNLOCK_REQ (Butonla Kilit Açma İsteği)
        DEBUG_PRINTLN("STM32'den Butonla Kilit Açma (0x16) komutu alindi.");
        if (mqtt_connected) {
          uint8_t btn_payload[5] = {0x01, 0, 0, 0, 0};
          uint32_t reqId = mqtt_seq;
          btn_payload[1] = (reqId >> 24) & 0xFF;
          btn_payload[2] = (reqId >> 16) & 0xFF;
          btn_payload[3] = (reqId >> 8) & 0xFF;
          btn_payload[4] = reqId & 0xFF;

          pending_mcu_cmd = cmd;
          pending_mcu_seq = mqtt_seq;
          pending_mcu_time = millis();

          publish_mqtt_frame(MSG_TYPE_EVENT, CMD_BUTTON_UNLOCK_REQ, btn_payload, 5, topic_event);
        } else {
          DEBUG_PRINTLN("MQTT baglantisi yok, red yaniti gonderiliyor.");
          uint8_t err_code = 0x02; // STATUS 0x02 = timeout/hata
          sendSTM32Packet(cmd, &err_code, 1);
        }
        break;
      }
      case CMD_FP_ENROLLED: { // 0x26 Parmak Izi Kayit Tamamlandi
        DEBUG_PRINTLN("STM32'den Parmak Izi Kayit Tamamlandi (0x26) komutu alindi.");
        if (mqtt_connected) {
          uint8_t fp_payload[6];
          fp_payload[0] = (active_fp_enroll_request_id >> 24) & 0xFF;
          fp_payload[1] = (active_fp_enroll_request_id >> 16) & 0xFF;
          fp_payload[2] = (active_fp_enroll_request_id >> 8) & 0xFF;
          fp_payload[3] = active_fp_enroll_request_id & 0xFF;

          uint16_t localId = 0;
          if (len >= 2) {
            localId = (payload[0] << 8) | payload[1];
          } else if (len == 1) {
            localId = payload[0];
          }
          fp_payload[4] = (localId >> 8) & 0xFF;
          fp_payload[5] = localId & 0xFF;

          publish_mqtt_frame(MSG_TYPE_EVENT, CMD_FP_ENROLLED, fp_payload, 6, topic_event);
        } else {
          DEBUG_PRINTLN("MQTT baglantisi yok, log sunucuya gonderilemedi.");
        }
        break;
      }
      case CMD_CARD_ENROLLED: { // 0x27 NFC Kart Kayit Tamamlandi
        DEBUG_PRINTLN("STM32'den NFC Kart Kayit Tamamlandi (0x27) komutu alindi.");
        if (mqtt_connected) {
          uint8_t card_payload[6];
          card_payload[0] = (active_nfc_enroll_request_id >> 24) & 0xFF;
          card_payload[1] = (active_nfc_enroll_request_id >> 16) & 0xFF;
          card_payload[2] = (active_nfc_enroll_request_id >> 8) & 0xFF;
          card_payload[3] = active_nfc_enroll_request_id & 0xFF;

          uint16_t localId = 0;
          if (len >= 2) {
            localId = (payload[0] << 8) | payload[1];
          } else if (len == 1) {
            localId = payload[0];
          }
          card_payload[4] = (localId >> 8) & 0xFF;
          card_payload[5] = localId & 0xFF;

          publish_mqtt_frame(MSG_TYPE_EVENT, CMD_CARD_ENROLLED, card_payload, 6, topic_event);
        } else {
          DEBUG_PRINTLN("MQTT baglantisi yok, log sunucuya gonderilemedi.");
        }
        break;
      }
      default:
        break;
    }

    size_t packet_size = 5 + payload_len;
    stm32_buffer_len -= packet_size;
    if (stm32_buffer_len > 0)
      memmove(stm32_buffer, stm32_buffer + packet_size, stm32_buffer_len);
  } else {
    stm32_buffer_len--;
    if (stm32_buffer_len > 0)
      memmove(stm32_buffer, stm32_buffer + 1, stm32_buffer_len);
  }
}

void sendSTM32Packet(uint8_t cmd, const uint8_t *payload, uint8_t payload_len) {
  uint8_t protocol_len = payload_len + 1;
  uint8_t header = 0xA5;
  uint8_t footer = 0x5A;
  uint16_t checksum = cmd + protocol_len;

  SerialUART.write(header);
  SerialUART.write(cmd);
  SerialUART.write(protocol_len);

  DEBUG_PRINT(">>> [MCU TX RAW] ");
  DEBUG_PRINTF("%02X %02X %02X ", header, cmd, protocol_len);

  for (int i = 0; i < payload_len; i++) {
    SerialUART.write(payload[i]);
    checksum += payload[i];
    DEBUG_PRINTF("%02X ", payload[i]);
  }

  uint8_t cs_byte = checksum & 0xFF;
  SerialUART.write(cs_byte);
  SerialUART.write(footer);
  DEBUG_PRINTF("%02X %02X\n", cs_byte, footer);

  // Gönderilen yanıt detayını anlaşılır şekilde yazdır
  if (cmd == STM32_CMD_RESPONSE && payload_len >= 2) {
    uint8_t orig_cmd = payload[0];
    uint8_t resp_status = payload[1];
    String status_str;
    switch (resp_status) {
      case STM32_RESP_OK: status_str = "OK (Basarili)"; break;
      case STM32_RESP_ERROR: status_str = "HATA (Genel Hata)"; break;
      case STM32_RESP_NO_WIFI: status_str = "HATA (Internet/MQTT Baglantisi Yok)"; break;
      case STM32_RESP_SERVER_ERR: status_str = "HATA (Sunucu Hatasi)"; break;
      case STM32_RESP_SERVER_REJ: status_str = "HATA (Sunucu Reddedildi)"; break;
      default: status_str = "Bilinmeyen Kod: " + String(resp_status); break;
    }
    DEBUG_PRINTF("[MCU TX DETAY] STM32'nin 0x%02X komutuna yanıt verildi. Durum: %s\n", orig_cmd, status_str.c_str());
  }
}

void sendSTM32SoundSetting(uint8_t enable) {
  uint8_t value = enable ? 0x01 : 0x00;
  sendSTM32Packet(STM32_CMD_SET_SOUND, &value, 1);
}

void sendSTM32LanguageSetting(uint8_t english) {
  uint8_t value = english ? 0x01 : 0x00;
  sendSTM32Packet(STM32_CMD_SET_LANGUAGE, &value, 1);
}

void sendSTM32FindDevice() {
  sendSTM32Packet(STM32_CMD_FIND_DEVICE, nullptr, 0);
}

void sendSTM32QuerySettings() {
  sendSTM32Packet(STM32_CMD_QUERY_SETTINGS, nullptr, 0);
}

void sendSTM32QueryBattery() {
  sendSTM32Packet(STM32_CMD_QUERY_BATTERY, nullptr, 0);
}
