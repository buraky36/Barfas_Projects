#include "door_control.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_ui.h" // Since the relay is driven by the 74HC595 in our current setup
#include "pin_manager.h"


static const char *TAG = "DOOR_CTRL";

void door_control_init(void) {
  ESP_LOGI(TAG, "Initializing Door Control...");

  // Initialize Door Sensor Pin
  gpio_config_t sensor_conf = {
      .intr_type = GPIO_INTR_DISABLE,
      .mode = GPIO_MODE_INPUT,
      .pin_bit_mask = (1ULL << PIN_DOOR_SENSOR),
      .pull_down_en = 0,
      .pull_up_en = 1 // Use internal pull-up if external isn't guaranteed
  };
  gpio_config(&sensor_conf);

  // Initialize Exit Button Pin (if we want to handle it here)
  gpio_config_t exit_btn_conf = {
      .intr_type = GPIO_INTR_ANYEDGE, // Example if we want interrupts later
      .mode = GPIO_MODE_INPUT,
      .pin_bit_mask = (1ULL << PIN_EXIT_BUTTON),
      .pull_down_en = 0,
      .pull_up_en = 1};
  gpio_config(&exit_btn_conf);

  // Ensure door is locked on boot
  door_control_lock();
}

void door_control_unlock(uint32_t duration_ms) {
  ESP_LOGI(TAG, "Unlocking door for %lu ms", duration_ms);
  door_relay_set(true);
  // Usually handled non-blocking via a FreeRTOS timer or state machine in
  // production, but for simplicity we can delay here if called from a dedicated
  // task, or let the state machine handle the timeout. For now, simple
  // blocking:
  vTaskDelay(pdMS_TO_TICKS(duration_ms));
  door_control_lock();
}

void door_control_lock(void) {
  door_relay_set(false);
  ESP_LOGI(TAG, "Door Locked.");
}

bool door_control_is_open(void) {
  // Assuming magnetic sensor reads 0 (LOW) when separated (open)
  return gpio_get_level(PIN_DOOR_SENSOR) == 0;
}
