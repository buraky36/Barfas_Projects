#include "hal_io.h"
#include "hal_shift_reg.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "led_strip.h"
#include "driver/ledc.h"

static led_color_t current_led_color = LED_COLOR_OFF;
static led_mode_t current_led_mode = LED_MODE_SOLID;

static bool buzzer_active = false;
static uint32_t buzzer_freq = 0;
static uint32_t buzzer_sequence_count = 0;
static uint32_t buzzer_duration = 0;
static uint32_t buzzer_interval = 0;
static int64_t buzzer_next_event_ms = 0;
static bool sound_enabled = true;

static led_strip_handle_t led_strip;

#define RGB_MAX 40 // Adjustable brightness (0-255). 40 is about 15% brightness, ideal for indoor keypads.

static void apply_led_color(led_color_t c) {
    if (c == LED_COLOR_OFF) {
        led_strip_set_pixel(led_strip, 0, 0, 0, 0);
    } else if (c == LED_COLOR_RED) {
        led_strip_set_pixel(led_strip, 0, RGB_MAX, 0, 0);
    } else if (c == LED_COLOR_GREEN) {
        led_strip_set_pixel(led_strip, 0, 0, RGB_MAX, 0);
    } else if (c == LED_COLOR_ORANGE) {
        led_strip_set_pixel(led_strip, 0, RGB_MAX, RGB_MAX/2, 0);
    } else if (c == LED_COLOR_BLUE) {
        led_strip_set_pixel(led_strip, 0, 0, 0, RGB_MAX);
    } else if (c == LED_COLOR_YELLOW) {
        led_strip_set_pixel(led_strip, 0, RGB_MAX, RGB_MAX, 0);
    } else if (c == LED_COLOR_WHITE) {
        led_strip_set_pixel(led_strip, 0, RGB_MAX, RGB_MAX, RGB_MAX);
    }
    led_strip_refresh(led_strip);
}

void hal_io_init(void) {
    // Inputs (DigIn1 and DigIn2)
    gpio_config_t in_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL<<DIGITAL_INPUT1_PIN) | (1ULL<<DIGITAL_INPUT2_PIN),
        .pull_down_en = 1,
        .pull_up_en = 0
    };
    gpio_config(&in_conf);

    // WS2812B RGB LED Init
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_WS2812_PIN,
        .max_leds = 1, // at least one LED on board
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_RGB,
        .led_model = LED_MODEL_WS2812,
        .flags.invert_out = false,
    };
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
        .flags.with_dma = false,
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    led_strip_clear(led_strip);

    // Buzzer PWM Init
    gpio_reset_pin(BUZZER_PIN);
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .timer_num        = LEDC_TIMER_0,
        .duty_resolution  = LEDC_TIMER_10_BIT,
        .freq_hz          = 2000,  
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = LEDC_CHANNEL_0,
        .timer_sel      = LEDC_TIMER_0,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = BUZZER_PIN,
        .duty           = 0, // off
        .hpoint         = 0
    };
    ledc_channel_config(&ledc_channel);
}

void hal_io_tick(void) {
    int64_t now = esp_timer_get_time() / 1000;
    
    // LED blink logic
    if (current_led_mode == LED_MODE_SOLID) {
        apply_led_color(current_led_color);
    } else {
        int blink_period = (current_led_mode == LED_MODE_BLINK_FAST) ? 100 : 500;
        if ((now / blink_period) % 2 == 0) {
            apply_led_color(current_led_color);
        } else {
            apply_led_color(LED_COLOR_OFF);
        }
    }
    
    // Buzzer logic
    if (buzzer_sequence_count > 0 && now >= buzzer_next_event_ms) {
        buzzer_sequence_count--;
        if (buzzer_active) {
            ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
            ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);

            buzzer_active = false;
            buzzer_next_event_ms = now + buzzer_interval;
        } else {
            ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_0, buzzer_freq);
            ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 512); // 50% duty
            ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
            buzzer_active = true;
            buzzer_next_event_ms = now + buzzer_duration;
        }
    } else if (buzzer_sequence_count == 0 && buzzer_active) {
        buzzer_active = false;
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    }
}

void hal_io_relay_set(bool state) {
    hal_shift_reg_set_relay(state);
}

void hal_io_buzzer_beep(uint32_t freq_hz, uint32_t duration_ms, uint32_t count) {
    if (count == 0 || !sound_enabled) return;
    buzzer_freq = freq_hz;
    buzzer_duration = duration_ms;
    buzzer_interval = duration_ms;
    buzzer_sequence_count = count * 2 - 1; // Transitions (ON OFF ON OFF ON)
    buzzer_next_event_ms = (esp_timer_get_time() / 1000) + buzzer_duration;
    
    ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_0, buzzer_freq);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 512);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    buzzer_active = true;
}

void hal_io_buzzer_intro_sweep(void) {
    if (!sound_enabled) return;

    // Drive buzzer PWM pin at 2.7kHz, 100% duty for 2 seconds
    ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_0, 2700);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 1023);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    vTaskDelay(pdMS_TO_TICKS(2000));
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

bool hal_io_read_door_sensor(void) {
    static uint32_t active_count = 0;
    static bool is_active = false;

    if (gpio_get_level(DIGITAL_INPUT2_PIN) == 1) {
        if (active_count < 10) active_count++;
        if (active_count >= 5) is_active = true;
    } else {
        active_count = 0;
        is_active = false;
    }
    return is_active;
}

bool hal_io_read_exit_button(void) {
    static uint32_t press_count = 0;
    static bool is_pressed = false;

    if (gpio_get_level(DIGITAL_INPUT1_PIN) == 1) {
        if (!is_pressed) {
            press_count++;
            if (press_count >= 5) { // Approx 50ms debounce
                is_pressed = true;
                return true; // Edge trigger: return true only once per press
            }
        }
    } else {
        press_count = 0;
        is_pressed = false;
    }
    return false;
}


void hal_io_led_set(led_color_t color, led_mode_t mode) {
    current_led_color = color;
    current_led_mode = mode;
}

void hal_io_set_sound_enabled(bool enabled) {
    sound_enabled = enabled;
}
