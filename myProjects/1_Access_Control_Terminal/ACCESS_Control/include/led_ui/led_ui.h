#ifndef LED_UI_H
#define LED_UI_H

#include "pin_manager.h"
#include "shift_register_map.h"
#include <stdbool.h>
#include <stdint.h>

// Operational constants kept local
#define BUZZER_PWM_CH LEDC_CHANNEL_0
#define BUZZER_PWM_TIMER LEDC_TIMER_0

/**
 * @brief Initialize Shift Registers (74HC595) and Buzzer (PWM)
 */
void led_ui_init(void);

/**
 * @brief Update the state of Shift Register 1 (LEDs 1-8)
 * @param val 8-bit value (1 = ON, 0 = OFF)
 */
void led_ui_set_sr1(uint8_t val);

/**
 * @brief Update the state of Shift Register 2 (LEDs 9-12, Wi-Fi, Red, Green,
 * Relay)
 * @param val 8-bit value (1 = ON, 0 = OFF)
 */
void led_ui_set_sr2(uint8_t val);

/**
 * @brief Set Relay state (connected to SR2 QH)
 * @param enable true to energize relay, false to de-energize
 */
void door_relay_set(bool enable);

/**
 * @brief Turn buzzer on/off
 * @param enable true to sound buzzer, false to stop
 */
void buzzer_set(bool enable);

/**
 * @brief Play a beep sequence using the buzzer
 * @param ms_on duration of each beep and gap between beeps in milliseconds
 * @param count number of beeps to play
 */
void led_ui_beep(uint32_t ms_on, uint8_t count);

#endif // LED_UI_H
