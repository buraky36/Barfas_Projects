#include "QRHandler.h"
#include "wrappers.h"
#include "RTCManager.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// BearSSL / libb64
// Assuming these headers are available in the include path of the Arduino core
#include <bearssl/bearssl.h>
#include <libb64/cdecode.h>

// Helper prototypes
static bool decryptAESGCM(const uint8_t *cipherObj, size_t totalLen, char *outputBuffer, size_t bufferSize);
static bool validateReservation(const char *jsonPayload, const DeviceConfig *config);
static bool checkDate(const char *resDateStr, DateTime now);
static bool checkTime(const char *startTime, const char *endTime, DateTime now);
static int parseTimeStr(const char *timeStr);

bool qr_process(const char *encryptedBase64, const DeviceConfig *config) {
    size_t len = strlen(encryptedBase64);
    sys_log("[QR] Processing Data (Len: %d)\n", len);

    // 1. Base64 Decode
    // Output length estimation: (len * 3) / 4
    size_t binaryLen = (len * 3) / 4;
    // Allocate buffer
    uint8_t *binaryBuffer = (uint8_t*)malloc(binaryLen + 16); // +margin
    if (!binaryBuffer) {
        sys_log_ln("[QR] Memory Error");
        return false;
    }

    base64_decodestate s;
    base64_init_decodestate(&s);
    // libb64 expects non-const input strictly in some versions, but usually const char* is fine for input
    // Casting to void* or similar might be needed if prototype differs, but standard is const char*
    int decodedLen = base64_decode_block(encryptedBase64, len, (char *)binaryBuffer, &s);

    if (decodedLen <= (GCM_IV_LEN + GCM_TAG_LEN)) {
        sys_log("[QR] ERROR: Data too short (Len: %d)\n", decodedLen);
        free(binaryBuffer);
        return false;
    }
    sys_log("[QR] Base64 Decoded. Binary Len: %d\n", decodedLen);

    // 2. Decrypt
    char *jsonBuffer = (char*)malloc(decodedLen + 1);
    if (!jsonBuffer) {
        free(binaryBuffer);
        return false;
    }
    
    sys_log_ln("[QR] Attempting AES-GCM Decryption...");
    bool decryptSuccess = decryptAESGCM(binaryBuffer, decodedLen, jsonBuffer, decodedLen);

    free(binaryBuffer);

    if (!decryptSuccess) {
        sys_log_ln("[QR] ERROR: Decryption Failed (Key/Tag Mismatch)");
        free(jsonBuffer);
        return false;
    }

    sys_log_ln("[QR] Decryption SUCCESS!");
    sys_log_ln("[QR] Payload: ");
    sys_log_ln(jsonBuffer);

    // 3. Validate
    sys_log_ln("[QR] Validating Payload...");
    bool valid = validateReservation(jsonBuffer, config);
    free(jsonBuffer);

    if (valid)
        sys_log_ln("[QR] Validation PASSED");
    else
        sys_log_ln("[QR] Validation FAILED");

    return valid;
}

static bool decryptAESGCM(const uint8_t *cipherObj, size_t totalLen, char *outputBuffer, size_t bufferSize) {
    if (totalLen < (GCM_IV_LEN + GCM_TAG_LEN)) return false;

    size_t cipherTextLen = totalLen - GCM_IV_LEN - GCM_TAG_LEN;
    const uint8_t *iv = cipherObj;
    const uint8_t *cipherText = cipherObj + GCM_IV_LEN;
    const uint8_t *tag = cipherObj + GCM_IV_LEN + cipherTextLen;

    br_aes_ct_ctr_keys ctx;
    br_aes_ct_ctr_init(&ctx, AES_KEY, 32);

    br_gcm_context gcm;
    br_gcm_init(&gcm, &ctx.vtable, br_ghash_ctmul32);
    br_gcm_reset(&gcm, iv, GCM_IV_LEN);

    memcpy(outputBuffer, cipherText, cipherTextLen);
    br_gcm_flip(&gcm);
    br_gcm_run(&gcm, 0, outputBuffer, cipherTextLen);
    outputBuffer[cipherTextLen] = '\0';

    uint8_t calculatedTag[16];
    br_gcm_get_tag(&gcm, calculatedTag);

    if (memcmp(tag, calculatedTag, GCM_TAG_LEN) != 0) {
        sys_log_ln("[QR] [GCM] Tag Mismatch!");
        return false;
    }

    return true;
}

static bool validateReservation(const char *jsonPayload, const DeviceConfig *config) {
    QRPayload payload;
    if (!sys_json_parse_qr(jsonPayload, &payload)) {
        sys_log_ln("[QR] ERROR: JSON Parse Failed");
        return false;
    }

    // 1. Check Device ID
    if (strlen(payload.checkinId) == 0) {
        sys_log_ln("[QR] ERROR: Missing checkinId");
        return false;
    }

    if (strcmp(payload.checkinId, config->deviceId) != 0) {
        sys_log("[QR] Device ID Mismatch (Qr: %s, Dev: %s)\n", payload.checkinId, config->deviceId);
        return false;
    }
    sys_log_ln("[QR] Device ID Verified.");

    // 2. Check MASTER
    if (strcmp(payload.type, "MASTER") == 0) {
        sys_log_ln("[QR] MASTER QR Detected.");
        return true;
    }

    // 3. Normal Validation
    if (strlen(payload.reservationDate) == 0 || strlen(payload.startTime) == 0 || strlen(payload.endTime) == 0) {
        sys_log_ln("[QR] ERROR: Missing valid fields");
        return false;
    }

    DateTime now = rtc_get_datetime();
    sys_log("[QR] Device Time: %04d-%02d-%02d %02d:%02d\n", now.year, now.month, now.day, now.hour, now.minute);
    sys_log("[QR] Reserv Time: %s %s-%s\n", payload.reservationDate, payload.startTime, payload.endTime);

    if (!checkDate(payload.reservationDate, now)) {
        sys_log_ln("[QR] Date Mismatch");
        return false;
    }

    if (!checkTime(payload.startTime, payload.endTime, now)) {
        sys_log_ln("[QR] Time Mismatch");
        return false;
    }

    return true;
}

static bool checkDate(const char *resDateStr, DateTime now) {
    int year, month, day;
    if (sscanf(resDateStr, "%d-%d-%d", &year, &month, &day) != 3) {
        return false;
    }
    return (year == now.year && month == now.month && day == now.day);
}

static bool checkTime(const char *startTime, const char *endTime, DateTime now) {
    int nowMins = now.hour * 60 + now.minute;
    int startMins = parseTimeStr(startTime);
    int endMins = parseTimeStr(endTime);

    if (startMins == -1 || endMins == -1) return false;

    return (nowMins >= startMins && nowMins <= endMins);
}

static int parseTimeStr(const char *timeStr) {
    int h, m;
    if (sscanf(timeStr, "%d:%d", &h, &m) != 2) return -1;
    return h * 60 + m;
}
