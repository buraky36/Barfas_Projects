#include "xr1300.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "hw_config.h"
#include <string.h>

#define XR1300_UART_NUM UART_NUM_1
#define XR1300_TXD_PIN PIN_QR_TX
#define XR1300_RXD_PIN PIN_QR_RX
#define RX_BUF_SIZE 1024

static QueueHandle_t xr1300_uart_queue = NULL;
static QueueHandle_t xr1300_event_queue = NULL;
static TaskHandle_t xr1300_task_handle = NULL;

static void xr1300_task(void *arg) {
  uart_event_t event;
  uint8_t temp_buf[512];
  int rx_idx = 0;

  while (1) {
    // Wait for UART event. After first byte arrives, use 20 ms burst-end
    // detection timeout.
    TickType_t timeout = (rx_idx > 0) ? pdMS_TO_TICKS(20) : portMAX_DELAY;

    if (xQueueReceive(xr1300_uart_queue, (void *)&event, timeout)) {
      switch (event.type) {

      case UART_DATA: {
        // Give the hardware buffer 5 ms to settle before reading.
        // timeout=0 could miss bytes if the scheduler context-switched
        // between the ISR queuing the event and this read.
        int len =
            uart_read_bytes(XR1300_UART_NUM, temp_buf + rx_idx,
                            sizeof(temp_buf) - 1 - rx_idx, pdMS_TO_TICKS(5));
        if (len > 0) {
          rx_idx += len;
        }
        break;
      }

      case UART_FIFO_OVF:
      case UART_BUFFER_FULL:
        // Real hardware errors — discard accumulated data and reset.
        printf("[QR XR1300] UART error (type=%d), flushing.\n", event.type);
        uart_flush_input(XR1300_UART_NUM);
        xQueueReset(xr1300_uart_queue);
        rx_idx = 0;
        break;

      default:
        // UART_BREAK, UART_PATTERN_DET, etc. — benign, ignore.
        // Previously these caused a full flush which silently discarded
        // valid in-flight QR data.
        break;
      }
    } else {
      // Timeout: no new event for 20 ms → burst is complete.
      if (rx_idx > 0) {
        temp_buf[rx_idx] = '\0';
        printf("[QR XR1300] RAW Rx (%d bytes): %s\n", rx_idx, temp_buf);

        // Strip non-printable characters (\r, \n, control codes).
        char clean_str[64] = {0};
        int out_idx = 0;
        for (int i = 0; i < rx_idx; i++) {
          uint8_t c = temp_buf[i];
          if (c >= 32 && c <= 126 && out_idx < (int)sizeof(clean_str) - 1) {
            clean_str[out_idx++] = (char)c;
          }
        }

        if (out_idx > 0) {
          xQueueSend(xr1300_event_queue, clean_str, 0);
        }
        rx_idx = 0;
      }
    }
  }
}

void xr1300_init(void) {
  const uart_config_t uart_config = {
      .baud_rate = 9600,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
      .source_clk = UART_SCLK_APB,
  };

  uart_param_config(XR1300_UART_NUM, &uart_config);
  uart_set_pin(XR1300_UART_NUM, XR1300_TXD_PIN, XR1300_RXD_PIN,
               UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

  uart_set_line_inverse(XR1300_UART_NUM, UART_SIGNAL_INV_DISABLE);

  // Install UART driver with an event queue
  uart_driver_install(XR1300_UART_NUM, RX_BUF_SIZE * 2, RX_BUF_SIZE * 2, 20,
                      &xr1300_uart_queue, 0);

  // Create the application event queue for parsed QR strings
  xr1300_event_queue =
      xQueueCreate(5, 64); // Holds up to 5 QR strings of max 64 chars

  // Create the background task for UART processing
  xTaskCreate(xr1300_task, "xr1300_task", 4096, NULL, 5, &xr1300_task_handle);

  // Send XR1300 Hardware Initialization Commands
  printf("[QR XR1300] Init at 9600 baud (TX=GPIO%d, RX=GPIO%d)...\n",
         XR1300_TXD_PIN, XR1300_RXD_PIN);

  const char *init_cmds[] = {
      // Continuous scan mode (from XR1300 datasheet):
      //   $100003-FACE  Continuous Mode  <- active
      //   $100004-6359  Auto-sensing Mode
      // Continuous mode keeps the scanner always active without needing
      // motion detection. Same-barcode inhibit below prevents duplicate output.
      "$100003-FACE\r\n", // Continuous scan mode (datasheet confirmed)
      "$1003F2-388B\r\n", // Same barcode delay 3s
      "$010207-26A3\r\n"  // Save settings
  };

  for (int i = 0; i < 3; i++) {
    uart_write_bytes(XR1300_UART_NUM, init_cmds[i], strlen(init_cmds[i]));
    vTaskDelay(pdMS_TO_TICKS(250));
  }
}

// Non-blocking poll from the queue
bool xr1300_read_qr(char *qr_string_out, uint16_t max_len) {
  if (!qr_string_out || max_len == 0)
    return false;

  char temp[64];
  if (xQueueReceive(xr1300_event_queue, temp, 0) == pdTRUE) {
    strncpy(qr_string_out, temp, max_len - 1);
    qr_string_out[max_len - 1] = '\0';
    return true;
  }

  return false;
}
