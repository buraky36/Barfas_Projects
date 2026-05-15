#include "hal_shift_reg.h"
#include "driver/gpio.h"
#include "esp_timer.h" // For non-blocking timers
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "rom/ets_sys.h" // For setup time micro-delays

static SemaphoreHandle_t shift_reg_mutex = NULL;

static uint8_t current_reg1 = 0x00;
static uint8_t current_reg2 = 0x00;

static shift_reg_anim_t current_anim = ANIM_NONE;
static uint32_t anim_step = 0;
static int64_t next_anim_time_ms = 0;
static uint8_t anim_key_index = 0;

// Keypad activity tracking for the 20-second LED light-up window
#define KEYPAD_ACTIVE_DURATION_MS 20000
static int64_t keypad_active_until_ms =
    0; // Timestamp until which all LEDs should be ON
static bool keypad_leds_forced_on =
    false; // True while inside the 20-second window

// Screensaver wakeup: set when a touch breaks the screensaver so the caller
// can discard that keypress (first touch = wake only, not an input event).
static bool keypad_wakeup_pending = false;

// Blink restore: non-zero while a touch blink is in progress.
// When elapsed, all keypad LEDs are restored to fully ON.
static int64_t blink_restore_ms = 0;

static void set_all_leds(uint16_t mask);

// Mappings for '1' to '#' to index 1-12
static uint8_t char_to_led_index(char c) {
  if (c >= '1' && c <= '9')
    return c - '0';
  if (c == '*')
    return 10;
  if (c == '0')
    return 11;
  if (c == '#')
    return 12;
  return 0; // Invalid
}

// Shifts 8 bits out and latches them
static void shift_out_and_latch(uint8_t data, int latch_pin) {
  for (int i = 7; i >= 0; i--) {
    gpio_set_level(SHIFT_REG_CLK_PIN, 0);
    gpio_set_level(SHIFT_REG_DATA_PIN, (data >> i) & 0x01);
    ets_delay_us(1);
    gpio_set_level(SHIFT_REG_CLK_PIN, 1);
    ets_delay_us(1);
  }
  gpio_set_level(SHIFT_REG_CLK_PIN, 0);

  gpio_set_level(latch_pin, 1);
  ets_delay_us(1);
  gpio_set_level(latch_pin, 0);
}

void hal_shift_reg_write(uint8_t reg1_data, uint8_t reg2_data) {
  if (shift_reg_mutex)
    xSemaphoreTake(shift_reg_mutex, portMAX_DELAY);
  current_reg1 = reg1_data;
  current_reg2 = reg2_data;
  shift_out_and_latch(current_reg1, SHIFT_REG_LATCH1_PIN);
  shift_out_and_latch(current_reg2, SHIFT_REG_LATCH2_PIN);
  if (shift_reg_mutex)
    xSemaphoreGive(shift_reg_mutex);
}

void hal_shift_reg_init(void) {
  gpio_config_t io_conf = {.intr_type = GPIO_INTR_DISABLE,
                           .mode = GPIO_MODE_OUTPUT,
                           .pin_bit_mask = (1ULL << SHIFT_REG_CLK_PIN) |
                                           (1ULL << SHIFT_REG_DATA_PIN) |
                                           (1ULL << SHIFT_REG_LATCH1_PIN) |
                                           (1ULL << SHIFT_REG_LATCH2_PIN),
                           .pull_down_en = 0,
                           .pull_up_en = 0};
  gpio_config(&io_conf);

  gpio_set_level(SHIFT_REG_CLK_PIN, 0);
  gpio_set_level(SHIFT_REG_LATCH1_PIN, 0);
  gpio_set_level(SHIFT_REG_LATCH2_PIN, 0);

  if (shift_reg_mutex == NULL) {
    shift_reg_mutex = xSemaphoreCreateMutex();
  }

  hal_shift_reg_write(0x00, 0x00);
}

static uint32_t anim_duration_ms = 0;
static int64_t anim_end_time_ms = 0;

void hal_shift_reg_play_anim(shift_reg_anim_t anim, uint32_t arg) {
  current_anim = anim;
  anim_step = 0;
  next_anim_time_ms = esp_timer_get_time() / 1000;
  blink_restore_ms =
      0; // Clear any pending blink restores to prevent overlapping

  if (anim == ANIM_KEY_BLINK) {
    anim_key_index = char_to_led_index((char)arg);
  } else if (anim == ANIM_SUCCESS_TICK || anim == ANIM_ERROR_FLASH) {
    anim_duration_ms = arg;
    anim_end_time_ms = next_anim_time_ms + arg;
  }
}

void hal_shift_reg_tick(void) {
  int64_t now = esp_timer_get_time() / 1000;

  // --- Touch blink restore ---
  // A touch blink dimmed some LEDs briefly; restore full brightness after the
  // timer.
  if (blink_restore_ms > 0 && now >= blink_restore_ms) {
    blink_restore_ms = 0;
    if (keypad_leds_forced_on && current_anim == ANIM_NONE) {
      set_all_leds(0xFFF);
    }
  }

  // --- Keypad activity window management ---
  if (keypad_leds_forced_on) {
    if (now >= keypad_active_until_ms) {
      // Window expired: turn off all keypad LEDs so idle animation can start.
      keypad_leds_forced_on = false;
      blink_restore_ms = 0;
      if (current_anim == ANIM_NONE) {
        set_all_leds(0);
      }
    } else if (current_anim == ANIM_NONE && blink_restore_ms == 0) {
      // Still inside the window, no animation, no blink in progress — keep all
      // ON.
      set_all_leds(0xFFF);
    }
  }

  if (current_anim == ANIM_NONE)
    return;

  if (now < next_anim_time_ms)
    return;

  switch (current_anim) {
  case ANIM_KEY_BLINK:
    if (anim_step == 0) {
      if (anim_key_index > 0)
        set_all_leds(1 << (anim_key_index - 1));
      next_anim_time_ms = now + 100;
      anim_step++;
    } else {
      set_all_leds(0);
      current_anim = ANIM_NONE;
    }
    break;

  case ANIM_SUCCESS_TICK:
    // Tick (Checkmark) shape is now a Frame shape per user request: 0x0F6F
    if (anim_step == 0) {
      set_all_leds(0); // clear all first
      next_anim_time_ms = now + 100;
      anim_step++;
    } else {
      if (now >= anim_end_time_ms) {
        current_anim = ANIM_NONE;
        set_all_leds(0);
      } else {
        if (anim_step % 2 != 0) {
          set_all_leds(0x0F6F); // Frame shape
        } else {
          set_all_leds(0);
        }
        next_anim_time_ms = now + 300;
        anim_step++;
      }
    }
    break;

  case ANIM_FACTORY_RESET:
    // "O" shape sequence looping 3 times for factory reset
    {
      static const uint8_t ring[] = {1, 2, 3, 6, 9, 12, 11, 10, 7, 4};
      if (anim_step < sizeof(ring) * 3) {
        int idx = anim_step % sizeof(ring);
        set_all_leds(1 << (ring[idx] - 1));
        next_anim_time_ms = now + 40; // Fast rotation spin
        anim_step++;
      } else if (anim_step == sizeof(ring) * 3) {
        set_all_leds(0); // clear once finished
        next_anim_time_ms = now + 50;
        anim_step++;
      } else {
        current_anim = ANIM_NONE;
      }
    }
    break;

  case ANIM_MASTER_WAVE:
    // Top to bottom cascade wave: (1,2,3), then (4,5,6), then (7,8,9), then
    // (*,0,#)
    {
      static const uint16_t wave[] = {0x0007, 0x0038, 0x01C0, 0x0E00};
      if (anim_step < 4) {
        set_all_leds(wave[anim_step]);
        next_anim_time_ms = now + 70;
        anim_step++;
      } else {
        set_all_leds(0);
        current_anim = ANIM_NONE;
      }
    }
    break;

  case ANIM_ERROR_FLASH:
    if (anim_step == 0) {
      set_all_leds(0); // clear all first for dramatic effect
      next_anim_time_ms = now + 100;
      anim_step++;
    } else {
      if (now >= anim_end_time_ms) {
        current_anim = ANIM_NONE;
        set_all_leds(0);
      } else {
        if (anim_step % 2 != 0) {
          // Inner cross/X shape: 1,3,5,7,9 (Bits 0,2,4,6,8) -> 0x0155
          set_all_leds(0x0155);
        } else {
          set_all_leds(0);
        }
        next_anim_time_ms = now + 300;
        anim_step++;
      }
    }
    break;

  case ANIM_IDLE_SNAKE:
    // Snake pattern across the keypad layout:
    //   Row 1 L→R:  1, 2, 3
    //   Row 2 R→L:  6, 5, 4
    //   Row 3 L→R:  7, 8, 9
    //   Row 4 R→L:  12(#), 11(0), 10(*)
    // Then full reverse, then 3 s pause, then loop.
    {
      // key numbers (1-based) for forward pass
      static const uint8_t snake_fwd[12] = {1, 2, 3, 6,  5,  4,
                                            7, 8, 9, 12, 11, 10};
      // reverse pass is simply snake_fwd traversed backwards
      const uint32_t STEPS_FWD = 12;  // steps 0-11
      const uint32_t STEPS_REV = 12;  // steps 12-23
      const uint32_t STEP_PAUSE = 24; // step 24: dark pause
      const uint32_t STEP_END = 25;   // step 25: restart

      if (anim_step < STEPS_FWD) {
        // Forward pass
        set_all_leds(1 << (snake_fwd[anim_step] - 1));
        next_anim_time_ms = now + 80;
        anim_step++;
      } else if (anim_step < STEPS_FWD + STEPS_REV) {
        // Reverse pass (walk snake_fwd array backwards)
        uint32_t rev_idx = STEPS_FWD - 1 - (anim_step - STEPS_FWD);
        set_all_leds(1 << (snake_fwd[rev_idx] - 1));
        next_anim_time_ms = now + 80;
        anim_step++;
      } else if (anim_step == STEP_PAUSE) {
        // All off, 3 second pause
        set_all_leds(0);
        next_anim_time_ms = now + 3000;
        anim_step = STEP_END;
      } else {
        // Loop: restart the animation from step 0
        anim_step = 0;
        next_anim_time_ms = now;
      }
    }
    break;

  case ANIM_IDLE_WAVE:
    {
      const uint16_t row_masks[4] = {
          0x0007, // Row 1: 1,2,3
          0x0038, // Row 2: 4,5,6
          0x01C0, // Row 3: 7,8,9
          0x0E00  // Row 4: *,0,#
      };
      // We want to loop the 0->1->2->3->2->1 pattern multiple times.
      // One cycle is 6 steps. 3 cycles = 18 steps.
      const uint32_t CYCLES = 3;
      const uint32_t STEPS_PER_CYCLE = 6;
      const uint32_t TOTAL_STEPS = CYCLES * STEPS_PER_CYCLE;
      const uint32_t STEP_PAUSE = TOTAL_STEPS;
      const uint32_t STEP_END = TOTAL_STEPS + 1;

      if (anim_step < TOTAL_STEPS) {
          uint32_t cycle_step = anim_step % STEPS_PER_CYCLE;
          if (cycle_step < 4) {
              set_all_leds(row_masks[cycle_step]);
          } else {
              set_all_leds(row_masks[6 - cycle_step]); // 4 -> row 2, 5 -> row 1
          }
          next_anim_time_ms = now + 150;
          anim_step++;
      } else if (anim_step == STEP_PAUSE) {
          set_all_leds(0);
          next_anim_time_ms = now + 3000;
          anim_step = STEP_END;
      } else {
          anim_step = 0;
          next_anim_time_ms = now;
      }
    }
    break;

  default:
    current_anim = ANIM_NONE;
    break;
  }
}

// Fixed Helpers for specific system bits
typedef struct {
  uint8_t reg;
  uint8_t bit;
} led_map_t;
static const led_map_t led_map[12] = {
    {1, 1}, // 1 -> U1_QB
    {1, 7}, // 2 -> U1_QH
    {1, 6}, // 3 -> U1_QG
    {1, 2}, // 4 -> U1_QC
    {1, 4}, // 5 -> U1_QE
    {1, 5}, // 6 -> U1_QF
    {2, 2}, // 7 -> U2_QC
    {2, 0}, // 8 -> U2_QA
    {1, 0}, // 9 -> U1_QA
    {2, 1}, // 10-> U2_QB
    {2, 3}, // 11-> U2_QD
    {2, 4}  // 12-> U2_QE
};

static void set_all_leds(uint16_t mask) {
  if (shift_reg_mutex)
    xSemaphoreTake(shift_reg_mutex, portMAX_DELAY);
  uint8_t r1 = current_reg1 & MASK_U1_TSM_RST;
  uint8_t r2 = (current_reg2 & MASK_U2_RFID_RST) |
               (current_reg2 & MASK_U2_WIEGAND_EN) |
               (current_reg2 & MASK_U2_RELAY);

  for (int i = 0; i < 12; i++) {
    if (mask & (1 << i)) {
      if (led_map[i].reg == 1)
        r1 |= (1 << led_map[i].bit);
      else
        r2 |= (1 << led_map[i].bit);
    }
  }

  current_reg1 = r1;
  current_reg2 = r2;
  shift_out_and_latch(current_reg1, SHIFT_REG_LATCH1_PIN);
  shift_out_and_latch(current_reg2, SHIFT_REG_LATCH2_PIN);
  if (shift_reg_mutex)
    xSemaphoreGive(shift_reg_mutex);
}

// Helpers
static void update_reg1_bit(uint8_t mask, bool state) {
  if (shift_reg_mutex)
    xSemaphoreTake(shift_reg_mutex, portMAX_DELAY);
  if (state)
    current_reg1 |= mask;
  else
    current_reg1 &= ~mask;
  shift_out_and_latch(current_reg1, SHIFT_REG_LATCH1_PIN);
  if (shift_reg_mutex)
    xSemaphoreGive(shift_reg_mutex);
}

static void update_reg2_bit(uint8_t mask, bool state) {
  if (shift_reg_mutex)
    xSemaphoreTake(shift_reg_mutex, portMAX_DELAY);
  if (state)
    current_reg2 |= mask;
  else
    current_reg2 &= ~mask;
  shift_out_and_latch(current_reg2, SHIFT_REG_LATCH2_PIN);
  if (shift_reg_mutex)
    xSemaphoreGive(shift_reg_mutex);
}

void hal_shift_reg_set_rfid_rst(bool state) {
  update_reg2_bit(MASK_U2_RFID_RST, state);
}
void hal_shift_reg_set_tsm_rst(bool state) {
  update_reg1_bit(MASK_U1_TSM_RST, state);
}
void hal_shift_reg_set_relay(bool state) {
  update_reg2_bit(MASK_U2_RELAY, state);
}
void hal_shift_reg_set_wiegand_en(bool state) {
  update_reg2_bit(MASK_U2_WIEGAND_EN, state);
}

void hal_shift_reg_set_led(uint8_t led_index, bool state) {
  if (led_index < 1 || led_index > 12)
    return;
  int i = led_index - 1;
  if (shift_reg_mutex)
    xSemaphoreTake(shift_reg_mutex, portMAX_DELAY);
  if (led_map[i].reg == 1) {
    if (state)
      current_reg1 |= (1 << led_map[i].bit);
    else
      current_reg1 &= ~(1 << led_map[i].bit);
    shift_out_and_latch(current_reg1, SHIFT_REG_LATCH1_PIN);
  } else {
    if (state)
      current_reg2 |= (1 << led_map[i].bit);
    else
      current_reg2 &= ~(1 << led_map[i].bit);
    shift_out_and_latch(current_reg2, SHIFT_REG_LATCH2_PIN);
  }
  if (shift_reg_mutex)
    xSemaphoreGive(shift_reg_mutex);
}

// Returns true while the 20-second keypad active window is running.
bool hal_shift_reg_is_keypad_active(void) {
  int64_t now_ms = esp_timer_get_time() / 1000;
  return now_ms < keypad_active_until_ms;
}

// Returns true ONCE if the previous touch was a screensaver-wakeup touch
// (i.e. it arrived while the window was inactive). Clears the flag on read.
// The caller should discard the associated keypress.
bool hal_shift_reg_consume_wakeup_touch(void) {
  if (keypad_wakeup_pending) {
    keypad_wakeup_pending = false;
    return true;
  }
  return false;
}

void hal_shift_reg_set_touch_mask(uint16_t mask) {
  if (hal_shift_reg_is_blocking_anim_playing()) return;

  static uint16_t last_touch_mask = 0;
  static int64_t last_change_time = 0;
  int64_t now_ms = esp_timer_get_time() / 1000;

  bool in_active_window = (now_ms < keypad_active_until_ms);

  if (mask != last_touch_mask) {
    if (mask != 0) {
      // --- New touch event ---
      // 1. Check if this touch is breaking the screensaver (window was
      // inactive).
      if (!in_active_window) {
        // Mark as wakeup so the consumer can discard the associated keypress.
        keypad_wakeup_pending = true;
        // Wakeup touch: just light all LEDs (no blink — user just woke the
        // device).
        set_all_leds(0xFFF);
      } else {
        // Normal touch during active window: blink the touched key(s) off
        // for 120 ms then restore full brightness via the timer.
        set_all_leds(0xFFF & ~mask); // Touched keys dim briefly
        blink_restore_ms = now_ms + 220;
      }

      // 2. Refresh (or start) the 20-second activity window.
      keypad_active_until_ms = now_ms + KEYPAD_ACTIVE_DURATION_MS;
      keypad_leds_forced_on = true;

      // 3. Abort any running idle animation.
      current_anim = ANIM_NONE;
    }
    // On release (mask == 0): do NOT clear LEDs; the 20-second window keeps
    // them on.
    last_touch_mask = mask;
    last_change_time = now_ms;
  } else if (last_touch_mask != 0 && (now_ms - last_change_time) > 200) {
    // Stuck LED fix: mask hasn't changed in 200ms but still non-zero.
    // Chip missed the release event — force-reset touch tracking.
    last_touch_mask = 0;
    // Keep LEDs on if still inside the activity window.
    if (now_ms < keypad_active_until_ms && blink_restore_ms == 0 &&
        current_anim == ANIM_NONE) {
      set_all_leds(0xFFF);
    }
  }

  // Maintain full LED brightness while inside the active window (handles
  // release restore), but only when no blink is in progress (blink_restore_ms
  // == 0).
  if (mask == 0 && in_active_window && blink_restore_ms == 0 &&
      current_anim == ANIM_NONE) {
    set_all_leds(0xFFF);
  }
}

bool hal_shift_reg_is_blocking_anim_playing(void) {
  return (current_anim == ANIM_SUCCESS_TICK || current_anim == ANIM_ERROR_FLASH || current_anim == ANIM_FACTORY_RESET);
}

void hal_shift_reg_wakeup_keypad(void) {
  int64_t now_ms = esp_timer_get_time() / 1000;
  keypad_active_until_ms = now_ms + KEYPAD_ACTIVE_DURATION_MS;
  keypad_leds_forced_on = true;
  if (!hal_shift_reg_is_blocking_anim_playing()) {
    current_anim = ANIM_NONE;
    set_all_leds(0xFFF);
  }
}
