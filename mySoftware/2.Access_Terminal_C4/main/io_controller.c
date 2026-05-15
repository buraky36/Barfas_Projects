#include "io_controller.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


static const char *TAG = "IO_CTRL";

// Dummy GPIO Pins (User can adjust later)
#define GPIO_RELAY 12
#define GPIO_BUZZER 13
#define GPIO_LED_G 14
#define GPIO_LED_R 15
#define GPIO_DI1_EXIT 25
#define GPIO_DI2_SENSOR 26

esp_err_t io_controller_init(void) {
  ESP_LOGI(TAG, "Initializing IO Controller");

  gpio_config_t io_conf = {
      .intr_type = GPIO_INTR_DISABLE,
      .mode = GPIO_MODE_OUTPUT,
      .pin_bit_mask = (1ULL << GPIO_RELAY) | (1ULL << GPIO_BUZZER) |
                      (1ULL << GPIO_LED_G) | (1ULL << GPIO_LED_R),
      .pull_down_en = 0,
      .pull_up_en = 0,
  };
  gpio_config(&io_conf);

  io_conf.mode = GPIO_MODE_INPUT;
  io_conf.pin_bit_mask = (1ULL << GPIO_DI1_EXIT) | (1ULL << GPIO_DI2_SENSOR);
  io_conf.pull_up_en = 1; // Assuming active low inputs
  gpio_config(&io_conf);

  io_controller_set_relay(false);
  io_controller_set_led(0);

  return ESP_OK;
}

void io_controller_set_relay(bool open) {
  ESP_LOGI(TAG, "Relay -> %s", open ? "OPEN" : "CLOSED");
  gpio_set_level(GPIO_RELAY, open ? 1 : 0);
}

void io_controller_beep(uint32_t ms) {
  ESP_LOGI(TAG, "Buzzer beep %lu ms", ms);
  gpio_set_level(GPIO_BUZZER, 1);
  vTaskDelay(pdMS_TO_TICKS(ms));
  gpio_set_level(GPIO_BUZZER, 0);
}

void io_controller_set_led(uint8_t state) {
  if (state == 0) {
    gpio_set_level(GPIO_LED_G, 0);
    gpio_set_level(GPIO_LED_R, 0);
  } else if (state == 1) { // Green
    gpio_set_level(GPIO_LED_G, 1);
    gpio_set_level(GPIO_LED_R, 0);
  } else { // Red
    gpio_set_level(GPIO_LED_G, 0);
    gpio_set_level(GPIO_LED_R, 1);
  }
}

bool io_controller_get_door_sensor(void) {
  // Active low: 0 = Closed
  return gpio_get_level(GPIO_DI2_SENSOR) == 0;
}
