#include "hal_wiegand.h"
#include "hw_config.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "rom/ets_sys.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "HAL_WIEGAND";

// Forward declaration
void hal_shift_reg_set_wiegand_en(bool state);

static bool current_is_output = false;

#define MAX_WG_BITS 512
static uint8_t wg_data_buf[MAX_WG_BITS / 8] = {0};
static uint16_t wg_bit_count = 0;
static int64_t wg_last_bit_time = 0;

static uint8_t wg_ready_data_buf[MAX_WG_BITS / 8] = {0};
static uint16_t wg_ready_bit_count = 0;
static bool wg_available = false;

static void IRAM_ATTR wiegand_d0_isr_handler(void* arg) {
    int64_t now = esp_timer_get_time();
    if (wg_bit_count < MAX_WG_BITS) {
        wg_data_buf[wg_bit_count / 8] &= ~(1 << (7 - (wg_bit_count % 8)));
        wg_bit_count++;
    }
    wg_last_bit_time = now;
}

static void IRAM_ATTR wiegand_d1_isr_handler(void* arg) {
    int64_t now = esp_timer_get_time();
    if (wg_bit_count < MAX_WG_BITS) {
        wg_data_buf[wg_bit_count / 8] |= (1 << (7 - (wg_bit_count % 8)));
        wg_bit_count++;
    }
    wg_last_bit_time = now;
}

void hal_wiegand_init(void) {
    esp_err_t err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to install GPIO ISR service");
    }
    
    hal_wiegand_set_direction(false); // Default: Input Mode
}

void hal_wiegand_set_direction(bool is_output) {
    current_is_output = is_output;
    // Donanım OE (Output Enable) mantığı ile çalışıyorsa, hattı açmak için 1 (true) gerekebilir. 
    // Geçici olarak her iki durumda da (Input/Output) Shift Register pinini TRUE yapıyoruz.
    hal_shift_reg_set_wiegand_en(true);
    
    if (is_output) {
        gpio_isr_handler_remove(PIN_WIEGAND_D0);
        gpio_isr_handler_remove(PIN_WIEGAND_D1);
        
        gpio_set_direction(PIN_WIEGAND_D0, GPIO_MODE_OUTPUT);
        gpio_set_direction(PIN_WIEGAND_D1, GPIO_MODE_OUTPUT);
        gpio_set_level(PIN_WIEGAND_D0, 1);
        gpio_set_level(PIN_WIEGAND_D1, 1);
        ESP_LOGI(TAG, "Wiegand set to OUTPUT mode");
    } else {
        gpio_set_direction(PIN_WIEGAND_D0, GPIO_MODE_INPUT);
        gpio_set_direction(PIN_WIEGAND_D1, GPIO_MODE_INPUT);
        gpio_set_pull_mode(PIN_WIEGAND_D0, GPIO_PULLUP_ONLY);
        gpio_set_pull_mode(PIN_WIEGAND_D1, GPIO_PULLUP_ONLY);
        
        gpio_set_intr_type(PIN_WIEGAND_D0, GPIO_INTR_NEGEDGE);
        gpio_set_intr_type(PIN_WIEGAND_D1, GPIO_INTR_NEGEDGE);
        
        memset(wg_data_buf, 0, sizeof(wg_data_buf));
        wg_bit_count = 0;
        wg_available = false;
        
        gpio_isr_handler_add(PIN_WIEGAND_D0, wiegand_d0_isr_handler, NULL);
        gpio_isr_handler_add(PIN_WIEGAND_D1, wiegand_d1_isr_handler, NULL);
        ESP_LOGI(TAG, "Wiegand set to INPUT mode");
    }
}

static uint8_t calc_even_parity(uint32_t val, uint8_t num_bits) {
    uint8_t p = 0;
    for (int i = 0; i < num_bits; i++) {
        p ^= (val & 1);
        val >>= 1;
    }
    return p;
}

void hal_wiegand_write_raw(uint64_t data, uint8_t bit_len) {
    if (!current_is_output) return;

    for (int i = bit_len - 1; i >= 0; i--) {
        uint8_t bit = (data >> i) & 1;
        if (bit == 0) {
            gpio_set_level(PIN_WIEGAND_D0, 0);
            ets_delay_us(50);
            gpio_set_level(PIN_WIEGAND_D0, 1);
        } else {
            gpio_set_level(PIN_WIEGAND_D1, 0);
            ets_delay_us(50);
            gpio_set_level(PIN_WIEGAND_D1, 1);
        }
        vTaskDelay(pdMS_TO_TICKS(2)); // 2ms gap between bits
    }
}

void hal_wiegand_write(uint32_t data, uint8_t bit_len, bool parity_en) {
    if (!current_is_output) return;

    uint64_t transmit_data = 0;
    uint8_t total_bits = bit_len;
    
    if (parity_en && bit_len == 26) {
        // Data is 24 bits
        uint32_t d24 = data & 0xFFFFFF;
        uint8_t ep = calc_even_parity(d24 >> 12, 12);
        uint8_t op = !calc_even_parity(d24 & 0xFFF, 12);
        
        transmit_data = (uint64_t)ep << 25;
        transmit_data |= (uint64_t)d24 << 1;
        transmit_data |= op;
    } else if (parity_en && bit_len == 34) {
        // Data is 32 bits
        uint32_t d32 = data & 0xFFFFFFFF;
        uint8_t ep = calc_even_parity(d32 >> 16, 16);
        uint8_t op = !calc_even_parity(d32 & 0xFFFF, 16);
        
        transmit_data = (uint64_t)ep << 33;
        transmit_data |= (uint64_t)d32 << 1;
        transmit_data |= op;
    } else {
        // Just send as is (used for 4-bit, 8-bit keypad bursts, or parity disabled)
        transmit_data = data;
    }

    hal_wiegand_write_raw(transmit_data, total_bits);
}

uint16_t hal_wiegand_peek_bit_count(void) {
    if (!wg_available) return 0;
    return wg_ready_bit_count;
}

bool hal_wiegand_available(void) {
    if (!current_is_output && wg_bit_count > 0) {
        // 50ms timeout
        if ((esp_timer_get_time() - wg_last_bit_time) > 50000) { 
            memcpy(wg_ready_data_buf, wg_data_buf, sizeof(wg_data_buf));
            wg_ready_bit_count = wg_bit_count;
            memset(wg_data_buf, 0, sizeof(wg_data_buf));
            wg_bit_count = 0;
            wg_available = true;
            
            if (wg_ready_bit_count <= 64) {
                uint64_t tmp = 0;
                for (int i = 0; i < wg_ready_bit_count; i++) {
                    tmp <<= 1;
                    if (wg_ready_data_buf[i / 8] & (1 << (7 - (i % 8)))) {
                        tmp |= 1;
                    }
                }
                ESP_LOGI(TAG, "Wiegand Input Received: Raw Data = 0x%08llX (Bits: %d)", (unsigned long long)tmp, wg_ready_bit_count);
            } else {
                ESP_LOGI(TAG, "Wiegand Input Received: Long Payload (Bits: %d)", wg_ready_bit_count);
            }
        }
    }
    return wg_available;
}

uint32_t hal_wiegand_read(uint8_t *bits) {
    if (!wg_available) return 0;
    
    uint64_t tmp = 0;
    for (int i = 0; i < wg_ready_bit_count; i++) {
        tmp <<= 1;
        if (wg_ready_data_buf[i / 8] & (1 << (7 - (i % 8)))) {
            tmp |= 1;
        }
    }
    
    uint32_t card_id = 0;
    
    // Auto-strip parity if the length indicates standard formats
    if (wg_ready_bit_count == 26) {
        card_id = (tmp >> 1) & 0xFFFFFF; 
    } else if (wg_ready_bit_count == 34) {
        card_id = (tmp >> 1) & 0xFFFFFFFF;
    } else {
        card_id = tmp & 0xFFFFFFFF;
    }
    
    if (bits) *bits = wg_ready_bit_count;
    wg_available = false;
    
    return card_id;
}

bool hal_wiegand_read_string(char *out_str, size_t max_len, uint16_t *bit_len_out) {
    if (!wg_available) return false;
    
    if (bit_len_out) *bit_len_out = wg_ready_bit_count;
    
    size_t byte_len = wg_ready_bit_count / 8;
    if (byte_len >= max_len) byte_len = max_len - 1;
    
    for (size_t i = 0; i < byte_len; i++) {
        out_str[i] = wg_ready_data_buf[i];
    }
    out_str[byte_len] = '\0';
    
    wg_available = false;
    return true;
}
