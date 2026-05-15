#ifndef HAL_SHIFT_REG_H
#define HAL_SHIFT_REG_H

#include <stdint.h>
#include <stdbool.h>
#include "hw_config.h"

// Pins
#define SHIFT_REG_CLK_PIN      PIN_SHIFT_CLK
#define SHIFT_REG_DATA_PIN     PIN_SHIFT_DATA
#define SHIFT_REG_LATCH1_PIN   PIN_SHIFT_LATCH1
#define SHIFT_REG_LATCH2_PIN   PIN_SHIFT_LATCH2

// The 74HC595 mappings depend on which shift register they are in.
// U1 (First Register):
// QA=LED_EN_9, QB=LED_EN_1, QC=LED_EN_4, QD=TSM_RST, QE=LED_EN_5, QF=LED_EN_6, QG=LED_EN_3, QH=LED_EN_2
// U2 (Second Register):
// QA=LED_EN_8, QB=LED_EN_10, QC=LED_EN_7, QD=LED_EN_11, QE=LED_EN_12, QF=Wiegand_OUT_EN, QG=RFID_RST, QH=RELAY-1

#define MASK_U1_TSM_RST      (1 << 3) // U1 QD

#define MASK_U2_WIEGAND_EN   (1 << 5) // U2 QF
#define MASK_U2_RFID_RST     (1 << 6) // U2 QG
#define MASK_U2_RELAY        (1 << 7) // U2 QH

typedef enum {
    ANIM_NONE = 0,
    ANIM_KEY_BLINK,
    ANIM_SUCCESS_TICK,
    ANIM_MASTER_WAVE,
    ANIM_ERROR_FLASH,
    ANIM_FACTORY_RESET,
    ANIM_IDLE_SNAKE,    // Screensaver: snake slide 1→2→3→6→5→4→7→8→9→#→0→* and back
    ANIM_IDLE_WAVE      // Screensaver: sweeping wave across keypad
} shift_reg_anim_t;

void hal_shift_reg_init(void);
void hal_shift_reg_tick(void);

// Play an animation continuously (non-blocking)
void hal_shift_reg_play_anim(shift_reg_anim_t anim, uint32_t arg);

// Direct access
void hal_shift_reg_write(uint8_t reg1_data, uint8_t reg2_data);
void hal_shift_reg_set_rfid_rst(bool state);
void hal_shift_reg_set_tsm_rst(bool state);
void hal_shift_reg_set_relay(bool state);
void hal_shift_reg_set_wiegand_en(bool state);
void hal_shift_reg_set_led(uint8_t led_index, bool state); // 1-12
void hal_shift_reg_set_touch_mask(uint16_t mask);
bool hal_shift_reg_is_keypad_active(void);        // True while 20s LED activity window is running
bool hal_shift_reg_consume_wakeup_touch(void);    // Returns true ONCE for the screensaver-wakeup touch
bool hal_shift_reg_is_blocking_anim_playing(void); // True if an animation is playing that should block touches
void hal_shift_reg_wakeup_keypad(void);           // Wakes up keypad from idle/screensaver

#endif
