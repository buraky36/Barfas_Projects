#include "hal_rfid.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "HAL_RFID";
static spi_device_handle_t spi_handle;

void hal_rfid_init(void) {
  esp_err_t ret;

  // SPI Bus config
  spi_bus_config_t buscfg = {.miso_io_num = RFID_MISO_PIN,
                             .mosi_io_num = RFID_MOSI_PIN,
                             .sclk_io_num = RFID_CLK_PIN,
                             .quadwp_io_num = -1,
                             .quadhd_io_num = -1,
                             .max_transfer_sz = 32};

  // SPI Device config
  spi_device_interface_config_t devcfg = {
      .clock_speed_hz = 5 * 1000 * 1000, // 5 MHz
      .mode = 0,                         // SPI mode 0
      .spics_io_num = RFID_CS_PIN,
      .queue_size = 7,
  };

  // Initialize the SPI bus (HSPI_HOST or SPI2_HOST)
  ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
  ESP_ERROR_CHECK(ret);

  // Attach the MFRC522 to the SPI bus
  ret = spi_bus_add_device(SPI2_HOST, &devcfg, &spi_handle);
  ESP_ERROR_CHECK(ret);

  // Setup IRQ Pin
  gpio_config_t irq_conf = {.intr_type = GPIO_INTR_DISABLE,
                            .mode = GPIO_MODE_INPUT,
                            .pin_bit_mask = (1ULL << RFID_IRQ_PIN),
                            .pull_down_en = 0,
                            .pull_up_en = 1};
  gpio_config(&irq_conf);

  ESP_LOGI(TAG, "RFID SPI Initialized.");
}

bool hal_rfid_has_interrupt(void) {
  // MFRC522 IRQ pin is active LOW
  return gpio_get_level(RFID_IRQ_PIN) == 0;
}

bool hal_rfid_check_card(uint8_t *uid_out, uint8_t *uid_len_out) {
  // TODO: Implement MFRC522 specific commands using spi_device_transmit
  // 1. Check if card is present (PICC_REQIDL)
  // 2. Anti-collision (PICC_ANTICOLL)
  // 3. Select card, get UID
  return false;
}
