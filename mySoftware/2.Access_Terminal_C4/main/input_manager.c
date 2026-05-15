#include "input_manager.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "state_manager.h"


static const char *TAG = "INPUT_MGR";

static void input_manager_task(void *pvParameters) {
  ESP_LOGI(TAG, "Input Manager Task started");
  while (1) {
    // Here we would normally poll registers or wait for an interrupt semaphore
    // Examples for stubs:

    // if (mfrc522_is_card_present()) {
    //     state_manager_send_event(EVENT_RFID_SCANNED);
    // }
    //
    // if (tsm12_has_touch_data()) {
    //     state_manager_send_event(EVENT_PIN_ENTERED);
    // }
    //
    // if (xr1300_has_qr_data()) {
    //     state_manager_send_event(EVENT_QR_SCANNED);
    // }

    vTaskDelay(pdMS_TO_TICKS(100)); // Poll every 100ms
  }
}

esp_err_t input_manager_init(void) {
  ESP_LOGI(TAG, "Initializing MFRC522 (SPI)");
  // spi_bus_initialize(...)

  ESP_LOGI(TAG, "Initializing TSM12 (I2C)");
  // i2c_param_config(...)

  ESP_LOGI(TAG, "Initializing XR1300 (UART)");
  // uart_param_config(...)

  xTaskCreate(input_manager_task, "input_task", 4096, NULL, 5, NULL);
  return ESP_OK;
}
