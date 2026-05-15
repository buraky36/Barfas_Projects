#include "keypad.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "event_bus.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_ui.h" // For beep feedback in task


static const char *TAG = "KEYPAD_TSM12";
static TaskHandle_t keypad_task_handle = NULL;

#define KEYPAD_TIMEOUT_MS 5000
#define PIN_MAX_LEN 16

static const uint8_t key_sort[12] = {
    3, 2,  1, // CH1-CH3
    6, 5,  4, // CH4-CH6
    7, 8,  9, // CH7-CH9
    0, 10, 11 // CH10=0, CH11=START(*), CH12=HASH(#)
};

#define KEY_STAR 10
#define KEY_HASH 11

// ISR handler for TSM12 INT pin
static void IRAM_ATTR keypad_isr_handler(void *arg) {
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  if (keypad_task_handle != NULL) {
    vTaskNotifyGiveFromISR(keypad_task_handle, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken) {
      portYIELD_FROM_ISR();
    }
  }
}

// Default TSM12 I2C address (7-bit) -> 0xD0 >> 1
#define TSM12_I2C_ADDR 0x68

// Register for reading key status
#define TSM12_REG_OUTPUT1 0x10

// Helper to write to TSM12 register
static esp_err_t __attribute__((unused)) tsm12_write_reg(uint8_t reg, uint8_t val) {
  uint8_t data[2] = {reg, val};
  return i2c_master_write_to_device(I2C_PORT_TSM12, TSM12_I2C_ADDR, data, 2,
                                    pdMS_TO_TICKS(100));
}

// Helper to read from TSM12 register
static esp_err_t tsm12_read_reg(uint8_t reg, uint8_t *data, size_t len) {
  return i2c_master_write_read_device(I2C_PORT_TSM12, TSM12_I2C_ADDR, &reg, 1,
                                      data, len, pdMS_TO_TICKS(100));
}

void keypad_init(void) {
  ESP_LOGI(TAG, "Initializing TSM12 Touch I2C...");

  i2c_config_t conf = {
      .mode = I2C_MODE_MASTER,
      .sda_io_num = PIN_TSM12_SDA,
      .sda_pullup_en = GPIO_PULLUP_ENABLE,
      .scl_io_num = PIN_TSM12_SCL,
      .scl_pullup_en = GPIO_PULLUP_ENABLE,
      .master.clk_speed = TSM12_I2C_FREQ_HZ,
  };

  esp_err_t err = i2c_param_config(I2C_PORT_TSM12, &conf);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "I2C param config failed.");
    return;
  }

  err = i2c_driver_install(I2C_PORT_TSM12, conf.mode, 0, 0, 0);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "I2C driver install failed.");
    return;
  }

  gpio_config_t io_conf = {.intr_type = GPIO_INTR_DISABLE,
                           .mode = GPIO_MODE_OUTPUT,
                           .pin_bit_mask = (1ULL << PIN_TSM12_RST),
                           .pull_down_en = 0,
                           .pull_up_en = 0};
  gpio_config(&io_conf);

  gpio_config_t int_conf = {
      .intr_type = GPIO_INTR_NEGEDGE, // TSM12 pulls INT low when touched
      .mode = GPIO_MODE_INPUT,
      .pin_bit_mask = (1ULL << PIN_TSM12_INT),
      .pull_down_en = 0,
      .pull_up_en = 1};
  gpio_config(&int_conf);

  gpio_install_isr_service(0);
  gpio_isr_handler_add(PIN_TSM12_INT, keypad_isr_handler, NULL);

  // Hard reset
  gpio_set_level(PIN_TSM12_RST, 0);
  vTaskDelay(pdMS_TO_TICKS(50));
  gpio_set_level(PIN_TSM12_RST, 1);
  vTaskDelay(pdMS_TO_TICKS(50));

  // Basic config (Example: Set sensitivity or operation mode in CTRL1)
  // tsm12_write_reg(TSM12_REG_CTRL1, 0x00);

  ESP_LOGI(TAG, "TSM12 I2C hardware initialized successfully.");
}

bool keypad_is_touched(void) {
  // If INT is pulled low by the TSM12 on touch
  return (gpio_get_level(PIN_TSM12_INT) == 0);
}

bool keypad_read(uint16_t *keys) {
  if (!keypad_is_touched()) {
    *keys = 0;
    return true;
  }

  uint8_t data[3] = {0};
  esp_err_t err = tsm12_read_reg(TSM12_REG_OUTPUT1, data, 3);
  if (err == ESP_OK) {
    // Construct 24-bit output
    uint32_t temp = (data[2] << 16) | (data[1] << 8) | data[0];

    // Convert to a 12-bit bitmask where 1 bit = 1 key
    uint16_t mask = 0;
    for (int i = 0; i < 12; i++) {
      if (temp & (0x03 << (i * 2))) {
        mask |= (1 << i);
      }
    }
    *keys = mask;
    return true;
  }

  ESP_LOGE(TAG, "TSM12 read failed");
  *keys = 0;
  return false;
}

uint8_t keypad_get_key(void) {
  uint16_t keys;
  if (keypad_read(&keys)) {
    for (int i = 0; i < 12; i++) {
        if (keys & (1 << i)) {
            return key_sort[i];
        }
    }
  }
  return 0xFF; // Return 0xFF for 'No Key' because 0 is a valid key digit
}


static void keypad_task_runner(void *pvParameters) {
  ESP_LOGI(TAG, "Keypad Task Started on Core %d", xPortGetCoreID());

  char pin_buffer[PIN_MAX_LEN + 1];
  uint8_t pin_idx = 0;
  uint32_t last_keypress_time = 0;
  uint8_t last_key = 0xFF;

  memset(pin_buffer, 0, sizeof(pin_buffer));

  while (1) {
    // Wait for the ISR to notify us (or timeout to check for pin timeout)
    uint32_t notified = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100));
    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

    // Check timeout
    if (pin_idx > 0 &&
        (current_time - last_keypress_time) > KEYPAD_TIMEOUT_MS) {
      ESP_LOGI(TAG, "Keypad timeout. Clearing PIN buffer.");
      pin_idx = 0;
      memset(pin_buffer, 0, sizeof(pin_buffer));
      led_ui_beep(50, 2); // Timeout feedback
    }

    if (notified > 0 || keypad_is_touched()) {
      uint8_t key = keypad_get_key();

      if (key != 0xFF && key != last_key) {
        last_key = key;
        last_keypress_time = current_time;
        led_ui_beep(20, 1);

        if (key == KEY_STAR) {
          pin_idx = 0;
          memset(pin_buffer, 0, sizeof(pin_buffer));
          ESP_LOGI(TAG, "PIN Input Cleared / Canceled");
        } else if (key == KEY_HASH) {
          if (pin_idx > 0) {
            ESP_LOGI(TAG, "PIN Submission triggered (%d digits)", pin_idx);

            system_event_t evt;
            evt.type = EVENT_PIN_ENTERED;
            strncpy(evt.payload.pin, pin_buffer, PIN_MAX_LEN);
            evt.payload.pin[PIN_MAX_LEN] = '\0'; // ensure null termination

            event_bus_publish(&evt);

            pin_idx = 0;
            memset(pin_buffer, 0, sizeof(pin_buffer));
          }
        } else { // 0-9
          if (pin_idx < PIN_MAX_LEN) {
            pin_buffer[pin_idx] = '0' + key; // Convert to char
            pin_idx++;
            ESP_LOGI(TAG, "Added digit %d to PIN buffer. Length: %d", key,
                     pin_idx);
          }
        }
      } else if (key == 0xFF) {
        last_key = 0xFF; // released
      }
    }
  }
}

void keypad_start_task(void) {
  keypad_task_handle = NULL;
  xTaskCreatePinnedToCore(keypad_task_runner, "keypad_task", 4096, NULL, 5,
                          &keypad_task_handle,
                          0 // Core 0
  );
}
