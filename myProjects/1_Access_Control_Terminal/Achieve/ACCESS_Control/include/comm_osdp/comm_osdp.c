#include "comm_osdp.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "pin_manager.h"
#include <string.h>


static const char *TAG = "RS485_OSDP";

void comm_osdp_init(void) {
  ESP_LOGI(TAG, "Initializing RS485 UART for OSDP...");

  uart_config_t uart_config = {
      .baud_rate = OSDP_BAUD_RATE,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
      .source_clk = UART_SCLK_APB,
  };

  int intr_alloc_flags = 0;

  const int rx_buffer_size = 1024 * 2;
  const int tx_buffer_size = 1024 * 2;

  esp_err_t err = uart_driver_install(
      OSDP_UART_NUM, rx_buffer_size, tx_buffer_size, 0, NULL, intr_alloc_flags);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "UART driver install failed. err: %s", esp_err_to_name(err));
    return;
  }

  err = uart_param_config(OSDP_UART_NUM, &uart_config);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "UART param config failed. err: %s", esp_err_to_name(err));
    return;
  }

  err = uart_set_pin(OSDP_UART_NUM, PIN_RS485_TX, PIN_RS485_RX, PIN_RS485_DE_RE,
                     UART_PIN_NO_CHANGE);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "UART set pin failed. err: %s", esp_err_to_name(err));
    return;
  }

  // Tell the UART driver to handle the RS485 half-duplex control pin (DE/RE)
  uart_set_mode(OSDP_UART_NUM, UART_MODE_RS485_HALF_DUPLEX);

  ESP_LOGI(TAG, "RS485 UART initialized successfully.");
}

void comm_osdp_send(const uint8_t *data, uint16_t len) {
  if (data == NULL || len == 0)
    return;
  uart_write_bytes(OSDP_UART_NUM, (const char *)data, len);
}

uint16_t comm_osdp_read(uint8_t *buffer, uint16_t buffer_len) {
  if (buffer == NULL || buffer_len == 0)
    return 0;

  int length = 0;
  uart_get_buffered_data_len(OSDP_UART_NUM, (size_t *)&length);

  if (length > 0) {
    int read_len = uart_read_bytes(OSDP_UART_NUM, buffer, buffer_len,
                                   20 / portTICK_PERIOD_MS);
    if (read_len > 0) {
      return read_len;
    }
  }
  return 0;
}
