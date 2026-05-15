#ifndef QRHANDLER_H
#define QRHANDLER_H

#include "Config.h"
#include "RTCManager.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <bearssl/bearssl.h>
#include <libb64/cdecode.h>

// AES GCM Constants
#define GCM_IV_LEN 12
#define GCM_TAG_LEN 16

class QRHandler {
public:
  QRHandler(RTCManager &rtcMgr, DeviceConfig &deviceConfig);
  bool processQR(String encryptedBase64);

private:
  RTCManager &_rtcMgr;
  DeviceConfig &_deviceConfig;

  // Decoding & Decryption
  bool decryptAESGCM(const uint8_t *cipherObj, size_t totalLen,
                     char *outputBuffer, size_t bufferSize);

  // Validation
  bool validateReservation(const char *jsonPayload);
  bool checkDate(const char *resDateStr, DateTime now);
  bool checkTime(const char *startTime, const char *endTime, DateTime now);

  // Helpers
  int parseTimeStr(const char *timeStr); // Returns minutes from 00:00
};

#endif
