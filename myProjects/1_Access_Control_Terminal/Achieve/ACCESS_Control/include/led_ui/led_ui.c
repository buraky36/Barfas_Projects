#include "led_ui.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "rom/ets_sys.h" // For ets_delay_us

static const char *TAG = "LED_UI";

// Store current state to allow bitwise modifications (like relay)
static uint8_t sr1_state = 0x00;
static uint8_t sr2_state = 0x00;

static void shift_out(uint8_t data, int latch_pin) {
  // Basic bit-banging shift out (LSB or MSB first depending on wiring, assuming
  // MSB first)
  gpio_set_level(latch_pin, 0);
  for (int i = 7; i >= 0; i--) {
    gpio_set_level(PIN_SR_CLK, 0);
    gpio_set_level(PIN_SR_DATA, (data >> i) & 0x01);
    ets_delay_us(1); // Small delay for stability
    gpio_set_level(PIN_SR_CLK, 1);
    ets_delay_us(1);
  }
  gpio_set_level(latch_pin, 1);
}

void led_ui_init(void) {
  ESP_LOGI(TAG, "Initializing 74HC595 Shift Registers and Buzzer...");

  // Configure Shift Register GPIOs
  gpio_config_t io_conf = {
      .intr_type = GPIO_INTR_DISABLE,
      .mode = GPIO_MODE_OUTPUT,
      .pin_bit_mask = (1ULL << PIN_SR_CLK) | (1ULL << PIN_SR_DATA) |
                      (1ULL << PIN_SR_LATCH1) | (1ULL << PIN_SR_LATCH2),
      .pull_down_en = 0,
      .pull_up_en = 0};
  gpio_config(&io_conf);

  // Initial state: all low
  gpio_set_level(PIN_SR_CLK, 0);
  gpio_set_level(PIN_SR_DATA, 0);
  gpio_set_level(PIN_SR_LATCH1, 1);
  gpio_set_level(PIN_SR_LATCH2, 1);

  // Clear shift registers
  led_ui_set_sr1(0x00);
  led_ui_set_sr2(0x00);

  // Configure Buzzer PWM
  ledc_timer_config_t ledc_timer = {.speed_mode = LEDC_LOW_SPEED_MODE,
                                    .timer_num = BUZZER_PWM_TIMER,
                                    .duty_resolution = LEDC_TIMER_13_BIT,
                                    .freq_hz =
                                        2000, // 2kHz common buzzer frequency
                                    .clk_cfg = LEDC_AUTO_CLK};
  ledc_timer_config(&ledc_timer);

  ledc_channel_config_t ledc_channel = {.speed_mode = LEDC_LOW_SPEED_MODE,
                                        .channel = BUZZER_PWM_CH,
                                        .timer_sel = BUZZER_PWM_TIMER,
                                        .intr_type = LEDC_INTR_DISABLE,
                                        .gpio_num = PIN_BUZZER_PWM,
                                        .duty = 0, // Initially off
                                        .hpoint = 0};
  ledc_channel_config(&ledc_channel);

  ESP_LOGI(TAG, "LED IO and Buzzer initialized successfully.");
}

void led_ui_set_sr1(uint8_t val) {
  sr1_state = val;
  shift_out(sr1_state, PIN_SR_LATCH1);
}

void led_ui_set_sr2(uint8_t val) {
  sr2_state = val;
  shift_out(sr2_state, PIN_SR_LATCH2);
}

void door_relay_set(bool enable) {
  // Relay is on SR2, QH (Bit 7 if MSB first, or Bit 0 depending on cascade)
  // Assuming QH is highest bit (0x80)
  if (enable) {
    sr2_state |= SR2_RELAY_EN;
  } else {
    sr2_state &= ~SR2_RELAY_EN;
  }
  led_ui_set_sr2(sr2_state);
  ESP_LOGI(TAG, "Door Relay %s", enable ? "ENERGIZED" : "DE-ENERGIZED");
}

void buzzer_set(bool enable) {
  if (enable) {
    // 50% duty cycle for 13-bit resolution (8192)
    ledc_set_duty(LEDC_LOW_SPEED_MODE, BUZZER_PWM_CH, 4096);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, BUZZER_PWM_CH);
  } else {
    ledc_set_duty(LEDC_LOW_SPEED_MODE, BUZZER_PWM_CH, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, BUZZER_PWM_CH);
  }
}

void led_ui_beep(uint32_t ms_on, uint8_t count) {
  for (int i = 0; i < count; i++) {
    buzzer_set(true);
    vTaskDelay(pdMS_TO_TICKS(ms_on));
    buzzer_set(false);
    if (i < count - 1) {
      vTaskDelay(pdMS_TO_TICKS(ms_on));
    }
  }
}

