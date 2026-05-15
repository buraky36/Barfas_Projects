#include "APServer.h"
#include "Config.h"
#include "ConfigManager.h"
#include "QRHandler.h"
#include "RTCManager.h"
#include <SoftwareSerial.h>

// =============================================================
// GLOBAL OBJECTS
// =============================================================

ConfigManager configMgr;
RTCManager rtcMgr;
DeviceConfig deviceConfig;
APServer apServer(configMgr, rtcMgr, deviceConfig);
SoftwareSerial QRSerial(QR_RX_PIN, QR_TX_PIN); // RX, TX
QRHandler qrHandler(rtcMgr, deviceConfig);

// =============================================================
// HELPER FUNCTIONS
// =============================================================

void buzzerBeep(int ms = 100) {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(ms);
  digitalWrite(BUZZER_PIN, LOW);
}

void buzzerOk() {
  buzzerBeep(100);
  delay(50);
  buzzerBeep(100);
  delay(50);
  buzzerBeep(800);
}

void buzzerError() {
  for (int i = 0; i < 4; i++) {
    buzzerBeep(80);
    delay(80);
  }
}

void relayActivate(int ms = 5000) {
  Serial.printf("[Relay] Activating for %d ms\n", ms);
  digitalWrite(RELAY_PIN, HIGH);
  delay(ms);
  digitalWrite(RELAY_PIN, LOW);
  Serial.println("[Relay] Deactivated");
}

void processQRData(String qrData) {
  Serial.println("----------------------------------------------");
  Serial.print("[Main] QR Input Received (Len: ");
  Serial.print(qrData.length());
  Serial.println(")");

  // Feedback for scan
  buzzerBeep(50);

  // 1. Check for Admin/Legacy commands
  if (qrData.equalsIgnoreCase("BARFAS OTOMASYON TEKNOLOJİLERİ")) {
    Serial.println("[Main] COMMAND: Admin QR Scanned");
    buzzerOk();
    return;
  }

  // 2. Check for explicit OPEN command (Testing)
  if (qrData.startsWith("OPEN")) {
    Serial.println("[Main] COMMAND: Test OPEN");
    buzzerOk();
    relayActivate(3000);
    return;
  }

  // 3. Process Encrypted QR
  if (qrHandler.processQR(qrData)) {
    Serial.println("[Main] Validation SUCCESS! Opening Door...");
    buzzerOk();
    relayActivate(5000);
  } else {
    Serial.println("[Main] Validation FAILED.");
    buzzerError();
  }

  // Slight delay to prevent floods
  delay(1000);
}

// =============================================================
// MAIN SETUP
// =============================================================
void setup() {
  Serial.begin(115200);
  Serial.println("\n\n");
  Serial.println("===========================================");
  Serial.println("   OFFLINE QR DOOR SYSTEM STARTING...      ");
  Serial.printf("   Firmware Version: %s\n", FIRMWARE_VERSION);
  Serial.println("===========================================");

  // Init Pins
  Serial.println("[Main] Configuring Pins...");
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW); // Beep to indicate IO init

  // Init QR Serial
  Serial.println("[Main] Initializing QR Serial...");
  QRSerial.begin(9600);
  QRSerial.setTimeout(100);
  delay(100);

// QR Init Commands
  Serial.println("[Main] Sending QR Hardware Initialization Codes...");

  // 1. OKUMA MODU: Otomatik Algılama (Sürekli okumayı durdurur, hareket görünce okur)
  // Bu mod, kağıt/telefon sabit durduğunda okumayı kesmek için en iyisidir.
  QRSerial.println("$100004-6359"); 
  delay(250);

  // 2. AYNI BARKOD GECİKMESİ: 3 Saniye ($1003F2-388B)
  // Cihaz bir QR'ı okuduktan sonra 3 saniye boyunca aynı veriyi görmezden gelir.
  // 3 saniye sonra hala oradaysa veya çekip geri getirirseniz tekrar okur.
  QRSerial.println("$1003F2-388B"); 
  delay(250);

  // 3. AYARLARI KAYDET
  QRSerial.println("$010207-26A3"); 
  delay(250);

  // Init Config
  if (configMgr.begin()) {
    configMgr.loadConfig(deviceConfig);
  } else {
    Serial.println("[Main] CRITICAL: Config System Failed!");
  }

  // Init RTC
  if (rtcMgr.begin()) {
    Serial.print("[Main] System Time: ");
    Serial.println(rtcMgr.getDateTimeString());
  } else {
    Serial.println(
        "[Main] CRITICAL: RTC Failed to Init! Check Wiring (SDA/SCL).");
  }

  // Init AP Server
  apServer.begin();

  Serial.println("[Main] Startup Sequence Complete. System Ready.");
  Serial.println("[Main] Waiting for QR or Web Client...");
}

// =============================================================
// MAIN LOOP
// =============================================================
void loop() {
  // Handle Web Server
  apServer.handleClient();

  // Check for QR Input
  if (QRSerial.available() > 0) {
    String qrInput = QRSerial.readStringUntil('\n');
    qrInput.trim();
    if (qrInput.length() > 0) {
      processQRData(qrInput);
    }
  }

  // Serial Monitor Input
  if (Serial.available() > 0) {
    String debugInput = Serial.readStringUntil('\n');
    debugInput.trim();
    if (debugInput.length() > 0) {
      processQRData(debugInput);
    }
  }
}
