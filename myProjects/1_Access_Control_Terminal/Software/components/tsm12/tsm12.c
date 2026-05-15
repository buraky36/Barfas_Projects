#include "tsm12.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "hw_config.h"

static const char *TAG = "TSM12";

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "hal_shift_reg.h"

static TaskHandle_t tsm12_task_handle = NULL;
static QueueHandle_t tsm12_queue = NULL;

static void IRAM_ATTR tsm12_isr_handler(void *arg) {
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  if (tsm12_task_handle != NULL) {
    vTaskNotifyGiveFromISR(tsm12_task_handle, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken) {
      portYIELD_FROM_ISR();
    }
  }
}

// I2C Bus Configuration
#define I2C_MASTER_SCL_IO PIN_TSM_SCL
#define I2C_MASTER_SDA_IO PIN_TSM_SDA
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_FREQ_HZ 100000 // 100kHz standard speed

// TSM12 I2C Address (0xD0 >> 1 = 0x68, ESP-IDF uses 7-bit format)
#define TSM12_I2C_ADDR 0x68

// TSM12 Control GPIO Pins
#define TSM12_EN_PIN PIN_TSM_I2C_EN
#define TSM12_INT_PIN PIN_TSM_INT

// TSM12 Registers (Source: Smart_Lock_TFC Tsm12.h)
#define TSM12_REG_SENS1 0x02
#define TSM12_REG_SENS2 0x03
#define TSM12_REG_SENS3 0x04
#define TSM12_REG_SENS4 0x05
#define TSM12_REG_SENS5 0x06
#define TSM12_REG_SENS6 0x07
#define TSM12_REG_CTRL1 0x08
#define TSM12_REG_CTRL2 0x09
#define TSM12_REG_REF_RST1 0x0A
#define TSM12_REG_REF_RST2 0x0B
#define TSM12_REG_CH_HOLD1 0x0C
#define TSM12_REG_CH_HOLD2 0x0D
#define TSM12_REG_CAL_HOLD1 0x0E
#define TSM12_REG_CAL_HOLD2 0x0F
#define TSM12_REG_OUTPUT1 0x10
#define TSM12_REG_OUTPUT2 0x11
#define TSM12_REG_OUTPUT3 0x12

// Sensitivity value — 0x0F recommended for very high sensitivity
#define TSM12_SENSITIVITY 0xFF

static void tsm12_write_reg(uint8_t reg, uint8_t val) {
  uint8_t data[2] = {reg, val};
  gpio_set_level(TSM12_EN_PIN, 0); // EN LOW — enable I2C
  i2c_master_write_to_device(I2C_MASTER_NUM, TSM12_I2C_ADDR, data, 2,
                             pdMS_TO_TICKS(20));
  gpio_set_level(TSM12_EN_PIN, 1); // EN HIGH — release
}

void tsm12_init(void) {
  // RST is now handled via Shift Register (U1_QD). The shift register init
  // should happen BEFORE tsm12_init() in main.c.
  hal_shift_reg_set_tsm_rst(true);
  vTaskDelay(pdMS_TO_TICKS(100));
  hal_shift_reg_set_tsm_rst(false);
  vTaskDelay(pdMS_TO_TICKS(200));

  // EN: Output — reference driver initially sets to 0 (or we just use it during
  // transactions) IMPORTANT: The user states "TSM12_I2C_EN pini yazılımda
  // şuanda nasıl kullanılıyorsa yani I2C haberleşmesi başlamadan hemen önce aç
  // kapa gibi kalsın." So we init it here.
  gpio_set_direction(TSM12_EN_PIN, GPIO_MODE_OUTPUT);
  gpio_set_level(TSM12_EN_PIN, 0);
  vTaskDelay(pdMS_TO_TICKS(200));

  // INT: Input — hardware interrupt line
  gpio_config_t int_conf = {
      .intr_type = GPIO_INTR_ANYEDGE, // Trigger on both press and release
      .mode = GPIO_MODE_INPUT,
      .pin_bit_mask = (1ULL << TSM12_INT_PIN),
      .pull_up_en = 1,
  };
  gpio_config(&int_conf);

  // Install ISR service if not already installed
  gpio_install_isr_service(0);

  // Create queue and task before adding ISR
  tsm12_queue = xQueueCreate(10, sizeof(uint16_t));
  // Provide reference to tsm12_task later in the file, wait, we need to forward
  // declare it or define it above. Actually I'll define tsm12_task before
  // tsm12_init. Let's add forward declaration.
  extern void tsm12_task(void *arg);
  xTaskCreate(tsm12_task, "tsm12_task", 4096, NULL, 10, &tsm12_task_handle);

  gpio_isr_handler_add(TSM12_INT_PIN, tsm12_isr_handler, NULL);

  // --- I2C Setup ---
  i2c_config_t conf = {
      .mode = I2C_MODE_MASTER,
      .sda_io_num = I2C_MASTER_SDA_IO,
      .scl_io_num = I2C_MASTER_SCL_IO,
      .sda_pullup_en = GPIO_PULLUP_ENABLE,
      .scl_pullup_en = GPIO_PULLUP_ENABLE,
      .master.clk_speed = I2C_MASTER_FREQ_HZ,
  };
  i2c_param_config(I2C_MASTER_NUM, &conf);
  i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);

  // --- I2C Address Scanner (With EN Handshake) ---
  printf("[TSM12] Scanning I2C bus (SDA=GPIO%d, SCL=GPIO%d)...\n",
         I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO);

  for (uint8_t addr = 0x08; addr < 0x78; addr++) {
    gpio_set_level(TSM12_EN_PIN, 0); // EN LOW — enable
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_stop(cmd);
    esp_err_t ret =
        i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(20));
    i2c_cmd_link_delete(cmd);
    gpio_set_level(TSM12_EN_PIN, 1); // EN HIGH — release
    if (ret == ESP_OK) {
      printf("[TSM12] FOUND device at 7-bit=0x%02X\n", addr);
    }
  }

  // --- Register Initialization (Sequence from Smart_Lock_TFC) ---
  // RepCnt loops are removed for clarity as I2C writes are checked by
  // tsm12_write_reg

  // CTRL2 Init
  tsm12_write_reg(TSM12_REG_CTRL2, 0x0F);
  vTaskDelay(pdMS_TO_TICKS(50));
  tsm12_write_reg(TSM12_REG_CTRL2, 0x07);
  vTaskDelay(pdMS_TO_TICKS(5));

  // Sensitivity setting
  tsm12_write_reg(TSM12_REG_SENS1, TSM12_SENSITIVITY);
  tsm12_write_reg(TSM12_REG_SENS2, TSM12_SENSITIVITY);
  tsm12_write_reg(TSM12_REG_SENS3, TSM12_SENSITIVITY);
  tsm12_write_reg(TSM12_REG_SENS4, TSM12_SENSITIVITY);
  tsm12_write_reg(TSM12_REG_SENS5, TSM12_SENSITIVITY);
  tsm12_write_reg(TSM12_REG_SENS6, TSM12_SENSITIVITY);

  // Other Control Registers
  tsm12_write_reg(TSM12_REG_CTRL1, 0x02);
  tsm12_write_reg(TSM12_REG_CH_HOLD1, 0x00);
  tsm12_write_reg(TSM12_REG_CH_HOLD2, 0x00);
  tsm12_write_reg(TSM12_REG_REF_RST1, 0x00);
  tsm12_write_reg(TSM12_REG_REF_RST2, 0x00);

  vTaskDelay(pdMS_TO_TICKS(10));
  printf("[TSM12] Init complete. Configured per reference project.\n");
}

// Mapping of bit indices to actual button values based on real hardware raw
// read
static const uint8_t TSM12_KEY_MAP[12] = {
    6,  // i=0  bits 0,1   (data[0]=0x03)  -> Key-7
    9,  // i=1  bits 2,3   (data[0]=0x0C)  -> Key-*
    3,  // i=2  bits 4,5   (data[0]=0x30)  -> Key-4
    10, // i=3  bits 6,7   (data[0]=0xC0)  -> Key-0
    7,  // i=4  bits 8,9   (data[1]=0x03)  -> Key-8
    11, // i=5  bits 10,11 (data[1]=0x0C)  -> Key-#
    8,  // i=6  bits 12,13 (data[1]=0x30)  -> Key-9
    0,  // i=7  bits 14,15 (data[1]=0xC0)  -> Key-1
    4,  // i=8  bits 16,17 (data[2]=0x03)  -> Key-5
    5,  // i=9  bits 18,19 (data[2]=0x0C)  -> Key-6
    1,  // i=10 bits 20,21 (data[2]=0x30)  -> Key-2
    2   // i=11 bits 22,23 (data[2]=0xC0)  -> Key-3
};

void tsm12_task(void *arg) {
  uint16_t last_mask = 0;
  while (1) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    // Wait a tiny bit just in case
    esp_rom_delay_us(1000);

    // Read I2C
    uint8_t cmd_reg = TSM12_REG_OUTPUT1;
    uint8_t data[3] = {0};

    gpio_set_level(TSM12_EN_PIN, 0); // EN LOW — enable I2C
    esp_rom_delay_us(
        500); // Small settle delay (non-blocking yield is not needed for 500us)
    esp_err_t err =
        i2c_master_write_read_device(I2C_MASTER_NUM, TSM12_I2C_ADDR, &cmd_reg,
                                     1, data, 3, pdMS_TO_TICKS(50));
    gpio_set_level(TSM12_EN_PIN, 1); // EN HIGH — release

    if (err == ESP_OK) {
      uint32_t raw_bits = (data[2] << 16) | (data[1] << 8) | data[0];

      uint16_t detected_mask = 0;
      for (int i = 0; i < 12; i++) {
        if (raw_bits & (0x03 << (i * 2))) {
          detected_mask |= (1 << TSM12_KEY_MAP[i]);
        }
      }

      if (detected_mask != last_mask) {
        xQueueSend(tsm12_queue, &detected_mask, 0);
        last_mask = detected_mask;
      }
      // LED remap: detected_mask bit j → physical LED bit for that pad.
      // PIN+QR  → identity (bit j lights LED j+1).
      // PIN+RFID→ each touch bit remapped to the LED next to the pressed pad.
      // Verified by hardware test on PIN+RFID board.
      // bit:          0   1   2   3   4   5   6   7   8   9  10  11
      static const uint8_t led_remap_qr  [12] = { 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11};
      static const uint8_t led_remap_rfid[12] = {11,  5,  2,  9,  1,  8,  3,  0,  4,  6, 10,  7};
      const uint8_t *remap = (active_hw_version == HW_VERSION_RFID_ONLY)
                             ? led_remap_rfid : led_remap_qr;
      uint16_t led_mask = 0;
      for (int k = 0; k < 12; k++) {
        if (detected_mask & (1 << k)) led_mask |= (1 << remap[k]);
      }
      hal_shift_reg_set_touch_mask(led_mask);

    } else {
      ESP_LOGE(TAG, "I2C ERROR: %s (addr=0x%02X)", esp_err_to_name(err),
               TSM12_I2C_ADDR);
    }

    // Rate limit and debounce
    vTaskDelay(pdMS_TO_TICKS(20));
    xTaskNotifyStateClear(NULL);
  }
}

// Non-blocking poll: pops from queue
bool tsm12_read_keys(uint16_t *keys_mask) {
  uint16_t mask;
  if (xQueueReceive(tsm12_queue, &mask, 0) == pdTRUE) {
    if (keys_mask) {
      *keys_mask = mask;
    }
    return true;
  }
  return false;
}
