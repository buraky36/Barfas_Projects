#include "qr_reader.h"
#include "esp_log.h"
#include "event_bus.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "led_ui.h"
#include <string.h>


static const char *TAG = "QR_XR1300";
static QueueHandle_t qr_uart_queue;
static TaskHandle_t qr_task_handle = NULL;

void qr_reader_init(void) {
  ESP_LOGI(TAG, "Initializing XR1300 QR Reader UART...");

  uart_config_t uart_config = {
      .baud_rate = QR_UART_BAUD_RATE,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
      .source_clk = UART_SCLK_APB,
  };

  int intr_alloc_flags = 0;

  // Buffer sizes
  const int rx_buffer_size = 1024 * 2;
  const int tx_buffer_size = 0; // Not sending data back usually

  esp_err_t err =
      uart_driver_install(UART_PORT_QR, rx_buffer_size, tx_buffer_size, 20,
                          &qr_uart_queue, intr_alloc_flags);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "UART driver install failed. err: %s", esp_err_to_name(err));
    return;
  }

  err = uart_param_config(UART_PORT_QR, &uart_config);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "UART param config failed. err: %s", esp_err_to_name(err));
    return;
  }

  err = uart_set_pin(UART_PORT_QR, PIN_QR_TX, PIN_QR_RX, UART_PIN_NO_CHANGE,
                     UART_PIN_NO_CHANGE);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "UART set pin failed. err: %s", esp_err_to_name(err));
    return;
  }

  ESP_LOGI(TAG, "XR1300 UART hardware initialized successfully.");
}

bool qr_reader_get_data(char *buffer, uint16_t buffer_len) {
  if (buffer == NULL || buffer_len == 0)
    return false;

  int length = 0;
  uart_get_buffered_data_len(UART_PORT_QR, (size_t *)&length);

  if (length > 0) {
    int read_len = uart_read_bytes(UART_PORT_QR, buffer, (buffer_len - 1),
                                   20 / portTICK_PERIOD_MS);
    if (read_len > 0) {
      buffer[read_len] = '\0'; // Null-terminate string
      return true;
    }
  }
  return false;
}

static void qr_reader_task_runner(void *pvParameters) {
  ESP_LOGI(TAG, "QR Reader Task Started on Core %d", xPortGetCoreID());
  uart_event_t event;
  char qr_buffer[256];

  while (1) {
    // Wait for UART event
    if (xQueueReceive(qr_uart_queue, (void *)&event,
                      (TickType_t)portMAX_DELAY)) {
      switch (event.type) {
      // Event of UART receiving data
      case UART_DATA:
        memset(qr_buffer, 0, sizeof(qr_buffer));
        int len = uart_read_bytes(UART_PORT_QR, qr_buffer,
                                  sizeof(qr_buffer) - 1, pdMS_TO_TICKS(100));
        if (len > 0) {
          qr_buffer[len] = '\0';
          ESP_LOGI(TAG, "QR Scanned: %s", qr_buffer);

          // Beep for feedback
          led_ui_beep(50, 1);

          system_event_t evt;
          evt.type = EVENT_QR_SCANNED;
          strncpy(evt.payload.qr_data, qr_buffer,
                  sizeof(evt.payload.qr_data) - 1);
          evt.payload.qr_data[sizeof(evt.payload.qr_data) - 1] = '\0';

          event_bus_publish(&evt);
        }
        break;
      // Event of HW FIFO overflow detected
      case UART_FIFO_OVF:
        ESP_LOGW(TAG, "hw fifo overflow");
        uart_flush_input(UART_PORT_QR);
        xQueueReset(qr_uart_queue);
        break;
      // Event of UART ring buffer full
      case UART_BUFFER_FULL:
        ESP_LOGW(TAG, "ring buffer full");
        uart_flush_input(UART_PORT_QR);
        xQueueReset(qr_uart_queue);
        break;
      default:
        ESP_LOGI(TAG, "uart event type: %d", event.type);
        break;
      }
    }
  }
}

void qr_reader_start_task(void) {
  qr_task_handle = NULL;
  xTaskCreatePinnedToCore(qr_reader_task_runner, "qr_reader_task", 4096, NULL,
                          5, &qr_task_handle,
                          0 // Core 0
  );
}
