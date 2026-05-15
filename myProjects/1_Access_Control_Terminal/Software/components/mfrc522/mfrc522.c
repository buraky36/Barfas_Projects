#include "mfrc522.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_rom_sys.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define PIN_NUM_MISO 36
#define PIN_NUM_MOSI 5
#define PIN_NUM_CLK 15
#define PIN_NUM_CS 0
// PIN_NUM_RST not connected per user hardware info

static spi_device_handle_t spi;

// MFRC522 Registers
// Page 0:Command and Status
#define Reserved00 0x00
#define CommandReg 0x01
#define CommIEnReg 0x02
#define DivlEnReg 0x03
#define CommIrqReg 0x04
#define DivIrqReg 0x05
#define ErrorReg 0x06
#define Status1Reg 0x07
#define Status2Reg 0x08
#define FIFODataReg 0x09
#define FIFOLevelReg 0x0A
#define WaterLevelReg 0x0B
#define ControlReg 0x0C
#define BitFramingReg 0x0D
#define CollReg 0x0E
#define Reserved01 0x0F
// Page 1:Command
#define Reserved10 0x10
#define ModeReg 0x11
#define TxModeReg 0x12
#define RxModeReg 0x13
#define TxControlReg 0x14
#define TxAutoReg 0x15
#define TxSelReg 0x16
#define RxSelReg 0x17
#define RxThresholdReg 0x18
#define DemodReg 0x19
#define Reserved11 0x1A
#define Reserved12 0x1B
#define MifareReg 0x1C
#define Reserved13 0x1D
#define Reserved14 0x1E
#define SerialSpeedReg 0x1F

// Page 2:CFG
#define Reserved20 0x20
#define CRCResultRegM 0x21
#define CRCResultRegL 0x22
#define Reserved21 0x23
#define ModWidthReg 0x24
#define Reserved22 0x25
#define RFCfgReg 0x26
#define GsNReg 0x27
#define CWGsPReg 0x28
#define ModGsPReg 0x29
#define TModeReg 0x2A
#define TPrescalerReg 0x2B
#define TReloadRegH 0x2C
#define TReloadRegL 0x2D
#define TCounterValueRegH 0x2E
#define TCounterValueRegL 0x2F
// Page 3:TestRegister
#define Reserved30 0x30
#define TestSel1Reg 0x31
#define TestSel2Reg 0x32
#define TestPinEnReg 0x33
#define TestPinValueReg 0x34
#define TestBusReg 0x35
#define AutoTestReg 0x36
#define VersionReg 0x37
#define AnalogTestReg 0x38
#define TestDAC1Reg 0x39
#define TestDAC2Reg 0x3A
#define TestADCReg 0x3B
#define Reserved31 0x3C
#define Reserved32 0x3D
#define Reserved33 0x3E
#define Reserved34 0x3F

// MFRC522 Commands
#define PCD_IDLE 0x00       // NO action; Cancel the current command
#define PCD_AUTHENT 0x0E    // Authentication Key
#define PCD_RECEIVE 0x08    // Receive Data
#define PCD_TRANSMIT 0x04   // Transmit data
#define PCD_TRANSCEIVE 0x0C // Transmit and receive data,
#define PCD_RESETPHASE 0x0F // Reset
#define PCD_CALCCRC 0x03    // CRC Calculate

// Mifare_One card command word
#define PICC_REQIDL 0x26    // find the antenna area does not enter hibernation
#define PICC_REQALL 0x52    // find all the cards antenna area
#define PICC_ANTICOLL 0x93  // anti-collision
#define PICC_SElECTTAG 0x93 // election card
#define PICC_AUTHENT1A 0x60 // authentication key A
#define PICC_AUTHENT1B 0x61 // authentication key B
#define PICC_READ 0x30      // Read Block
#define PICC_WRITE 0xA0     // write block
#define PICC_DECREMENT 0xC0 // debit
#define PICC_INCREMENT 0xC1 // recharge
#define PICC_RESTORE 0xC2   // transfer block data to the buffer
#define PICC_TRANSFER 0xB0  // save the data in the buffer
#define PICC_HALT 0x50      // Sleep

static void write_reg(uint8_t reg, uint8_t val) {
  spi_transaction_t t;
  memset(&t, 0, sizeof(t));
  t.flags = SPI_TRANS_USE_TXDATA;
  t.length = 16;
  t.tx_data[0] = (reg << 1) & 0x7E;
  t.tx_data[1] = val;
  spi_device_polling_transmit(spi, &t);
}

static uint8_t read_reg(uint8_t reg) {
  spi_transaction_t t;
  memset(&t, 0, sizeof(t));
  t.flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA;
  t.length = 16;
  t.tx_data[0] = ((reg << 1) & 0x7E) | 0x80;
  t.tx_data[1] = 0;
  spi_device_polling_transmit(spi, &t);
  return t.rx_data[1];
}

static void set_bit_mask(uint8_t reg, uint8_t mask) {
  uint8_t tmp = read_reg(reg);
  write_reg(reg, tmp | mask);
}

static void clear_bit_mask(uint8_t reg, uint8_t mask) {
  uint8_t tmp = read_reg(reg);
  write_reg(reg, tmp & (~mask));
}

bool mfrc522_init(void) {
  spi_bus_config_t buscfg = {.miso_io_num = PIN_NUM_MISO,
                             .mosi_io_num = PIN_NUM_MOSI,
                             .sclk_io_num = PIN_NUM_CLK,
                             .quadwp_io_num = -1,
                             .quadhd_io_num = -1,
                             .max_transfer_sz = 32};
  spi_device_interface_config_t devcfg = {
      .clock_speed_hz = 1 * 1000 * 1000, 
      .mode = 0,
      .spics_io_num = PIN_NUM_CS,
      .queue_size = 1,
  };
  // Strengthen GPIO0 (CS pin) against its internal pull-down
  gpio_set_drive_capability(PIN_NUM_CS, GPIO_DRIVE_CAP_3);
  spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
  spi_bus_add_device(SPI2_HOST, &devcfg, &spi);
  gpio_set_drive_capability(PIN_NUM_CS, GPIO_DRIVE_CAP_3); // re-apply after SPI driver init

  // MFRC522 Soft Reset
  write_reg(CommandReg, PCD_RESETPHASE);
  vTaskDelay(pdMS_TO_TICKS(50));

  // Communication Check: Read Version Register
  uint8_t version = read_reg(VersionReg);
  printf("[MFRC522] SPI Init. Version Register: 0x%02X\n", version);
  
  if (version == 0x00 || version == 0xFF) {
      printf("[MFRC522] Hardware not found! Reverting to PIN+QR mode.\n");
      // Optional: de-init SPI bus here if we want to free it, but it might be shared
      return false; 
  }

  // Timer settings
  write_reg(TModeReg,
            0x80); // TAuto=1: timer starts automatically at end of transmission
  write_reg(TPrescalerReg, 0xA9); // f_timer = 13.56 MHz / (2*TPrescaler+1)
  write_reg(TReloadRegH, 0x03);   // Timer reload value high byte
  write_reg(TReloadRegL, 0xE8); // Timer reload value low byte -> ~30ms timeout
  write_reg(TxAutoReg, 0x40);   // Force 100% ASK modulation
  write_reg(ModeReg, 0x3D);     // CRC Initial value 0x6363

  // Antenna configuration
  write_reg(RFCfgReg, 0x70); // Max antenna gain (48dB)

  // Antenna ON
  if (!(read_reg(TxControlReg) & 0x03)) {
    set_bit_mask(TxControlReg, 0x03);
  }
  // Verify key registers after init (printed once at boot)
  printf(
      "[MFRC522 DBG] TxControlReg=0x%02X (expect 0x8x or 0x03 in bits[1:0])\n",
      read_reg(TxControlReg));
  printf("[MFRC522 DBG] TxAutoReg    =0x%02X (expect 0x40)\n",
         read_reg(TxAutoReg));
  printf("[MFRC522 DBG] ModeReg     =0x%02X (expect 0x3D)\n",
         read_reg(ModeReg));
  printf("[MFRC522 DBG] TModeReg    =0x%02X (expect 0x80)\n",
         read_reg(TModeReg));
  
  return true;
}

static int to_card(uint8_t command, uint8_t *send_data, uint8_t send_len,
                   uint8_t *back_data, uint32_t *back_len) {
  int status = -1;
  uint8_t irqEn = 0x77;
  uint8_t waitIRq = 0x30;

  // match reference project (MFRC522_ToCard in Smart_Lock_TFC) exactly
  write_reg(CommIEnReg, irqEn | 0x80);
  clear_bit_mask(CommIrqReg, 0x80);
  write_reg(FIFOLevelReg, 0x80); // Flush
  write_reg(CommandReg, PCD_IDLE);

  for (int i = 0; i < send_len; i++)
    write_reg(FIFODataReg, send_data[i]);

  write_reg(CommandReg, command);

  if (command == PCD_TRANSCEIVE)
    set_bit_mask(BitFramingReg, 0x80); // StartSend

  uint32_t i = 5000;
  uint8_t n = 0;
  while (1) {
    n = read_reg(CommIrqReg);
    i--;
    esp_rom_delay_us(5);
    if ((i == 0) || (n & 0x01) || (n & waitIRq))
      break;
  }
  clear_bit_mask(BitFramingReg, 0x80);

  uint8_t errReg = read_reg(ErrorReg);

  if (i == 0) status = -1; // Timeout

  if (i != 0 && !(errReg & 0x1B)) {
    status = 0;
    if (read_reg(CommIrqReg) & 0x01) status = -1; // Timer IRQ = Timeout
    if (command == PCD_TRANSCEIVE) {
      n = read_reg(FIFOLevelReg);
      uint8_t lastBits = read_reg(ControlReg) & 0x07;
      *back_len = (lastBits) ? ((n - 1) * 8 + lastBits) : (n * 8);
      
      if (n == 0) n = 1;
      if (n > 16) n = 16;
      for (i = 0; i < n; i++)
        back_data[i] = read_reg(FIFODataReg);
    }
  }
  return status;
}

static uint32_t simulated_uid = 0;

void mfrc522_inject(uint32_t uid) {
    simulated_uid = uid;
}

bool mfrc522_check_card(uint32_t *uid) {
  if (simulated_uid != 0) {
      if (uid) *uid = simulated_uid;
      simulated_uid = 0;
      return true;
  }

  uint8_t reqMode = PICC_REQIDL;
  uint8_t backData[16];
  uint32_t backLen = 0;

  // Static for duplicate filtering
  static uint32_t last_uid = 0;
  static uint32_t last_scan_ticks = 0;
  uint32_t now_ticks = xTaskGetTickCount();

  // Step 1: REQA - Request card (7-bit short frame)
  write_reg(BitFramingReg, 0x07); // Send 7 bits
  int status = to_card(PCD_TRANSCEIVE, &reqMode, 1, backData, &backLen);

  if (status != 0 || backLen != 0x10) {
    return false;
  }

  // Step 2: Anti-collision loop to get UID
  uint8_t antiCollCmd[2] = {PICC_ANTICOLL, 0x20}; 
  write_reg(BitFramingReg, 0x00);
  status = to_card(PCD_TRANSCEIVE, antiCollCmd, 2, backData, &backLen);

  if (status == 0) {
    uint8_t bcc = backData[0] ^ backData[1] ^ backData[2] ^ backData[3];
    if (bcc != backData[4] || backLen != 40) {
        return false; // Checksum failed or incomplete data
    }
    
    uint32_t current_uid = ((uint32_t)backData[0] << 24) | ((uint32_t)backData[1] << 16) |
                           ((uint32_t)backData[2] << 8) | (uint32_t)backData[3];

    // Filter duplicates: if same card, wait 1 second before reporting again
    // 1000ms cooldown
    if (current_uid == last_uid && (now_ticks - last_scan_ticks < pdMS_TO_TICKS(1000))) {
        return false;
    }

    last_uid = current_uid;
    last_scan_ticks = now_ticks;

    if (uid) *uid = current_uid;

    // Step 3: Halt the card so it doesn't immediately respond to the next request
    uint8_t haltCmd[4] = {PICC_HALT, 0, 0, 0};
    // Calculate CRC or use raw Halt if card supports it
    // For simplicity, we just send the command; many cards will just sleep.
    to_card(PCD_TRANSCEIVE, haltCmd, 2, backData, &backLen);

    return true;
  }

  return false;
}
