#include "app_state_machine.h"
#include "ble_prov.h"
#include "api_client.h"
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
#include "esp_chip_info.h"
#include "esp_mac.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_system.h"

// ============================================================
// Firmware Version
// ============================================================
#define FW_VERSION_MAJOR  1
#define FW_VERSION_MINOR  6
#define FW_VERSION_PATCH  2
#define FW_VERSION_STR    "v1.6.2"
#define FW_BUILD_DATE     __DATE__
#define FW_BUILD_TIME     __TIME__
// ============================================================

char device_code_str[32] = {0};
char ble_name_str[64] = {0};
uint8_t active_hw_version = HW_VERSION_PIN_QR; // Default

static void app_generate_hw_info(void) {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_BT);
    uint16_t mac_last_2 = (mac[4] << 8) | mac[5];
    
    if (active_hw_version == HW_VERSION_PIN_QR) {
        strcpy(device_code_str, "OE0110");
    } else if (active_hw_version == HW_VERSION_PIN_RFID) {
        strcpy(device_code_str, "OE0111");
    } else if (active_hw_version == HW_VERSION_RFID_ONLY) {
        strcpy(device_code_str, "OE0112");
    } else {
        strcpy(device_code_str, "UNKNOWN");
    }
    
    snprintf(ble_name_str, sizeof(ble_name_str), "SMART_%s_%04X", device_code_str, mac_last_2);
    ESP_LOGI("MAIN", "Project Code: %s, BLE Name: %s", device_code_str, ble_name_str);
}

static void serial_cli_task(void *pvParameters) {
    char buf[128];
    int pos = 0;
    while(1) {
        int c = fgetc(stdin);
        if (c != EOF && c != '\r' && c != '\n') {
            if (pos < sizeof(buf) - 1) {
                buf[pos++] = c;
            }
        } else if (c == '\n' || c == '\r') {
            if (pos > 0) {
                buf[pos] = '\0';
                if (strcmp(buf, "buraky36_fab_reset") == 0) {
                    printf("\n[CLI] Triggering Factory Reset via State Machine...\n");
                    app_set_state(STATE_FACTORY_RESET);
                } else if (strcmp(buf, "buraky36_user_list_view") == 0) {
                    printf("\n--- USER LIST ---\n");
                    int count = 0;
                    for (int i=1; i<=9988; i++) {
                        user_record_t usr;
                        if (nv_storage_get_user(i, &usr)) {
                            printf("User %d: PIN=%s, CARD=%s, QR=%s\n", i, usr.pin, usr.card_id, usr.qr_id);
                            count++;
                        }
                    }
                    printf("Total Users: %d\n", count);
                    printf("--- END OF LIST ---\n");
                } else if (strcmp(buf, "buraky36_master_list_view") == 0) {
                    printf("\n--- MASTER LIST ---\n");
                    sys_config_t cfg;
                    nv_storage_get_config(&cfg);
                    for (int i=0; i<cfg.master_card_count; i++) {
                        printf("Master %d (ID %d): %s\n", i+1, cfg.master_ids[i], cfg.master_cards[i]);
                    }
                    printf("Total Masters: %d\n", cfg.master_card_count);
                    printf("--- END OF LIST ---\n");
                }
                pos = 0;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void app_main(void) {
  printf("\n============================================\n");
  printf(" Access Control Terminal Firmware %s\n", FW_VERSION_STR);
  printf(" Build: %s %s\n", FW_BUILD_DATE, FW_BUILD_TIME);
  printf("============================================\n\n");

  // ESP-IDF core services
  esp_event_loop_create_default();

  // Hardware initialization
  hal_io_init();
  hal_shift_reg_init(); // MUST be before touch and RFID due to RST pins
  nv_storage_init();
  
  bool has_tsm12 = hal_touch_init();
  
  // MFRC522 RST is driven by Shift Register U2 QG (MASK_U2_RFID_RST).
  // After hal_shift_reg_init() all outputs are 0x00 → RST is LOW (active reset).
  // The MFRC522 ignores SPI while RST is LOW, so we must release it first
  // and wait at least 37µs (datasheet) before any SPI transaction.
  hal_shift_reg_set_rfid_rst(true);
  vTaskDelay(pdMS_TO_TICKS(50)); // 50ms: safe margin after hard reset release

  bool has_mfrc522 = mfrc522_init();

  uint8_t detected_hw = HW_VERSION_PIN_QR;
  if (has_tsm12 && has_mfrc522) {
      detected_hw = HW_VERSION_PIN_RFID;
      printf("Hardware Detected: PIN + RFID\n");
  } else if (!has_tsm12 && has_mfrc522) {
      detected_hw = HW_VERSION_RFID_ONLY;
      printf("Hardware Detected: RFID ONLY\n");
  } else if (has_tsm12 && !has_mfrc522) {
      detected_hw = HW_VERSION_PIN_QR;
      printf("Hardware Detected: PIN + QR\n");
  } else {
      detected_hw = HW_VERSION_PIN_QR; // fallback
      printf("Hardware Detected: NONE! Falling back to PIN + QR\n");
  }

  uint8_t saved_hw = nv_storage_get_hw_version();
  if (saved_hw == 0 || saved_hw > 3) {
      saved_hw = detected_hw;
      nv_storage_set_hw_version(saved_hw);
      printf("Hardware version saved to NVS for the first time: %d\n", saved_hw);
  } else if (saved_hw != detected_hw) {
      printf("WARNING: Hardware mismatch! Saved: %d, Detected: %d. Using saved configuration.\n", saved_hw, detected_hw);
  }

  active_hw_version = saved_hw;
  app_generate_hw_info();

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
    if (active_hw_version != HW_VERSION_PIN_QR) {
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
      api_client_init();
  } else {
      printf("Wiegand Reader Mode Active: Wi-Fi and BLE are DISABLED for security and power saving.\n");
  }

  xTaskCreate(serial_cli_task, "serial_cli", 4096, NULL, 5, NULL);

  // Main Super-Loop
  while (1) {
    // Run state machine tick which handles inputs, time and LED/Buzzer
    // non-blocking io
    app_state_machine_tick();
    
    if (current_mode != 3) {
        wifi_manager_tick();
        ble_prov_tick();
        api_client_tick();
    }

    // Crucial for FreeRTOS: allow IDLE task to run and prevent WDT timeouts
    vTaskDelay(pdMS_TO_TICKS(10));
  }
#endif
}
