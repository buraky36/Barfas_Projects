#include "state_machine.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"

// Hardware and Communication
#include "comm_osdp.h"
#include "comm_wiegand.h"
#include "event_bus.h"
#include "keypad.h"
#include "led_ui.h"
#include "qr_reader.h"
#include "rfid.h"

#include "auth_engine.h"
#include "door_control.h"

#include "server_comm.h"
#include "wifi_manager.h"

static const char *TAG = "STATE_MACHINE";
static device_mode_t current_mode = DEVICE_MODE_CONTROLLER; // Default mode

#define KEYPAD_TIMEOUT_MS 5000
#define RFID_BLOCK_MS 3000
#define PIN_MAX_LEN 16

#define KEY_STAR 10
#define KEY_HASH 11

void state_machine_init(void) {
  ESP_LOGI(TAG, "Initialization. Default Mode: %s",
           current_mode == DEVICE_MODE_READER ? "READER" : "CONTROLLER");

  auth_engine_init();
  door_control_init();
}

void state_machine_set_mode(device_mode_t mode) {
  current_mode = mode;
  ESP_LOGI(TAG, "Mode switched to: %s",
           current_mode == DEVICE_MODE_READER ? "READER" : "CONTROLLER");
}

static void state_machine_task_runner(void *pvParameters) {
  ESP_LOGI(TAG, "State Machine Main Loop Running on Core %d", xPortGetCoreID());

  // Initial UI state: Locked (Red LED)
  led_ui_set_sr1(0x01);

  while (1) {
    // --- 1. Process Event Bus ---
    system_event_t evt;
    // Wait up to 50ms for an event
    if (event_bus_receive(&evt, pdMS_TO_TICKS(50))) {
      switch (evt.type) {
      case EVENT_CARD_SCANNED: {
        uint32_t card_uid = evt.payload.card_uid;
        ESP_LOGI(TAG, "Event Received: Card Scanned: %lu", card_uid);
        led_ui_beep(50, 1);

        if (current_mode == DEVICE_MODE_READER) {
          // In Reader Mode, send via Wiegand
          led_ui_set_sr1(0x02); // Green feedback
          comm_wiegand_send_34(card_uid);
          // Avoid long blocking delays in the main event loop
          // In a full implementation, we'd use a timer or another task to
          // revert the LED
          led_ui_set_sr1(0x01);
        } else {
          // In Controller Mode, validate locally/remotely
          auth_result_t res = auth_engine_validate_card(card_uid);

          char card_str[16];
          snprintf(card_str, sizeof(card_str), "%lu", card_uid);

          if (res == AUTH_RESULT_GRANTED) {
            led_ui_set_sr1(0x02);      // Green means go
            led_ui_beep(50, 2);        // Happy double beep
            door_control_unlock(3000); // Open door for 3 seconds

            // Send log to server
            if (wifi_manager_is_connected()) {
              server_comm_send_event(SERVER_EVENT_UNLOCK_CARD, card_str);
            }

            led_ui_set_sr1(0x01); // Revert to red
          } else {
            led_ui_beep(300, 1); // Angry long beep
            // Send alarm log to server
            if (wifi_manager_is_connected()) {
              server_comm_send_alarm(SERVER_ALARM_WRONG_CARD);
            }
          }
        }
        break;
      }
      case EVENT_PIN_ENTERED: {
        ESP_LOGI(TAG, "Event Received: PIN Entered: %s", evt.payload.pin);

        if (current_mode == DEVICE_MODE_READER) {
          // Placeholder: transmit PIN upward (e.g. Wiegand bursts)
          // comm_wiegand_send_pin(evt.payload.pin);
        } else {
          // In Controller Mode, Auth Engine makes the decision
          // For now just beep to acknowledge the reception
          led_ui_beep(100, 2);

          if (wifi_manager_is_connected()) {
            server_comm_send_event(SERVER_EVENT_UNLOCK_PASSWORD,
                                   evt.payload.pin);
          }
          // auth_result_t res = auth_engine_validate_pin(evt.payload.pin);
          // ... Validation logic here ...
        }
        break;
      }
      case EVENT_QR_SCANNED: {
        ESP_LOGI(TAG, "Event Received: QR Scanned: %s", evt.payload.qr_data);

        if (current_mode == DEVICE_MODE_READER) {
          // Placeholder: transmit QR payload upward (OSDP / HTTP)
          led_ui_beep(50, 1);
        } else {
          // In Controller Mode, validate locally/remotely
          // auth_result_t res = auth_engine_validate_qr(evt.payload.qr_data);
          // ... Validation logic here ...
          led_ui_beep(50, 2);
          door_control_unlock(3000);

          if (wifi_manager_is_connected()) {
            server_comm_send_event(SERVER_EVENT_UNLOCK_APP,
                                   evt.payload.qr_data);
          }
        }
        break;
      }
      default:
        break;
      }
      }
    }
  }

  void state_machine_start_task(void) {
    xTaskCreatePinnedToCore(state_machine_task_runner, "state_machine", 8192,
                            NULL,
                            4, // Lower priority than IO
                            NULL,
                            1 // Core 1
    );
  }
