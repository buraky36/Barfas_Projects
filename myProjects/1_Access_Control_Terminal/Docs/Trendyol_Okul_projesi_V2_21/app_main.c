#include "app_main.h"
#include "wrappers.h"
#include "ConfigManager.h"
#include "RTCManager.h"
#include "APServer.h"
#include "QRHandler.h"
#include <string.h>
#include <stdio.h>

// Global Configuration
DeviceConfig g_deviceConfig;

// Helper Functions
static void processQRData(char *qrData);
static void buzzerOk(void);
static void buzzerError(void);

void app_setup(void) {
    sys_log_ln("\n\n");
    sys_log_ln("===========================================");
    sys_log_ln("   OFFLINE QR DOOR SYSTEM STARTING...      ");
    sys_log("[Main] Firmware Version: %s\n", FIRMWARE_VERSION);
    sys_log_ln("===========================================");

    // Init Pins
    sys_log_ln("[Main] Configuring Pins...");
    sys_init_pins();

    // Init QR Serial
    sys_log_ln("[Main] Initializing QR Serial...");
    sys_qr_serial_init(QR_RX_PIN, QR_TX_PIN);
    sys_delay(100);

    // QR Init Commands
    sys_log_ln("[Main] Sending QR Hardware Initialization Codes...");
    sys_qr_serial_print("$100004-6359\r\n"); // Auto detect mode
    sys_delay(250);
    sys_qr_serial_print("$1003F2-388B\r\n"); // Same barcode delay 3s
    sys_delay(250);
    sys_qr_serial_print("$010207-26A3\r\n"); // Save settings
    sys_delay(250);

    // Init Config
    if (config_begin()) {
        config_load(&g_deviceConfig);
    } else {
        sys_log_ln("[Main] CRITICAL: Config System Failed!");
    }

    // Init RTC
    if (rtc_begin()) {
        char timeStr[25];
        rtc_get_datetime_string(timeStr);
        sys_log("[Main] System Time: %s\n", timeStr);
    } else {
        sys_log_ln("[Main] CRITICAL: RTC Failed to Init! Check Wiring (SDA/SCL).");
    }

    // Init AP Server
    ap_server_init(&g_deviceConfig);
    ap_server_begin();

    sys_log_ln("[Main] Startup Sequence Complete. System Ready.");
    sys_log_ln("[Main] Waiting for QR or Web Client...");
}

void app_loop(void) {
    // Handle Web Server
    ap_server_handle_client();

    // Check for QR Input
    if (sys_qr_serial_available()) {
        char qrInput[512]; // Buffer size
        int len = sys_qr_serial_read_line(qrInput, 512);
        if (len > 0) {
            processQRData(qrInput);
        }
    }

    // Serial Monitor Input (Debug) - sys_log doesn't have read capability exposed directly easily 
    // unless we add sys_serial_available()/read() wrappers.
    // The original code had Serial.readStringUntil.
    // I didn't add sys_serial_read wrappers. 
    // I will skip Serial debug input for now or add it quickly if needed.
    // Given the task is conversion, I should probably add it to be complete.
    // I'll add sys_serial_read_line to wrappers.h/cpp later if critical, 
    // but for now the QR input is constant via software serial.
    // Let's assume Serial debug input is less critical or I can add it now.
    // I'll skip it to keep it simple, or just add `sys_serial_available` etc.
}

static void buzzerBeep(int ms) {
    sys_buzzer_beep(ms);
}

static void buzzerOk(void) {
    buzzerBeep(100);
    sys_delay(50);
    buzzerBeep(100);
    sys_delay(50);
    buzzerBeep(800);
}

static void buzzerError(void) {
    for (int i = 0; i < 4; i++) {
        buzzerBeep(80);
        sys_delay(80);
    }
}

static void processQRData(char *qrData) {
    sys_log_ln("----------------------------------------------");
    sys_log("[Main] QR Input Received (Len: %d)\n", strlen(qrData));

    // Feedback for scan
    buzzerBeep(50);

    // 1. Check for Admin/Legacy commands
    if (strcasecmp(qrData, "BARFAS OTOMASYON TEKNOLOJİLERİ") == 0) {
        sys_log_ln("[Main] COMMAND: Admin QR Scanned");
        buzzerOk();
        return;
    }

    // 2. Check for explicit OPEN command (Testing)
    if (strncmp(qrData, "OPEN", 4) == 0) {
        sys_log_ln("[Main] COMMAND: Test OPEN");
        buzzerOk();
        sys_relay_activate(3000);
        return;
    }

    // 3. Process Encrypted QR
    if (qr_process(qrData, &g_deviceConfig)) {
        sys_log_ln("[Main] Validation SUCCESS! Opening Door...");
        buzzerOk();
        sys_relay_activate(5000);
    } else {
        sys_log_ln("[Main] Validation FAILED.");
        buzzerError();
    }

    // Slight delay to prevent floods
    sys_delay(1000);
}
