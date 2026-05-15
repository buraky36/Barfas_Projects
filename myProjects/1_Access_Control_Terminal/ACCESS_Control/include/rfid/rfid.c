#include "rfid.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "event_bus.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mfrc522_regs.h"
#include "pin_manager.h"
#include <string.h>

static const char *TAG = "RFID_MFRC522";
static spi_device_handle_t spi_handle;
static TaskHandle_t rfid_task_handle = NULL;

// ISR handler for MFRC522 IRQ
static void IRAM_ATTR mfrc522_isr_handler(void *arg) {
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  if (rfid_task_handle != NULL) {
    vTaskNotifyGiveFromISR(rfid_task_handle, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken) {
      portYIELD_FROM_ISR();
    }
  }
}

// Low level SPI write
static void mfrc522_write_reg(uint8_t reg, uint8_t val) {
  spi_transaction_t t;
  memset(&t, 0, sizeof(t));
  t.length = 16;
  uint16_t tx_data = (((reg << 1) & 0x7E) << 8) | val;
  t.tx_buffer = &tx_data;
  t.flags = SPI_TRANS_USE_TXDATA;
  t.tx_data[0] = (reg << 1) & 0x7E;
  t.tx_data[1] = val;
  spi_device_polling_transmit(spi_handle, &t);
}

// Low level SPI read
static uint8_t mfrc522_read_reg(uint8_t reg) {
  spi_transaction_t t;
  memset(&t, 0, sizeof(t));
  t.length = 16;
  t.flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA;
  t.tx_data[0] = ((reg << 1) & 0x7E) | 0x80;
  t.tx_data[1] = 0;
  spi_device_polling_transmit(spi_handle, &t);
  return t.rx_data[1];
}

static void mfrc522_set_bit_mask(uint8_t reg, uint8_t mask) {
  uint8_t tmp = mfrc522_read_reg(reg);
  mfrc522_write_reg(reg, tmp | mask);
}

static void mfrc522_clear_bit_mask(uint8_t reg, uint8_t mask) {
  uint8_t tmp = mfrc522_read_reg(reg);
  mfrc522_write_reg(reg, tmp & (~mask));
}

static void mfrc522_antenna_on(void) {
  uint8_t temp = mfrc522_read_reg(MFRC522_REG_TX_CONTROL);
  if (!(temp & 0x03)) {
    mfrc522_set_bit_mask(MFRC522_REG_TX_CONTROL, 0x03);
  }
}

static void mfrc522_reset(void) {
  mfrc522_write_reg(MFRC522_REG_COMMAND, MFRC522_CMD_SOFT_RESET);
  vTaskDelay(pdMS_TO_TICKS(50));
}

void rfid_init(void) {
  ESP_LOGI(TAG, "Initializing MFRC522 SPI...");

  spi_bus_config_t buscfg = {.miso_io_num = PIN_RFID_MISO,
                             .mosi_io_num = PIN_RFID_MOSI,
                             .sclk_io_num = PIN_RFID_CLK,
                             .quadwp_io_num = -1,
                             .quadhd_io_num = -1,
                             .max_transfer_sz = 32};

  spi_device_interface_config_t devcfg = {
      .clock_speed_hz = 1 * 1000 * 1000,
      .mode = 0,
      .spics_io_num = PIN_RFID_CS,
      .queue_size = 7,
  };

  esp_err_t ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize SPI bus. err: %s",
             esp_err_to_name(ret));
  }

  ret = spi_bus_add_device(SPI2_HOST, &devcfg, &spi_handle);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to add SPI device. err: %s", esp_err_to_name(ret));
  }

  // Configure RST pin
  gpio_config_t rst_conf = {.intr_type = GPIO_INTR_DISABLE,
                            .mode = GPIO_MODE_OUTPUT,
                            .pin_bit_mask = (1ULL << PIN_RFID_RST),
                            .pull_down_en = 0,
                            .pull_up_en = 0};
  gpio_config(&rst_conf);

  // Configure IRQ pin
  gpio_config_t irq_conf = {.intr_type =
                                GPIO_INTR_NEGEDGE, // MFRC522 IRQ is active LOW
                            .mode = GPIO_MODE_INPUT,
                            .pin_bit_mask = (1ULL << PIN_RFID_IRQ),
                            .pull_down_en = 0,
                            .pull_up_en = 1};
  gpio_config(&irq_conf);

  // Install ISR service if not already installed (allow acceptable failure if
  // installed by another driver)
  gpio_install_isr_service(0);
  gpio_isr_handler_add(PIN_RFID_IRQ, mfrc522_isr_handler, NULL);

  // Hard reset
  gpio_set_level(PIN_RFID_RST, 0);
  vTaskDelay(pdMS_TO_TICKS(50));
  gpio_set_level(PIN_RFID_RST, 1);
  vTaskDelay(pdMS_TO_TICKS(50));

  // Soft reset
  mfrc522_reset();

  // Invert MFRC522 IRQ pin? Default is CMOS active LOW.
  // Let's explicitly clear DivIEnReg IRqInv bit (bit 7) to ensure it's active
  // LOW.
  mfrc522_clear_bit_mask(0x03, 0x80); // 0x03 is MFRC522_REG_DIVIEN

  // Configuration
  mfrc522_write_reg(MFRC522_REG_T_MODE, 0x8D);
  mfrc522_write_reg(MFRC522_REG_T_PRESCALER, 0x3E);
  mfrc522_write_reg(MFRC522_REG_T_RELOAD_L, 30);
  mfrc522_write_reg(MFRC522_REG_T_RELOAD_H, 0);
  mfrc522_write_reg(MFRC522_REG_TX_ASK, 0x40);
  mfrc522_write_reg(MFRC522_REG_MODE, 0x3D);

  mfrc522_antenna_on();

  // Check firmware version
  uint8_t fw_ver = mfrc522_read_reg(0x37); // VersionReg
  ESP_LOGI(TAG, "MFRC522 Firmware Version: 0x%02X", fw_ver);
  if (fw_ver == 0x00 || fw_ver == 0xFF) {
    ESP_LOGW(TAG, "Communication failure, check SPI connections.");
  } else {
    ESP_LOGI(TAG, "MFRC522 SPI hardware initialized successfully.");
  }
}

// Low level ToCard communication
static int mfrc522_to_card(uint8_t command, uint8_t *sendData, uint8_t sendLen,
                           uint8_t *backData, uint32_t *backLen) {
  int status = MI_ERR;
  uint8_t irqEn = 0x00;
  uint8_t waitIRq = 0x00;
  uint8_t lastBits;
  uint8_t n;
  uint32_t i;

  switch (command) {
  case MFRC522_CMD_AUTHENT:
    irqEn = 0x12;
    waitIRq = 0x10;
    break;
  case MFRC522_CMD_TRANSCEIVE:
    irqEn = 0x77;
    waitIRq = 0x30;
    break;
  default:
    break;
  }

  mfrc522_write_reg(MFRC522_REG_COMIEN, irqEn | 0x80);
  mfrc522_clear_bit_mask(MFRC522_REG_COMIRQ, 0x80);
  mfrc522_set_bit_mask(MFRC522_REG_FIFO_LEVEL, 0x80);

  mfrc522_write_reg(MFRC522_REG_COMMAND, MFRC522_CMD_IDLE);

  for (i = 0; i < sendLen; i++) {
    mfrc522_write_reg(MFRC522_REG_FIFO_DATA, sendData[i]);
  }

  mfrc522_write_reg(MFRC522_REG_COMMAND, command);

  if (command == MFRC522_CMD_TRANSCEIVE) {
    mfrc522_set_bit_mask(MFRC522_REG_BIT_FRAMING, 0x80);
  }

  // Register this task to receive the IRQ notification
  rfid_task_handle = xTaskGetCurrentTaskHandle();
  ulTaskNotifyTake(pdTRUE, 0); // Clear any pending notifications

  // Wait for the IRQ pin to fire (with a timeout fallback)
  uint32_t notified = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(50));

  rfid_task_handle = NULL; // Unregister task

  n = mfrc522_read_reg(MFRC522_REG_COMIRQ);

  mfrc522_clear_bit_mask(MFRC522_REG_BIT_FRAMING, 0x80);

  if (notified > 0 || (n & 0x01) || (n & waitIRq)) {
    if (!(mfrc522_read_reg(MFRC522_REG_ERROR) & 0x1B)) {
      status = MI_OK;
      if (n & irqEn & 0x01) {
        status = MI_NOTAGERR;
      }
      if (command == MFRC522_CMD_TRANSCEIVE) {
        n = mfrc522_read_reg(MFRC522_REG_FIFO_LEVEL);
        lastBits = mfrc522_read_reg(MFRC522_REG_CONTROL) & 0x07;
        if (lastBits) {
          *backLen = (n - 1) * 8 + lastBits;
        } else {
          *backLen = n * 8;
        }
        if (n == 0)
          n = 1;
        if (n > 16)
          n = 16;
        for (i = 0; i < n; i++) {
          backData[i] = mfrc522_read_reg(MFRC522_REG_FIFO_DATA);
        }
      }
    } else {
      status = MI_ERR;
    }
  }

  return status;
}

static int mfrc522_request(uint8_t reqMode, uint8_t *tagType) {
  int status;
  uint32_t backBits;

  mfrc522_write_reg(MFRC522_REG_BIT_FRAMING, 0x07);
  tagType[0] = reqMode;
  status =
      mfrc522_to_card(MFRC522_CMD_TRANSCEIVE, tagType, 1, tagType, &backBits);

  if ((status != MI_OK) || (backBits != 0x10)) {
    status = MI_ERR;
  }
  return status;
}

static int mfrc522_anticoll(uint8_t *serNum) {
  int status;
  uint8_t i;
  uint8_t serNumCheck = 0;
  uint32_t unLen;

  mfrc522_write_reg(MFRC522_REG_BIT_FRAMING, 0x00);
  serNum[0] = PICC_ANTICOLL;
  serNum[1] = 0x20;
  status = mfrc522_to_card(MFRC522_CMD_TRANSCEIVE, serNum, 2, serNum, &unLen);

  if (status == MI_OK) {
    for (i = 0; i < 4; i++) {
      serNumCheck ^= serNum[i];
    }
    if (serNumCheck != serNum[i]) {
      status = MI_ERR;
    }
  }
  return status;
}

bool rfid_is_card_present(void) {
  uint8_t str[2];
  int status = mfrc522_request(PICC_REQIDL, str);
  return (status == MI_OK);
}

bool rfid_read_uid(uint8_t *uid, uint8_t *uid_len) {
  int status = mfrc522_anticoll(uid);
  if (status == MI_OK) {
    *uid_len = 4;
    return true;
  }
  return false;
}

// The core 0 task for evaluating RFID
static void rfid_task_runner(void *pvParameters) {
  ESP_LOGI(TAG, "RFID Task Started on Core %d", xPortGetCoreID());
  uint32_t last_card_uid = 0;
  uint32_t last_scan_ticks = 0;

  while (1) {
    if (rfid_is_card_present()) {
      uint8_t uid[4];
      uint8_t len;
      if (rfid_read_uid(uid, &len)) {
        uint32_t card_uid =
            (uid[0] << 24) | (uid[1] << 16) | (uid[2] << 8) | uid[3];
        uint32_t current_ticks = xTaskGetTickCount();

        // Simple duplicate filtering: don't send the same ID within 3 seconds
        if (card_uid != last_card_uid ||
            (current_ticks - last_scan_ticks) > pdMS_TO_TICKS(3000)) {
          ESP_LOGI(TAG, "Card successfully read in DB: %lu", card_uid);

          system_event_t evt;
          evt.type = EVENT_CARD_SCANNED;
          evt.payload.card_uid = card_uid;

          event_bus_publish(&evt);

          last_card_uid = card_uid;
          last_scan_ticks = current_ticks;
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(50)); // Poll at 20Hz
  }
}

// Spawns the task
void rfid_start_task(void) {
  // PRO_CPU (Core 0) handles real-time IO
  xTaskCreatePinnedToCore(rfid_task_runner, "rfid_task", 4096, NULL,
                          5, // High priority for IO
                          NULL,
                          0 // Core 0
  );
}
