#include "QRHandler.h"

QRHandler::QRHandler(RTCManager &rtcMgr, DeviceConfig &deviceConfig)
    : _rtcMgr(rtcMgr), _deviceConfig(deviceConfig) {}

bool QRHandler::processQR(String encryptedBase64) {
  Serial.printf("[QR] Processing Data (Len: %d)\n", encryptedBase64.length());

  // 1. Base64 Decode
  size_t len = encryptedBase64.length();
  size_t binaryLen = (len * 3) / 4;
  uint8_t *binaryBuffer = new uint8_t[binaryLen + 1];

  base64_decodestate s;
  base64_init_decodestate(&s);
  int decodedLen = base64_decode_block(encryptedBase64.c_str(), len,
                                       (char *)binaryBuffer, &s);

  if (decodedLen <= (GCM_IV_LEN + GCM_TAG_LEN)) {
    Serial.printf(
        "[QR] ERROR: Data too short after decode (Len: %d, Min: %d)\n",
        decodedLen, GCM_IV_LEN + GCM_TAG_LEN);
    delete[] binaryBuffer;
    return false;
  }
  Serial.printf("[QR] Base64 Decoded. Binary Len: %d\n", decodedLen);

  // 2. Decrypt
  char *jsonBuffer = new char[decodedLen + 1];
  Serial.println("[QR] Attempting AES-GCM Decryption...");
  bool decryptSuccess =
      decryptAESGCM(binaryBuffer, decodedLen, jsonBuffer, decodedLen);

  delete[] binaryBuffer;

  if (!decryptSuccess) {
    Serial.println("[QR] ERROR: Decryption Failed (Key/Tag Mismatch)");
    delete[] jsonBuffer;
    return false;
  }

  Serial.println("[QR] Decryption SUCCESS!");
  Serial.print("[QR] Payload: ");
  Serial.println(jsonBuffer);

  // 3. Validate
  Serial.println("[QR] Validating Payload...");
  bool valid = validateReservation(jsonBuffer);
  delete[] jsonBuffer;

  if (valid)
    Serial.println("[QR] Validation PASSED");
  else
    Serial.println("[QR] Validation FAILED");

  return valid;
}

bool QRHandler::decryptAESGCM(const uint8_t *cipherObj, size_t totalLen,
                              char *outputBuffer, size_t bufferSize) {
  if (totalLen < (GCM_IV_LEN + GCM_TAG_LEN))
    return false;

  size_t cipherTextLen = totalLen - GCM_IV_LEN - GCM_TAG_LEN;
  const uint8_t *iv = cipherObj;
  const uint8_t *cipherText = cipherObj + GCM_IV_LEN;
  const uint8_t *tag = cipherObj + GCM_IV_LEN + cipherTextLen;

  br_aes_ct_ctr_keys ctx;
  br_aes_ct_ctr_init(&ctx, AES_KEY, 32);

  // DEBUG: Print Key used
  Serial.print("[QR] Key (Hex): ");
  for (int i = 0; i < 32; i++)
    Serial.printf("%02X", AES_KEY[i]);
  Serial.println();

  br_gcm_context gcm;
  br_gcm_init(&gcm, &ctx.vtable, br_ghash_ctmul32);

  br_gcm_reset(&gcm, iv, GCM_IV_LEN);

  // DEBUG: Print IV
  Serial.print("[QR] IV: ");
  for (int i = 0; i < GCM_IV_LEN; i++)
    Serial.printf("%02X", iv[i]);
  Serial.println();

  memcpy(outputBuffer, cipherText, cipherTextLen);
  br_gcm_flip(&gcm);
  br_gcm_run(&gcm, 0, outputBuffer, cipherTextLen);
  outputBuffer[cipherTextLen] = '\0'; // Null terminate for debug printing

  uint8_t calculatedTag[16];
  br_gcm_get_tag(&gcm, calculatedTag);

  if (memcmp(tag, calculatedTag, GCM_TAG_LEN) != 0) {
    Serial.println("[QR] [GCM] Tag Mismatch!");

    // DEBUG: Print what we decrypted anyway
    Serial.println("[QR] [DEBUG-FAIL] Decrypted Content (Candidate):");
    Serial.println(outputBuffer);

    Serial.print("[QR] [GCM] Expected Tag: ");
    for (int i = 0; i < 16; i++)
      Serial.printf("%02X", tag[i]);
    Serial.println();

    Serial.print("[QR] [GCM] Calc/d Tag:   ");
    for (int i = 0; i < 16; i++)
      Serial.printf("%02X", calculatedTag[i]);
    Serial.println();

    return false;
  }

  return true;
}

bool QRHandler::validateReservation(const char *jsonPayload) {
  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, jsonPayload);

  if (error) {
    Serial.printf("[QR] ERROR: JSON Parse Failed: %s\n", error.c_str());
    return false;
  }

  // 1. Check Device ID (Mandatory for ALL QRs)
  const char *checkinId = doc["checkinId"];
  if (!checkinId) {
    Serial.println("[QR] ERROR: Missing checkinId in JSON");
    return false;
  }

  if (strcmp(checkinId, _deviceConfig.deviceId) != 0) {
    Serial.printf(
        "[QR] Validation Failed: Device ID Mismatch (Qr: %s, Dev: %s)\n",
        checkinId, _deviceConfig.deviceId);
    return false;
  }
  Serial.println("[QR] Device ID Verified.");

  // 2. Check for MASTER type
  const char *type = doc["type"];
  if (type && strcmp(type, "MASTER") == 0) {
    Serial.println("[QR] MASTER QR Detected. Bypassing Date/Time checks.");
    return true;
  }

  // 3. Normal Reservation Validation
  const char *resDate = doc["reservationDate"];
  const char *start = doc["startTime"];
  const char *end = doc["endTime"];

  if (!resDate || !start || !end) {
    Serial.println(
        "[QR] ERROR: Missing required fields in JSON for Standard QR");
    return false;
  }

  DateTime now = _rtcMgr.getDateTime();
  Serial.printf("[QR] Device Time: %04d-%02d-%02d %02d:%02d\n", now.year,
                now.month, now.day, now.hour, now.minute);
  Serial.printf("[QR] Reserv Time: %s %s-%s\n", resDate, start, end);

  if (!checkDate(resDate, now)) {
    Serial.println("[QR] Validation Failed: Date Mismatch");
    return false;
  }

  if (!checkTime(start, end, now)) {
    Serial.println("[QR] Validation Failed: Time Mismatch");
    return false;
  }

  const char *status = doc["status"];
  if (status && strcmp(status, "CREATED") != 0) {
    Serial.printf(
        "[QR] Validation Warning: Status is '%s' (Expected CREATED)\n", status);
  }

  return true;
}

bool QRHandler::checkDate(const char *resDateStr, DateTime now) {
  int year, month, day;
  if (sscanf(resDateStr, "%d-%d-%d", &year, &month, &day) != 3) {
    Serial.println("[QR] ERROR: Invalid Date Format");
    return false;
  }

  // Simple verification
  if (year == now.year && month == now.month && day == now.day) {
    return true;
  }
  return false;
}

bool QRHandler::checkTime(const char *startTime, const char *endTime,
                          DateTime now) {
  int nowMins = now.hour * 60 + now.minute;
  int startMins = parseTimeStr(startTime);
  int endMins = parseTimeStr(endTime);

  if (startMins == -1 || endMins == -1) {
    Serial.println("[QR] ERROR: Invalid Time Format");
    return false;
  }

  Serial.printf("[QR] Checking Validity: %d <= %d <= %d (Mins)\n", startMins,
                nowMins, endMins);

  return (nowMins >= startMins && nowMins <= endMins);
}

int QRHandler::parseTimeStr(const char *timeStr) {
  int h, m;
  if (sscanf(timeStr, "%d:%d", &h, &m) != 2)
    return -1;
  return h * 60 + m;
}
