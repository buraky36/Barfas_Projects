#ifndef HAL_IO_H
#define HAL_IO_H

#include <stdint.h>
#include <stdbool.h>
#include "hw_config.h"

typedef enum {
    LED_COLOR_OFF,
    LED_COLOR_RED,
    LED_COLOR_GREEN,
    LED_COLOR_ORANGE,
    LED_COLOR_BLUE,
    LED_COLOR_YELLOW,
    LED_COLOR_WHITE
} led_color_t;

typedef enum {
    LED_MODE_SOLID,
    LED_MODE_BLINK_SLOW,
    LED_MODE_BLINK_FAST
} led_mode_t;

// Pins are now centralized in hw_config.h
#define LED_WS2812_PIN  PIN_WS2812B_RGB
#define BUZZER_PIN      PIN_BUZZER_PWM
#define DIGITAL_INPUT1_PIN PIN_DIGITAL_IN1
#define DIGITAL_INPUT2_PIN PIN_DIGITAL_IN2

void hal_io_init(void);

// Relay
void hal_io_relay_set(bool state);

// Tick
void hal_io_tick(void);

// Sounds
void hal_io_buzzer_beep(uint32_t freq_hz, uint32_t duration_ms, uint32_t count);
void hal_io_buzzer_intro_sweep(void);

// Dig Inputs
bool hal_io_read_door_sensor(void);
bool hal_io_read_exit_button(void);

// LED
void hal_io_led_set(led_color_t color, led_mode_t mode);

// Config
void hal_io_set_sound_enabled(bool enabled);

#endif
