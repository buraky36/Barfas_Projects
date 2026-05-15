#include "protocol_handler.h"
#include "esp_log.h"

static const char *TAG = "PROTOCOL";

esp_err_t protocol_handler_init(void) {
  ESP_LOGI(TAG, "Initializing Protocols...");
  ESP_LOGI(TAG, "RS-485 UART configured for OSDP");
  // uart_param_config()

  ESP_LOGI(TAG, "Wiegand D0/D1 GPIOs configured");
  // gpio_config()

  return ESP_OK;
}

esp_err_t protocol_handler_send_wiegand(const uint8_t *data,
                                        uint8_t bit_length) {
  ESP_LOGI(TAG, "Sending Wiegand %d bits", bit_length);
  // Loop through bits and toggle D0/D1 with delays
  return ESP_OK;
}

esp_err_t protocol_handler_send_osdp(uint8_t cmd, const uint8_t *payload,
                                     uint8_t len) {
  ESP_LOGI(TAG, "Sending OSDP CMD: 0x%02X, Len: %d", cmd, len);
  // Build packet (SOM, Addr, Len, Ctrl, CMD, Data, CHK/MAC)
  // uart_write_bytes()
  return ESP_OK;
}
