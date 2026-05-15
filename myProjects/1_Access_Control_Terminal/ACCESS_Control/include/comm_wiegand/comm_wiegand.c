#include "comm_wiegand.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pin_manager.h"
#include "rom/ets_sys.h"


static const char *TAG = "WIEGAND";

// Wiegand timing variables (Typical: Pulse width 50us, Interval 2ms)
#define WIEGAND_PULSE_US 50
#define WIEGAND_INTERVAL_MS 2

void comm_wiegand_init(void) {
  ESP_LOGI(TAG, "Initializing Wiegand Interface on D0:%d, D1:%d",
           PIN_WIEGAND_D0, PIN_WIEGAND_D1);

  // Initial configuration for Wiegand OUTPUT mode (idle high)
  gpio_config_t io_conf = {
      .intr_type = GPIO_INTR_DISABLE,
      .mode = GPIO_MODE_OUTPUT_OD, // Open drain is safest for mixed IO
      .pin_bit_mask = (1ULL << PIN_WIEGAND_D0) | (1ULL << PIN_WIEGAND_D1),
      .pull_down_en = 0,
      .pull_up_en = 1 // Pull up required for Wiegand
  };
  gpio_config(&io_conf);

  gpio_set_level(PIN_WIEGAND_D0, 1);
  gpio_set_level(PIN_WIEGAND_D1, 1);

  // Note: To support INPUT mode at the same time or switching, interrupts need
  // to be attached. Skeleton implementation assumes Output by default.
}

static void send_bit(uint8_t bit) {
  if (bit == 0) {
    gpio_set_level(PIN_WIEGAND_D0, 0);
    ets_delay_us(WIEGAND_PULSE_US);
    gpio_set_level(PIN_WIEGAND_D0, 1);
  } else {
    gpio_set_level(PIN_WIEGAND_D1, 0);
    ets_delay_us(WIEGAND_PULSE_US);
    gpio_set_level(PIN_WIEGAND_D1, 1);
  }
  vTaskDelay(pdMS_TO_TICKS(WIEGAND_INTERVAL_MS));
}

void comm_wiegand_send_26(uint8_t facility_code, uint16_t card_number) {
  // 26-bit format: Even Parity (1 bit) + Facility (8 bits) + Card (16 bits) +
  // Odd Parity (1 bit)

  uint32_t payload = (facility_code << 16) | card_number;

  // Calculate simple even parity for first 12 bits
  uint8_t even_parity = 0;
  for (int i = 12; i < 24; i++) {
    even_parity ^= ((payload >> i) & 1);
  }

  // Calculate simple odd parity for last 12 bits
  uint8_t odd_parity = 1;
  for (int i = 0; i < 12; i++) {
    odd_parity ^= ((payload >> i) & 1);
  }

  ESP_LOGD(TAG, "Sending WG26: FAC:%d CARD:%d", facility_code, card_number);

  send_bit(even_parity);
  for (int i = 23; i >= 0; i--) {
    send_bit((payload >> i) & 1);
  }
  send_bit(odd_parity);
}

void comm_wiegand_send_34(uint32_t data) {
  // 34-bit format logic (Even parity over first 16, Odd parity over next 16)
  // Send even parity
  uint8_t even_parity = 0;
  for (int i = 16; i < 32; i++) {
    even_parity ^= ((data >> i) & 1);
  }
  send_bit(even_parity);

  // Send data
  for (int i = 31; i >= 0; i--) {
    send_bit((data >> i) & 1);
  }

  // Send odd parity
  uint8_t odd_parity = 1;
  for (int i = 0; i < 16; i++) {
    odd_parity ^= ((data >> i) & 1);
  }
  send_bit(odd_parity);
}

bool comm_wiegand_read(uint32_t *data, uint8_t *bits) {
  // Skeleton implementation: Wiegand Input requires interrupt handlers on D0
  // and D1. They populate a buffer and parse the format after a timeout.
  return false;
}
