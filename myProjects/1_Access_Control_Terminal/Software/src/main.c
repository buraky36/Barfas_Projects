#include "app_state_machine.h"
#include "ble_prov.h"
#include "esp_event.h"
#include "hal_io.h"
#include "hal_qr.h"
#include "hal_shift_reg.h"
#include "hal_touch.h"
#include "hal_wiegand.h"
#include "mfrc522.h"
#include "nv_storage.h"
#include "wifi_manager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hw_config.h"
#include "driver/gpio.h"
#include <string.h>

uint8_t active_hw_version = HW_VERSION_QR_ONLY; // Default

void app_main(void) {
  printf("Starting Access Control Terminal\n");

  // ESP-IDF core services
  esp_event_loop_create_default();

  // Hardware initialization
  hal_io_init();
  hal_shift_reg_init(); // MUST be before touch and RFID due to RST pins
  nv_storage_init();
  hal_touch_init();
  
  // MFRC522 RST is driven by Shift Register U2 QG (MASK_U2_RFID_RST).
  // After hal_shift_reg_init() all outputs are 0x00 → RST is LOW (active reset).
  // The MFRC522 ignores SPI while RST is LOW, so we must release it first
  // and wait at least 37µs (datasheet) before any SPI transaction.
  hal_shift_reg_set_rfid_rst(true);
  vTaskDelay(pdMS_TO_TICKS(50)); // 50ms: safe margin after hard reset release

  if (mfrc522_init()) {
      active_hw_version = HW_VERSION_RFID_ONLY;
      printf("Hardware Detected: PIN + RFID\n");
  } else {
      active_hw_version = HW_VERSION_QR_ONLY;
      printf("Hardware Detected: PIN + QR\n");
  }

  hal_wiegand_init();
  hal_qr_init();

// To enable hardware test mode, change 0 to 1
#define HARDWARE_TEST_MODE 0


#if HARDWARE_TEST_MODE
  printf("\n==================================\n");
  printf("   HARDWARE DEBUG TEST MODE ACTIVE  \n");
  printf("==================================\n");
  printf("Monitoring TSM12 (Touch), QR Module, Buzzer and RGB LED...\n");
  
  //printf("Starting BLE for phone visibility test...\n\n");

  //ble_prov_init();

  uint32_t tick_count = 0;
  led_color_t colors[] = {LED_COLOR_RED, LED_COLOR_GREEN, LED_COLOR_BLUE, LED_COLOR_YELLOW, LED_COLOR_ORANGE};

  while (1) {
    // 1. Test Touch Keypad
    char key = '\0';
    if (hal_touch_read_key(&key)) {
        hal_io_buzzer_beep(2700, 150, 1);
    }

    // 2. Test MFRC522 directly if attached
    if (active_hw_version != HW_VERSION_QR_ONLY) {
        uint32_t rfid_uid_test = 0;
        if (mfrc522_check_card(&rfid_uid_test)) {
            printf("[MFRC522] Card Scanned! UID (Decimal): %lu\n", (unsigned long)rfid_uid_test);
            hal_io_buzzer_beep(2700, 100, 1);
        }
    }

    // 3. Test QR Scanner
    char qr_buffer[64] = {0};
    if (hal_qr_read(qr_buffer, sizeof(qr_buffer))) {
        printf("[QR SENSOR] QR Code Scanned: %s\n", qr_buffer);
        hal_io_buzzer_beep(2700, 100, 1);

        // Toggle relay if QR content contains "ROLE" (case insensitive approach with strstr)
        if (strstr(qr_buffer, "ROLE") != NULL || strstr(qr_buffer, "role") != NULL || strstr(qr_buffer, "Role") != NULL) {
            static bool relay_state = false;
            relay_state = !relay_state;
            hal_io_relay_set(relay_state);
            printf("[TEST] Relay toggled to: %s\n", relay_state ? "ON" : "OFF");
        }
    }

    // 4. Test RGB LED (change color every ~1 second)
    if (tick_count % 20 == 0) { // 20 * 50ms = 1000ms
        uint8_t color_idx = (tick_count / 20) % 5;
        hal_io_led_set(colors[color_idx], LED_MODE_SOLID);
    }

    // 5. Shift Register LED Animation Test (every ~3 seconds)
    if (tick_count % 60 == 0) { // 60 * 50ms = 3000ms
        hal_shift_reg_play_anim(ANIM_MASTER_WAVE, '\0');
    }

    hal_io_tick(); // keep buzzer and led ticking
    hal_shift_reg_tick(); // keep shift reg anim ticking
    vTaskDelay(pdMS_TO_TICKS(50));
    tick_count++;
  }
#else
  // Logic initialization first to load config
  app_state_machine_init();
  
  uint8_t current_mode = app_get_working_mode();

  if (current_mode != 3) {
      // Network and BLE
      wifi_manager_init();
      ble_prov_init();
  } else {
      printf("Wiegand Reader Mode Active: Wi-Fi and BLE are DISABLED for security and power saving.\n");
  }

  // Main Super-Loop
  while (1) {
    // Run state machine tick which handles inputs, time and LED/Buzzer
    // non-blocking io
    app_state_machine_tick();
    
    if (current_mode != 3) {
        wifi_manager_tick();

        static char wifi_scan_buf[2048];
        if (wifi_manager_is_scan_ready(wifi_scan_buf, sizeof(wifi_scan_buf))) {
          ble_prov_send_response(wifi_scan_buf);
        }
    }

    // Crucial for FreeRTOS: allow IDLE task to run and prevent WDT timeouts
    vTaskDelay(pdMS_TO_TICKS(10));
  }
#endif
}
