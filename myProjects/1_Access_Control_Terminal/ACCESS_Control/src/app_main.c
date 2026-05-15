#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pin_manager.h"

// Hardware Drivers
#include "keypad.h"
#include "led_ui.h"
#include "qr_reader.h"
#include "rfid.h"

// Comm Interfaces
#include "ble_prov.h"
#include "comm_osdp.h"
#include "comm_wiegand.h"
#include "server_comm.h"
#include "wifi_manager.h"

// Logic & Storage
#include "state_machine.h"
#include "storage.h"

#include "event_bus.h"

static const char *TAG = "APP_MAIN";

void app_main(void) {
  ESP_LOGI(TAG, "Starting Smart Access Control Terminal...");

  // Initialize Event Bus first since tasks will need it
  event_bus_init();

  // Initialize UI (LEDs & Buzzer) First
  led_ui_init();

  // Initialize Storage Database
  storage_init();

  // Play startup beep
  led_ui_beep(100, 1);

  // Initialize other Hardware
  rfid_init();
  rfid_start_task(); // Start the Core 0 Polling Task

  keypad_init();
  keypad_start_task(); // Start the Core 0 Keypad Interrupt Task

  qr_reader_init();
  qr_reader_start_task();

  // Initialize Comm Interfaces
  comm_wiegand_init();
  comm_osdp_init();
  server_comm_init(); // Init server configs before wifi connects
  wifi_manager_init();

  if (!wifi_manager_is_connected()) {
    ESP_LOGI(TAG, "Wi-Fi not connected. Starting BLE Provisioning...");
    ble_prov_init();
  }

  // Set initial LED status (e.g. SR1 = Red LED on to indicate locked)
  // Assuming SR1 controls RGB LEDs. Let's say bit 0 is Red, bit 1 is Green, bit
  // 2 is Blue
  led_ui_set_sr1(0x01); // Red ON

  // Initialize State Machine and Sub-engines
  state_machine_init();

  ESP_LOGI(TAG, "System Initialization Complete. Starting State Machine Task.");

  state_machine_start_task();
}
