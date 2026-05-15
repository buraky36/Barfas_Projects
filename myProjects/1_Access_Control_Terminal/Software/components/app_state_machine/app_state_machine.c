#include "app_state_machine.h"
#include "api_client.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "hal_io.h"
#include "hal_qr.h"
#include "hal_shift_reg.h"
#include "hal_touch.h"
#include "hal_wiegand.h"
#include "hw_config.h"
#include "mfrc522.h"
#include "nv_storage.h"
#include "wifi_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static const char *TAG = "APP_SM";
static app_state_t current_state = STATE_BOOTING;
static sys_config_t config;

static int64_t state_timer_start = 0;
static int64_t led_revert_timer_ms = 0;

static void app_dump_user_list(void) {
  ESP_LOGI(TAG, "==== USER LIST PLEASE ====");
  ESP_LOGI(TAG, "System Passwords Table");
  ESP_LOGI(TAG, "1. MasterCode : \"%s\"", config.master_code);

  for (int i = 0; i < config.master_card_count; i++) {
    ESP_LOGI(TAG, "%c. MasterID %d - Master Card Number: %s", 'a' + i,
             config.master_ids[i], config.master_cards[i]);
  }

  ESP_LOGI(TAG, "Users Table");
  for (uint16_t i = 1; i <= 9988; i++) {
    user_record_t u;
    if (nv_storage_get_user(i, &u)) {
      if (strlen(u.pin) > 0) {
        ESP_LOGI(TAG, "UserID %d - PIN: %s", u.user_id, u.pin);
      }
      if (strlen(u.card_id) > 0) {
        ESP_LOGI(TAG, "UserID %d - User Card Number: %s", u.user_id, u.card_id);
      }
      if (strlen(u.qr_id) > 0) {
        ESP_LOGI(TAG, "UserID %d - User QR: %s", u.user_id, u.qr_id);
      }
    }
  }
  ESP_LOGI(TAG, "==========================");

  // Visual and Audio feedback for the dump
  hal_io_led_set(LED_COLOR_GREEN,
                 LED_MODE_SOLID); // Using set directly because app_set_temp_led
                                  // is static below
  hal_io_buzzer_beep(2700, 100, 2);
}

static const char *state_to_string(app_state_t state) {
  switch (state) {
  case STATE_BOOTING:
    return "BOOTING";
  case STATE_IDLE:
    return "IDLE";
  case STATE_PROGRAMMING:
    return "PROGRAMMING";
  case STATE_FACTORY_RESET:
    return "FACTORY_RESET";
  case STATE_DOOR_OPEN:
    return "DOOR_OPEN";
  case STATE_ALARM:
    return "ALARM";
  case STATE_USER_PIN_CHANGE:
    return "USER_PIN_CHANGE";
  case STATE_MASTER_ADD_MODE:
    return "MASTER_ADD_MODE";
  case STATE_MASTER_DELETE_MODE:
    return "MASTER_DELETE_MODE";
  default:
    return "UNKNOWN";
  }
}

typedef enum {
  PROG_STATE_ROOT,
  PROG_STATE_UPDATE_MASTER_CODE_1, // 0
  PROG_STATE_UPDATE_MASTER_CODE_2, // 0
  PROG_STATE_ADD_MASTER_CARD,      // 1
  PROG_STATE_ADD_MASTER_SPECIFIC,  // 1
  PROG_STATE_ADD_USER,             // 2 (ROOT)
  PROG_STATE_ADD_USER_SPECIFIC,    // 2 (SPECIFIC)
  PROG_STATE_ADD_USER_BLOCK,       // 2 (BLOCK)
  PROG_STATE_DEL_USER,             // 3
  PROG_STATE_RELAY,                // 4
  PROG_STATE_ACCESS_MODE,          // 5
  PROG_STATE_WORKING_MODE,         // 6
  PROG_STATE_DOOR_DETECTION,       // 7
  PROG_STATE_DOOR_DETECTION_VAL,   // 7
  PROG_STATE_AUDIO_VISUAL,         // 8
  PROG_STATE_WIEGAND,              // 9 - Wiegand card format (26/34)
  PROG_STATE_WIEGAND_PIN,          // 9 - PIN output format (4/8)
  PROG_STATE_WIEGAND_PARITY        // 9 - Parity enable (0/1)
} prog_substate_t;

typedef enum {
  PIN_CHANGE_OLD,
  PIN_CHANGE_NEW,
  PIN_CHANGE_REP
} user_pin_substate_t;
static user_pin_substate_t pin_change_state = PIN_CHANGE_OLD;
static user_record_t pin_change_user;
static char temp_new_pin[16];

static prog_substate_t prog_state = PROG_STATE_ROOT;
static char input_buffer[64];
static uint8_t input_len = 0;
static uint16_t pending_user_id = 0;
// Reader Mode * prefix: tracks if the buffered sequence has diverged from
// MasterCode If true, digits after * are sent over Wiegand. Reset on * press.
static bool wg_prefix_deviated = false;
static uint16_t pending_quantity = 0;
static char temp_master_code[16];
static bool is_entering_master = false;
static bool relay_state = false;
// static char current_master_card[32]; // Track the master card used to enter
// Master Mode

void app_set_state(app_state_t new_state) {
  if (new_state != current_state) {
    ESP_LOGI(TAG, "State Transition: %s -> %s", state_to_string(current_state),
             state_to_string(new_state));
  }
  current_state = new_state;
  state_timer_start = esp_timer_get_time();
  led_revert_timer_ms = 0; // Clear temporary led override

  switch (new_state) {
  case STATE_IDLE:
    hal_io_led_set(LED_COLOR_GREEN, LED_MODE_BLINK_SLOW);
    input_len = 0;
    is_entering_master = false;
    break;
  case STATE_PROGRAMMING:
    hal_io_led_set(LED_COLOR_BLUE, LED_MODE_BLINK_FAST);
    prog_state = PROG_STATE_ROOT;
    input_len = 0;
    pending_user_id = 0;
    hal_io_buzzer_beep(2000, 200, 1);
    break;
  case STATE_FACTORY_RESET:
    hal_io_led_set(LED_COLOR_ORANGE, LED_MODE_SOLID);
    hal_shift_reg_play_anim(ANIM_FACTORY_RESET, 0);
    hal_io_buzzer_beep(2700, 1000, 1);
    break;
  case STATE_DOOR_OPEN:
    // "Solid Green during beeps" -> the buzzer already plays, let's set solid
    // green temporarily here
    hal_io_led_set(LED_COLOR_GREEN, LED_MODE_SOLID);
    hal_io_relay_set(true);
    hal_io_buzzer_beep(2700, 100, 1);
    break;
  case STATE_ALARM:
    hal_io_led_set(LED_COLOR_RED, LED_MODE_BLINK_FAST);
    break;
  case STATE_USER_PIN_CHANGE:
    hal_io_led_set(LED_COLOR_YELLOW, LED_MODE_BLINK_FAST);
    pin_change_state = PIN_CHANGE_OLD;
    input_len = 0;
    hal_io_buzzer_beep(2000, 200, 1);
    break;
  case STATE_MASTER_ADD_MODE:
    hal_io_led_set(LED_COLOR_BLUE, LED_MODE_BLINK_SLOW);
    hal_io_buzzer_beep(2700, 100, 2); // Enter add mode
    input_len = 0;
    pending_user_id = 0;
    break;
  case STATE_MASTER_DELETE_MODE:
    hal_io_led_set(LED_COLOR_RED, LED_MODE_BLINK_SLOW);
    hal_io_buzzer_beep(2700, 100, 3); // Enter delete mode
    input_len = 0;
    break;
  default:
    break;
  }
}

void app_state_machine_init(void) {
  nv_storage_get_config(&config);
  hal_io_set_sound_enabled(config.sound_enabled);
  // Apply Wiegand Direction based on Working Mode
  if (config.working_mode == 2) { // Controller (Input)
    hal_wiegand_set_direction(false);
  } else if (config.working_mode == 3) { // Reader (Output)
    hal_wiegand_set_direction(true);
  } else {
    // Standalone defaults to Input passively or uninitialized
    hal_wiegand_set_direction(false);
  }

  // Play startup sweep sound
  hal_io_buzzer_intro_sweep();

  app_set_state(STATE_IDLE);
}

uint8_t app_get_working_mode(void) {
  return config.working_mode;
}

void app_trigger_door_open(void) {
  uint32_t dur = config.relay_time > 0 ? (config.relay_time * 1000) : 3000;
  hal_shift_reg_play_anim(ANIM_SUCCESS_TICK, dur);
  if (config.relay_time == 0) {
    relay_state = !relay_state;
    if (relay_state) {
      app_set_state(STATE_DOOR_OPEN);
    } else {
      hal_io_relay_set(false);
      app_set_state(STATE_IDLE);
    }
  } else {
    relay_state = true;
    app_set_state(STATE_DOOR_OPEN);
  }
}

void app_trigger_alarm(void) { app_set_state(STATE_ALARM); }

static bool app_is_access_granted(const char *prefix) {
  if (config.access_mode == 6)
    return true; // Any
  if (config.access_mode == 0 && strcmp(prefix, "RFID") == 0)
    return true; // Card Only
  if (config.access_mode == 1 && strcmp(prefix, "KEYPAD") == 0)
    return true; // PIN Only
  if (config.access_mode == 2 && strcmp(prefix, "QR") == 0)
    return true; // QR Only
  if (config.access_mode == 3 &&
      (strcmp(prefix, "RFID") == 0 || strcmp(prefix, "KEYPAD") == 0))
    return true; // Card or PIN
  if (config.access_mode == 4 &&
      (strcmp(prefix, "RFID") == 0 || strcmp(prefix, "QR") == 0))
    return true; // Card or QR
  if (config.access_mode == 5 &&
      (strcmp(prefix, "KEYPAD") == 0 || strcmp(prefix, "QR") == 0))
    return true; // PIN or QR
  return false;
}

static void app_set_temp_led(led_color_t color, led_mode_t mode,
                             uint32_t duration_ms) {
  hal_io_led_set(color, mode);
  led_revert_timer_ms = (esp_timer_get_time() / 1000) + duration_ms;
}

static void process_auth_pass_command(const char *prefix, const char *data) {
  char payload[128];
  snprintf(payload, sizeof(payload), "%s:%s", prefix, data);

  if (!app_is_access_granted(prefix)) {
    ESP_LOGW(TAG, "Access Mode %d Denied Access for %s", config.access_mode,
             payload);
    app_set_temp_led(LED_COLOR_RED, LED_MODE_SOLID, 3000);
    hal_io_buzzer_beep(1000, 300, 3);
    hal_shift_reg_play_anim(ANIM_ERROR_FLASH, 3000);
    return;
  }

  if (config.is_online) {
    if (wifi_manager_is_connected()) {
      ESP_LOGI(TAG, "Online Mode Auth: %s", payload);
      // Send only the raw credential value (card ID, PIN, or QR string)
      int code = api_client_send_pass_event(data);
      if (code == 200) {
        ESP_LOGI(TAG, "Online Server Granted Access!");
        hal_io_buzzer_beep(2700, 100, 2); // 2 Rapid Beeps
        app_trigger_door_open();
      } else {
        ESP_LOGW(TAG, "Online Server Denied Access! Code: %d", code);
        app_set_temp_led(LED_COLOR_RED, LED_MODE_SOLID, 3000);
        hal_io_buzzer_beep(1000, 300, 3); // 3 Slow Beeps
        hal_shift_reg_play_anim(ANIM_ERROR_FLASH, 3000);
      }
    } else {
      ESP_LOGW(TAG, "Online Mode but WiFi is disconnected! Strict Deny.");
      app_set_temp_led(LED_COLOR_RED, LED_MODE_SOLID, 3000);
      hal_io_buzzer_beep(1000, 300, 3);
      hal_shift_reg_play_anim(ANIM_ERROR_FLASH, 3000);
    }
    return; // Strict: Do NOT fallback to local DB if online
  }

  // Offline validation
  user_record_t u;
  bool found = false;
  if (strcmp(prefix, "KEYPAD") == 0)
    found = nv_storage_find_user_by_pin(data, &u);
  else if (strcmp(prefix, "RFID") == 0)
    found = nv_storage_find_user_by_card(data, &u);
  else if (strcmp(prefix, "QR") == 0)
    found = nv_storage_find_user_by_qr(data, &u);

  if (found) {
    ESP_LOGI(TAG, "Offline Mode Granted Access for %s (UserID: %d)", payload, u.user_id);
    hal_io_buzzer_beep(2700, 100, 2); // 2 Rapid Beeps
    app_trigger_door_open();
  } else {
    ESP_LOGW(TAG, "Offline Mode Denied Access for %s", payload);
    app_set_temp_led(LED_COLOR_RED, LED_MODE_SOLID, 3000);
    hal_io_buzzer_beep(1000, 300, 3); // 3 Slow Beeps
    hal_shift_reg_play_anim(ANIM_ERROR_FLASH, 3000);
  }
}

void app_state_machine_tick(void) {
  hal_io_tick();
  hal_shift_reg_tick();
  int64_t now_us = esp_timer_get_time();
  int64_t elapsed_ms = (now_us - state_timer_start) / 1000;

  if (led_revert_timer_ms > 0 && now_us / 1000 >= led_revert_timer_ms) {
    led_revert_timer_ms = 0;
    if (current_state == STATE_IDLE)
      hal_io_led_set(LED_COLOR_GREEN, LED_MODE_BLINK_SLOW);
    else if (current_state == STATE_PROGRAMMING)
      hal_io_led_set(LED_COLOR_BLUE, LED_MODE_BLINK_FAST);
    else if (current_state == STATE_ALARM)
      hal_io_led_set(LED_COLOR_RED, LED_MODE_BLINK_FAST);
    else if (current_state == STATE_MASTER_ADD_MODE)
      hal_io_led_set(LED_COLOR_BLUE, LED_MODE_BLINK_SLOW);
    else if (current_state == STATE_MASTER_DELETE_MODE)
      hal_io_led_set(LED_COLOR_RED, LED_MODE_BLINK_SLOW);
    else if (current_state == STATE_USER_PIN_CHANGE)
      hal_io_led_set(LED_COLOR_YELLOW, LED_MODE_BLINK_FAST);
  }

  static int64_t last_heartbeat = 0;
  if (now_us - last_heartbeat > 25000000) {
    ESP_LOGI(TAG, "Heartbeat - System is alive. Current State: %s, Online: %d",
             state_to_string(current_state), config.is_online);
    last_heartbeat = now_us;
  }

  static int64_t last_anim_tick = 0;
  if (current_state == STATE_IDLE && now_us - last_anim_tick > 3000000) {
    // Only play the idle animation when the 20-second keypad activity window
    // has expired (i.e., no key has been pressed recently).
    if (!hal_shift_reg_is_keypad_active()) {
      if (active_hw_version == HW_VERSION_QR_ONLY) {
        hal_shift_reg_play_anim(ANIM_IDLE_SNAKE, '\0');
      } else {
        hal_shift_reg_play_anim(ANIM_IDLE_WAVE, '\0');
      }
    }
    last_anim_tick = now_us;
  }

  // Rule: Auto-switch to Online permanently when connected to Wi-Fi
  if (wifi_manager_is_connected() && !config.is_online) {
    config.is_online = true;
    nv_storage_set_config(&config);
    ESP_LOGI(TAG, "WiFi connected! Switched permanently to Online Mode.");
  }

  // Dig_In Hardware Handlers
  if (config.working_mode == 1 || config.working_mode == 2) {
    if (hal_io_read_exit_button()) { // Exit Button
      if (current_state == STATE_IDLE) {
        ESP_LOGI(TAG, "Exit Button Pressed - Opening Door");
        app_trigger_door_open();
      }
    }

    if (config.door_open_detection && !hal_io_read_door_sensor()) {
      // Door is actively open. Can trigger alarm if open too long etc.
    }
  }

  switch (current_state) {
  /*
   * FACTORY RESET
   * -------------
   * Triggered by entering '0' five times on the keypad within
   * the first 10 seconds after power-on.
   *
   * What gets erased:
   *   - All registered users (cards, PINs, QR codes)
   *   - All master card credentials
   *   - Wi-Fi SSID / password
   *
   * What gets restored to factory defaults:
   *   - Master code       : "123456"
   *   - Relay time        : 5 seconds
   *   - Access mode       : Card + PIN + QR (all allowed)
   *   - Working mode      : Standalone
   *   - Online mode       : disabled
   *   - Door detection    : enabled
   *   - Wiegand format    : 34-bit
   *
   * The device plays the ANIM_FACTORY_RESET animation for 3 seconds
   * before wiping data, then performs a hard restart.
   */
  case STATE_FACTORY_RESET: {
    if (elapsed_ms >= 3000) { // Wait for animation to finish
      nv_storage_delete_all_users();
      nv_storage_clear_master_credentials();

      wifi_manager_clear_credentials();

      memset(&config, 0, sizeof(config));
      strcpy(config.master_code, "123456");
      config.relay_time = 5;
      config.access_mode = 6;
      config.alarm_time = 1;
      config.sound_enabled = true;
      config.led_always_on = true;
      config.working_mode = 1;
      config.is_online = false;
      config.door_open_detection = true;
      config.wiegand_format = 34;
      nv_storage_set_config(&config);

      ESP_LOGW(TAG, "Wipe complete. Restarting system...");
      esp_restart();
    }
    break;
  }
  case STATE_IDLE: {
    char key = '\0';
    if (hal_touch_read_key(&key)) {
      if (hal_shift_reg_is_blocking_anim_playing())
        break;
      // If this touch just woke the device from screensaver mode,
      // discard it entirely — it's a wakeup event, not user input.
      if (hal_shift_reg_consume_wakeup_touch()) {
        ESP_LOGI(
            TAG,
            "[IDLE] Screensaver wakeup touch consumed (key='%c' discarded).",
            key);
        break;
      }

      ESP_LOGI(TAG, "[IDLE] Key pressed: %c", key);

      /*
       * Factory Reset Backdoor
       * ----------------------
       * Press '0' five consecutive times within the first 10 seconds
       * after power-on to trigger a factory reset.
       * Any other key press during this window cancels the sequence.
       */
      if (elapsed_ms < 10000) {
        static int zero_count = 0;
        if (key == '0' && zero_count >= 0) {
          zero_count++;
          hal_io_buzzer_beep(2700, 50, 1);
          if (zero_count == 5) {
            ESP_LOGW(TAG, "5x ZERO ENTERED AT BOOT! FACTORY RESET!");
            app_set_state(STATE_FACTORY_RESET);
          }
          break; // Consume the key, don't pass to normal logic
        } else {
          zero_count = -1; // Wrong key entered, cancels the backdoor
        }
      }

      if (key == '*') {
        is_entering_master = true;
        wg_prefix_deviated = false; // Reset smart-prefix state
        input_len = 0;
        hal_io_buzzer_beep(2700, 50, 1);
      } else if (key == '#') {
        input_buffer[input_len] = '\0';
        if (is_entering_master) {
          if (strcmp(input_buffer, config.master_code) == 0 ||
              (strlen(config.master_code) == 0 &&
               strcmp(input_buffer, "123456") == 0)) {
            hal_shift_reg_play_anim(ANIM_MASTER_WAVE, 0);
            hal_io_buzzer_beep(2700, 100, 2); // 2 Rapid Beeps
            app_set_state(STATE_PROGRAMMING);
          } else {
            uint16_t uid = atoi(input_buffer);
            if (nv_storage_get_user(uid, &pin_change_user)) {
              app_set_state(STATE_USER_PIN_CHANGE);
            } else {
              app_set_temp_led(LED_COLOR_RED, LED_MODE_SOLID, 3000);
              hal_io_buzzer_beep(1000, 300, 3); // 3 Slow Beeps
              hal_shift_reg_play_anim(ANIM_ERROR_FLASH, 3000);
            }
          }
          is_entering_master = false;
          input_len = 0;
        } else if (config.working_mode == 3) {
          // Reader Mode: # ends the PIN burst - send 0x0B for '#'
          hal_wiegand_write(0x0B, config.pin_wiegand_format, false);
          ESP_LOGI(TAG, "Reader Mode: PIN entry completed, sent '#' (0x0B)");
          input_len = 0;
        } else {
          process_auth_pass_command("KEYPAD", input_buffer);
          input_len = 0;
        }
      } else {
        if (config.working_mode == 3 && !is_entering_master) {
          // Reader Mode: send digit immediately over Wiegand
          uint8_t hex_val = 0;
          if (key >= '1' && key <= '9')
            hex_val = key - '0';
          else if (key == '0')
            hex_val = 0x0B;
          uint8_t bits = config.pin_wiegand_format;
          uint32_t send_val =
              (bits == 8) ? (((hex_val & 0x0F) << 4) | (~hex_val & 0x0F))
                          : (hex_val & 0x0F);
          hal_wiegand_write(send_val, bits, false);
          ESP_LOGI(TAG, "Reader Mode: Sent key '%c' = 0x%02X (%d-bit)", key,
                   hex_val, bits);
        } else if (config.working_mode == 3 && is_entering_master &&
                   !wg_prefix_deviated) {
          // Reader Mode * prefix: compare this digit with the MasterCode
          // character at same position
          const char *mc =
              (strlen(config.master_code) == 0) ? "123456" : config.master_code;
          if (key != mc[input_len]) {
            // Diverged from MasterCode! Flush all buffered digits then this
            // one.
            wg_prefix_deviated = true;
            is_entering_master = false;
            ESP_LOGI(TAG,
                     "Reader Mode: * sequence diverged at pos %d, flushing to "
                     "Wiegand",
                     input_len);
            // Flush '* + already-typed digits' as Wiegand keypad burst
            uint8_t bits = config.pin_wiegand_format;
            // Send the '*' first (0x0A)
            uint32_t sv = (bits == 8) ? (((0x0A & 0x0F) << 4) | (~0x0A & 0x0F))
                                      : (0x0A & 0x0F);
            hal_wiegand_write(sv, bits, false);
            // Send all buffered digits
            for (int i = 0; i < input_len; i++) {
              char bc = input_buffer[i];
              uint8_t bv = 0;
              if (bc >= '1' && bc <= '9')
                bv = bc - '0';
              else if (bc == '0')
                bv = 0x0B;
              sv = (bits == 8) ? (((bv & 0x0F) << 4) | (~bv & 0x0F))
                               : (bv & 0x0F);
              hal_wiegand_write(sv, bits, false);
            }
            // Now send the current diverging digit
            uint8_t kv = 0;
            if (key >= '1' && key <= '9')
              kv = key - '0';
            else if (key == '0')
              kv = 0x0B;
            sv =
                (bits == 8) ? (((kv & 0x0F) << 4) | (~kv & 0x0F)) : (kv & 0x0F);
            hal_wiegand_write(sv, bits, false);
          }
          // else: still matching MasterCode, just buffer (don't send)
        }
        if (input_len < 62) {
          input_buffer[input_len++] = key;
          hal_io_buzzer_beep(2700, 50, 1);
        }
      }
    }

    uint32_t rfid_uid = 0;
    bool rfid_ok = false;
    if (active_hw_version != HW_VERSION_QR_ONLY) {
      rfid_ok = mfrc522_check_card(&rfid_uid);
    }
    bool weigand_ok = (config.working_mode != 1) && hal_wiegand_available();
    if (rfid_ok || weigand_ok) {
      if (hal_shift_reg_is_blocking_anim_playing())
        break;
      hal_shift_reg_wakeup_keypad();
      uint8_t bit_len = 0;
      uint32_t card_id =
          (rfid_uid != 0) ? rfid_uid : hal_wiegand_read(&bit_len);
      char card_str[24];
      snprintf(card_str, sizeof(card_str), "%lu", (unsigned long)card_id);

      if (is_entering_master && nv_storage_is_master_credential(card_str)) {
        ESP_LOGI(TAG, "Master Card read after *! Entering Programming Mode.");
        hal_shift_reg_play_anim(ANIM_MASTER_WAVE, 0);
        app_set_state(STATE_PROGRAMMING);
        is_entering_master = false;
      } else if (config.working_mode == 3) {
        // In Reader Mode, any Wiegand input shouldn't happen physically since
        // direction=Output, but if it reads RFID via SPI (MFRC522), it outputs
        // it over Wiegand.
        ESP_LOGI(TAG, "Reader Mode: Forwarding RFID: %s", card_str);
        hal_wiegand_write(card_id, config.wiegand_format,
                          config.wiegand_parity_en);
        hal_io_buzzer_beep(2700, 100, 1);
        is_entering_master = false;
      } else {
        if (nv_storage_is_master_credential(card_str)) {
          int32_t master_id = nv_storage_get_master_id(card_str);
          ESP_LOGI(TAG, "Master Card (ID: %d) scanned in IDLE. Entering Master Add Mode.", (int)master_id);
          hal_shift_reg_play_anim(ANIM_MASTER_WAVE, 0);
          app_set_state(STATE_MASTER_ADD_MODE);
        } else {
          process_auth_pass_command("RFID", card_str);
        }
        is_entering_master = false;
      }
    }

    char qr_str[64];
    if (hal_qr_read(qr_str, sizeof(qr_str))) {
      if (hal_shift_reg_is_blocking_anim_playing())
        break;
      hal_shift_reg_wakeup_keypad();
      if (strcmp(qr_str, "USERLISTPLEASE") == 0) {
        app_dump_user_list();
      } else if (is_entering_master &&
                 nv_storage_is_master_credential(qr_str)) {
        ESP_LOGI(TAG, "Master QR read after *! Entering Programming Mode.");
        hal_shift_reg_play_anim(ANIM_MASTER_WAVE, 0);
        app_set_state(STATE_PROGRAMMING);
        is_entering_master = false;
      } else if (config.working_mode == 3) {
        ESP_LOGI(TAG, "Reader Mode: Transmitting QR: %s", qr_str);
        is_entering_master = false;
      } else {
        if (nv_storage_is_master_credential(qr_str)) {
          int32_t master_id = nv_storage_get_master_id(qr_str);
          ESP_LOGI(TAG, "Master QR (ID: %d) scanned in IDLE. Entering Master Add Mode.", (int)master_id);
          hal_shift_reg_play_anim(ANIM_MASTER_WAVE, 0);
          app_set_state(STATE_MASTER_ADD_MODE);
        } else {
          process_auth_pass_command("QR", qr_str);
        }
        is_entering_master = false;
      }
    }
    break;
  }
  case STATE_USER_PIN_CHANGE: {
    if (elapsed_ms >= 15000) {
      app_set_state(STATE_IDLE);
      hal_io_buzzer_beep(1000, 500, 3);
      break;
    }
    char key = '\0';
    if (hal_touch_read_key(&key)) {
      if (hal_shift_reg_is_blocking_anim_playing())
        break;
      ESP_LOGI(TAG, "[PIN_CHANGE] Key pressed: %c", key);
      state_timer_start = esp_timer_get_time();

      if (key == '*') {
        hal_io_buzzer_beep(1000, 100, 1);
        app_set_state(STATE_IDLE);
      } else if (key == '#') {
        input_buffer[input_len] = '\0';
        if (pin_change_state == PIN_CHANGE_OLD) {
          if (strcmp(input_buffer, pin_change_user.pin) == 0) {
            pin_change_state = PIN_CHANGE_NEW;
            input_len = 0;
            hal_io_buzzer_beep(2700, 100, 1);
          } else {
            hal_io_buzzer_beep(1000, 300, 3);
            app_set_state(STATE_IDLE);
            app_set_temp_led(LED_COLOR_RED, LED_MODE_SOLID, 3000);
            hal_shift_reg_play_anim(ANIM_ERROR_FLASH, 3000);
          }
        } else if (pin_change_state == PIN_CHANGE_NEW) {
          strncpy(temp_new_pin, input_buffer, sizeof(temp_new_pin) - 1);
          pin_change_state = PIN_CHANGE_REP;
          input_len = 0;
          hal_io_buzzer_beep(2700, 100, 1);
        } else if (pin_change_state == PIN_CHANGE_REP) {
          if (strcmp(input_buffer, temp_new_pin) == 0) {
            strncpy(pin_change_user.pin, temp_new_pin,
                    sizeof(pin_change_user.pin) - 1);
            nv_storage_add_user(&pin_change_user); // Updates existing user
            hal_io_buzzer_beep(2700, 100, 2);
            app_set_state(STATE_IDLE);
            app_set_temp_led(LED_COLOR_GREEN, LED_MODE_SOLID, 1000);
          } else {
            hal_io_buzzer_beep(1000, 300, 3);
            app_set_state(STATE_IDLE);
            app_set_temp_led(LED_COLOR_RED, LED_MODE_SOLID, 3000);
            hal_shift_reg_play_anim(ANIM_ERROR_FLASH, 3000);
          }
        }
      } else {
        if (input_len < 10) {
          input_buffer[input_len++] = key;
          hal_io_buzzer_beep(2700, 50, 1);
        }
      }
    }
    break;
  }
  case STATE_MASTER_ADD_MODE:
  case STATE_MASTER_DELETE_MODE: {
    if (elapsed_ms >= 10000) {
      app_set_state(STATE_IDLE);
      hal_io_buzzer_beep(1000, 500, 3);
      break;
    }

    char key = '\0';
    if (hal_touch_read_key(&key)) {
      if (hal_shift_reg_is_blocking_anim_playing())
        break;
      state_timer_start = esp_timer_get_time();
      if (key == '*') {
        app_set_state(STATE_IDLE);
        hal_io_buzzer_beep(1000, 100, 1);
      } else if (key == '#') {
        input_buffer[input_len] = '\0';
        if (input_len > 0) {
          if (current_state == STATE_MASTER_ADD_MODE) {
            if (pending_user_id == 0) {
              pending_user_id = atoi(input_buffer);
              input_len = 0;
              hal_io_buzzer_beep(2700, 100, 1);
            } else {
              // Add PIN
              user_record_t new_user = {0};
              new_user.user_id = pending_user_id;
              new_user.is_active = true;
              strncpy(new_user.pin, input_buffer, sizeof(new_user.pin) - 1);
              if (nv_storage_add_user(&new_user)) {
                app_set_temp_led(LED_COLOR_GREEN, LED_MODE_SOLID, 1000);
                hal_io_buzzer_beep(2700, 100, 2);
              } else {
                app_set_temp_led(LED_COLOR_RED, LED_MODE_SOLID, 3000);
                hal_io_buzzer_beep(1000, 300, 3);
                hal_shift_reg_play_anim(ANIM_ERROR_FLASH, 3000);
              }
              pending_user_id = 0;
              input_len = 0;
            }
          } else {
            // Delete mode: Delete by UserID
            uint16_t uid = atoi(input_buffer);
            if (nv_storage_delete_user(uid)) {
              app_set_temp_led(LED_COLOR_GREEN, LED_MODE_SOLID, 1000);
              hal_io_buzzer_beep(2700, 100, 2);
            } else {
              app_set_temp_led(LED_COLOR_RED, LED_MODE_SOLID, 3000);
              hal_io_buzzer_beep(1000, 300, 3);
              hal_shift_reg_play_anim(ANIM_ERROR_FLASH, 3000);
            }
            input_len = 0;
          }
        }
      } else {
        if (input_len < 62) {
          input_buffer[input_len++] = key;
          hal_io_buzzer_beep(2700, 50, 1);
        }
      }
    }

    uint32_t rfid_uid = 0;
    bool rfid_ok = false;
    if (active_hw_version != HW_VERSION_QR_ONLY) {
      rfid_ok = mfrc522_check_card(&rfid_uid);
    }
    bool weigand_ok = (config.working_mode != 1) && hal_wiegand_available();
    if (rfid_ok || weigand_ok) {
      hal_shift_reg_wakeup_keypad();
      uint8_t bit_len = 0;
      uint32_t card_id =
          (rfid_uid != 0) ? rfid_uid : hal_wiegand_read(&bit_len);
      char card_str[24];
      snprintf(card_str, sizeof(card_str), "%lu", (unsigned long)card_id);
      state_timer_start = esp_timer_get_time();

      ESP_LOGI(TAG, "[MASTER_MODE] Card/Wiegand read: %s", card_str);
      if (nv_storage_is_master_credential(card_str)) {
        int32_t master_id = nv_storage_get_master_id(card_str);
        if (current_state == STATE_MASTER_ADD_MODE && elapsed_ms < 5000) {
          ESP_LOGI(TAG, "[MASTER_MODE] Master card (ID: %d) -> switch to DELETE mode", (int)master_id);
          app_set_state(STATE_MASTER_DELETE_MODE);
        } else if (current_state == STATE_MASTER_DELETE_MODE && elapsed_ms < 5000) {
          ESP_LOGW(TAG, "[MASTER_MODE] Master card (ID: %d) 3rd scan -> Delete ALL users", (int)master_id);
          if (nv_storage_delete_all_users()) {
            ESP_LOGI(TAG, "[MASTER_MODE] ALL users DELETED");
            app_set_temp_led(LED_COLOR_GREEN, LED_MODE_SOLID, 1000);
            hal_io_buzzer_beep(2700, 100, 2);
          }
          app_set_state(STATE_IDLE);
        } else {
          ESP_LOGI(TAG, "[MASTER_MODE] Master card (ID: %d) -> exit to IDLE", (int)master_id);
          app_set_state(STATE_IDLE);
          hal_io_buzzer_beep(2700, 100, 2);
        }
      } else {
        if (current_state == STATE_MASTER_ADD_MODE) {
          user_record_t u;
          if (nv_storage_find_user_by_card(card_str, &u)) {
            ESP_LOGW(TAG, "[MASTER_MODE] Card %s ALREADY registered to UserID:%d", card_str, u.user_id);
            app_set_temp_led(LED_COLOR_RED, LED_MODE_SOLID, 3000);
            hal_io_buzzer_beep(1000, 300, 3);
            hal_shift_reg_play_anim(ANIM_ERROR_FLASH, 3000);
            pending_user_id = 0;
            input_len = 0;
          } else {
            user_record_t new_user = {0};
            new_user.user_id = (pending_user_id > 0)
                                   ? pending_user_id
                                   : nv_storage_get_free_user_id();
            new_user.is_active = true;
            strncpy(new_user.card_id, card_str, sizeof(new_user.card_id) - 1);
            ESP_LOGI(TAG, "[MASTER_MODE] ADD: UserID=%d, Card=%s", new_user.user_id, card_str);
            if (nv_storage_add_user(&new_user)) {
              ESP_LOGI(TAG, "[MASTER_MODE] User SAVED: UserID=%d", new_user.user_id);
              app_set_temp_led(LED_COLOR_GREEN, LED_MODE_SOLID, 1000);
              hal_io_buzzer_beep(2700, 100, 2);
            } else {
              ESP_LOGW(TAG, "[MASTER_MODE] User FAILED to save: UserID=%d", new_user.user_id);
              app_set_temp_led(LED_COLOR_RED, LED_MODE_SOLID, 3000);
              hal_io_buzzer_beep(1000, 300, 3);
              hal_shift_reg_play_anim(ANIM_ERROR_FLASH, 3000);
            }
            pending_user_id = 0;
            input_len = 0;
          }
        } else {
          user_record_t u;
          if (nv_storage_find_user_by_card(card_str, &u)) {
            nv_storage_delete_user(u.user_id);
            ESP_LOGI(TAG, "[MASTER_MODE] DELETE: UserID=%d, Card=%s", u.user_id, card_str);
            app_set_temp_led(LED_COLOR_GREEN, LED_MODE_SOLID, 1000);
            hal_io_buzzer_beep(2700, 100, 2);
          } else {
            ESP_LOGW(TAG, "[MASTER_MODE] DELETE FAILED: Card %s not found", card_str);
            app_set_temp_led(LED_COLOR_RED, LED_MODE_SOLID, 3000);
            hal_io_buzzer_beep(1000, 300, 3);
          }
        }
      }
    }

    char qr_str[64];
    if (hal_qr_read(qr_str, sizeof(qr_str))) {
      state_timer_start = esp_timer_get_time();
      hal_shift_reg_wakeup_keypad();
      if (nv_storage_is_master_credential(qr_str)) {
        if (current_state == STATE_MASTER_ADD_MODE && elapsed_ms < 5000) {
          app_set_state(STATE_MASTER_DELETE_MODE);
        } else if (current_state == STATE_MASTER_DELETE_MODE && elapsed_ms < 5000) {
          if (nv_storage_delete_all_users()) {
            app_set_temp_led(LED_COLOR_GREEN, LED_MODE_SOLID, 1000);
            hal_io_buzzer_beep(2700, 100, 2);
          }
          app_set_state(STATE_IDLE);
        } else {
          app_set_state(STATE_IDLE);
          hal_io_buzzer_beep(2700, 100, 2);
        }
      } else {
        if (current_state == STATE_MASTER_ADD_MODE) {
          user_record_t new_user = {0};
          new_user.user_id = (pending_user_id > 0)
                                 ? pending_user_id
                                 : nv_storage_get_free_user_id();
          new_user.is_active = true;
          strncpy(new_user.qr_id, qr_str, sizeof(new_user.qr_id) - 1);
          if (nv_storage_add_user(&new_user)) {
            app_set_temp_led(LED_COLOR_GREEN, LED_MODE_SOLID, 1000);
            hal_io_buzzer_beep(2700, 100, 2);
          } else {
            app_set_temp_led(LED_COLOR_RED, LED_MODE_SOLID, 3000);
            hal_io_buzzer_beep(1000, 300, 3);
            hal_shift_reg_play_anim(ANIM_ERROR_FLASH, 3000);
          }
          pending_user_id = 0;
          input_len = 0;
        } else {
          user_record_t u;
          if (nv_storage_find_user_by_qr(qr_str, &u)) {
            nv_storage_delete_user(u.user_id);
            app_set_temp_led(LED_COLOR_GREEN, LED_MODE_SOLID, 1000);
            hal_io_buzzer_beep(2700, 100, 2);
          } else {
            app_set_temp_led(LED_COLOR_RED, LED_MODE_SOLID, 3000);
            hal_io_buzzer_beep(1000, 300, 3);
          }
        }
      }
    }
    break;
  }
  case STATE_PROGRAMMING: {
    if (elapsed_ms >= 25000) {
      app_set_state(STATE_IDLE);
      hal_io_buzzer_beep(1000, 500, 3);
      break;
    }

    char key = '\0';
    if (hal_touch_read_key(&key)) {
      ESP_LOGI(TAG, "[PROGRAMMING] Key pressed: %c", key);
      state_timer_start = esp_timer_get_time();

      if (key == '*') {
        hal_io_buzzer_beep(1000, 100, 1);
        app_set_state(STATE_IDLE);
        break;
      }

      if (prog_state == PROG_STATE_ROOT) {
        input_len = 0;
        if (config.working_mode == 3 && key != '9') {
          hal_io_buzzer_beep(1000, 200, 3);
          break;
        }
        if (key == '0') {
          prog_state = PROG_STATE_UPDATE_MASTER_CODE_1;
          ESP_LOGI(TAG, "Menu 0: Update Master Code");
          hal_io_buzzer_beep(2700, 50, 1);
        } else if (key == '1') {
          prog_state = PROG_STATE_ADD_MASTER_CARD;
          ESP_LOGI(TAG, "Menu 1: Add Master Card");
          hal_io_buzzer_beep(2700, 50, 1);
        } else if (key == '2') {
          if (config.is_online) {
            ESP_LOGW(TAG, "Menu 2: Locked (Online Mode)");
            hal_io_buzzer_beep(1000, 300, 3);
          } else {
            prog_state = PROG_STATE_ADD_USER;
            ESP_LOGI(TAG, "Menu 2: Add User");
            hal_io_buzzer_beep(2700, 50, 1);
          }
        } else if (key == '3') {
          if (config.is_online) {
            ESP_LOGW(TAG, "Menu 3: Locked (Online Mode)");
            hal_io_buzzer_beep(1000, 300, 3);
          } else {
            prog_state = PROG_STATE_DEL_USER;
            ESP_LOGI(TAG, "Menu 3: Delete User");
            hal_io_buzzer_beep(2700, 50, 1);
          }
        } else if (key == '4') {
          prog_state = PROG_STATE_RELAY;
          ESP_LOGI(TAG, "Menu 4: Relay Time");
          hal_io_buzzer_beep(2700, 50, 1);
        } else if (key == '5') {
          ESP_LOGW(TAG, "Menu 5: Access Mode is disabled");
          hal_io_buzzer_beep(1000, 300, 3);
        } else if (key == '6') {
          prog_state = PROG_STATE_WORKING_MODE;
          ESP_LOGI(TAG, "Menu 6: Working Mode");
          hal_io_buzzer_beep(2700, 50, 1);
        } else if (key == '7') {
          prog_state = PROG_STATE_DOOR_DETECTION;
          ESP_LOGI(TAG, "Menu 7: Door Detection");
          hal_io_buzzer_beep(2700, 50, 1);
        } else if (key == '8') {
          prog_state = PROG_STATE_AUDIO_VISUAL;
          ESP_LOGI(TAG, "Menu 8: Audio/Visual");
          hal_io_buzzer_beep(2700, 50, 1);
        } else if (key == '9') {
          // Menu 9 sub-menus:
          // 9→26→# or 9→34→#  : Wiegand card format
          // 9→4→#  or 9→8→#   : PIN output format
          // 9→0→#  or 9→1→#   : Parity (disable/enable)
          prog_state = PROG_STATE_WIEGAND;
          ESP_LOGI(TAG, "Menu 9: Wiegand Format");
          hal_io_buzzer_beep(2700, 50, 1);
        } else {
          hal_io_buzzer_beep(1000, 200, 1);
        }
      } else if (prog_state == PROG_STATE_UPDATE_MASTER_CODE_1) {
        if (key == '#') {
          input_buffer[input_len] = '\0';
          strncpy(temp_master_code, input_buffer, sizeof(temp_master_code) - 1);
          prog_state = PROG_STATE_UPDATE_MASTER_CODE_2;
          input_len = 0;
          hal_io_buzzer_beep(2700, 100, 1);
        } else if (input_len < 7) {
          input_buffer[input_len++] = key;
          hal_io_buzzer_beep(2700, 50, 1);
        }
      } else if (prog_state == PROG_STATE_UPDATE_MASTER_CODE_2) {
        if (key == '#') {
          input_buffer[input_len] = '\0';
          if (strcmp(input_buffer, temp_master_code) == 0) {
            strncpy(config.master_code, temp_master_code,
                    sizeof(config.master_code) - 1);
            nv_storage_set_config(&config);
            app_set_temp_led(LED_COLOR_GREEN, LED_MODE_SOLID, 1000);
            hal_io_buzzer_beep(2700, 100, 2); // 2 Rapid
          } else {
            app_set_temp_led(LED_COLOR_RED, LED_MODE_SOLID, 3000);
            hal_io_buzzer_beep(1000, 300, 3); // 3 Slow
            hal_shift_reg_play_anim(ANIM_ERROR_FLASH, 3000);
          }
          prog_state = PROG_STATE_ROOT;
        } else if (input_len < 7) {
          input_buffer[input_len++] = key;
          hal_io_buzzer_beep(2700, 50, 1);
        }
      } else if (prog_state == PROG_STATE_ADD_MASTER_CARD) {
        if (key == '#') {
          if (input_len == 0) {
            prog_state = PROG_STATE_ROOT;
          } else {
            input_buffer[input_len] = '\0';
            if (input_len >= 8) {
              // Add Auto ID Master with card number
              if (nv_storage_add_master_credential(0, input_buffer)) {
                ESP_LOGI(TAG, "DEBUG: Master ADDED (AutoID) - Card: %s",
                         input_buffer);
                app_set_temp_led(LED_COLOR_GREEN, LED_MODE_SOLID, 1000);
                hal_io_buzzer_beep(2700, 100, 2);
              } else {
                ESP_LOGW(TAG, "DEBUG: Failed to add Master (AutoID) - Card: %s",
                         input_buffer);
                app_set_temp_led(LED_COLOR_RED, LED_MODE_SOLID, 3000);
                hal_io_buzzer_beep(1000, 300, 3);
                hal_shift_reg_play_anim(ANIM_ERROR_FLASH, 3000);
              }
              prog_state = PROG_STATE_ADD_MASTER_CARD;
            } else {
              // Specific ID entered
              pending_user_id = atoi(input_buffer);
              prog_state = PROG_STATE_ADD_MASTER_SPECIFIC;
              hal_io_buzzer_beep(2700, 100, 1);
            }
          }
          input_len = 0;
        } else if (input_len < 18) {
          input_buffer[input_len++] = key;
          hal_io_buzzer_beep(2700, 50, 1);
        }
      } else if (prog_state == PROG_STATE_ADD_MASTER_SPECIFIC) {
        if (key == '#') {
          if (input_len > 0) {
            input_buffer[input_len] = '\0';
            if (nv_storage_add_master_credential(pending_user_id,
                                                 input_buffer)) {
              ESP_LOGI(TAG,
                       "DEBUG: Master ADDED (SpecificID) - ID: %d, Card: %s",
                       pending_user_id, input_buffer);
              app_set_temp_led(LED_COLOR_GREEN, LED_MODE_SOLID, 1000);
              hal_io_buzzer_beep(2700, 100, 2);
            } else {
              ESP_LOGW(TAG, "DEBUG: Failed to add Master (SpecificID) - ID: %d",
                       pending_user_id);
              app_set_temp_led(LED_COLOR_RED, LED_MODE_SOLID, 3000);
              hal_io_buzzer_beep(1000, 300, 3);
              hal_shift_reg_play_anim(ANIM_ERROR_FLASH, 3000);
            }
            prog_state = PROG_STATE_ADD_MASTER_CARD;
          }
          input_len = 0;
        } else if (input_len < 18) {
          input_buffer[input_len++] = key;
          hal_io_buzzer_beep(2700, 50, 1);
        }
      } else if (prog_state == PROG_STATE_ADD_USER) {
        if (key == '#') {
          if (input_len == 0) {
            prog_state = PROG_STATE_ROOT;
          } else {
            input_buffer[input_len] = '\0';
            if (input_len >= 8) {
              // Auto ID Add Card User
              user_record_t new_user = {0};
              new_user.user_id = nv_storage_get_free_user_id(); // Auto ID
              new_user.is_active = true;
              strncpy(new_user.card_id, input_buffer,
                      sizeof(new_user.card_id) - 1);
              if (nv_storage_add_user(&new_user)) {
                ESP_LOGI(TAG,
                         "DEBUG: User ADDED (AutoID) - UserID: %d, Card: %s",
                         new_user.user_id, new_user.card_id);
                app_set_temp_led(LED_COLOR_GREEN, LED_MODE_SOLID, 1000);
                hal_io_buzzer_beep(2700, 100, 2);
              } else {
                ESP_LOGW(TAG, "DEBUG: Failed to add AutoID user (Card: %s)",
                         new_user.card_id);
                app_set_temp_led(LED_COLOR_RED, LED_MODE_SOLID, 3000);
                hal_io_buzzer_beep(1000, 300, 3);
                hal_shift_reg_play_anim(ANIM_ERROR_FLASH, 3000);
              }
              prog_state = PROG_STATE_ADD_USER;
            } else {
              // specific UserID
              pending_user_id = atoi(input_buffer);
              prog_state = PROG_STATE_ADD_USER_SPECIFIC;
              hal_io_buzzer_beep(2700, 100, 1);
            }
          }
          input_len = 0;
        } else if (input_len < 18) {
          input_buffer[input_len++] = key;
          hal_io_buzzer_beep(2700, 50, 1);
        }
      } else if (prog_state == PROG_STATE_ADD_USER_SPECIFIC) {
        if (key == '#') {
          input_buffer[input_len] = '\0';
          if (input_len >= 1 && input_len <= 3) {
            // Quantity
            pending_quantity = atoi(input_buffer);
            prog_state = PROG_STATE_ADD_USER_BLOCK;
            hal_io_buzzer_beep(2700, 100, 1);
          } else {
            user_record_t new_user = {0};
            new_user.user_id = pending_user_id;
            new_user.is_active = true;
            if (input_len >= 8) {
              strncpy(new_user.card_id, input_buffer,
                      sizeof(new_user.card_id) - 1);
            } else if (input_len >= 4 && input_len <= 6) {
              strncpy(new_user.pin, input_buffer, sizeof(new_user.pin) - 1);
            }
            if (nv_storage_add_user(&new_user)) {
              ESP_LOGI(TAG, "DEBUG: User ADDED - UserID: %d, PIN: %s, Card: %s",
                       new_user.user_id, new_user.pin[0] ? new_user.pin : "N/A",
                       new_user.card_id[0] ? new_user.card_id : "N/A");
              app_set_temp_led(LED_COLOR_GREEN, LED_MODE_SOLID, 1000);
              hal_io_buzzer_beep(2700, 100, 2);
            } else {
              ESP_LOGW(TAG, "DEBUG: Failed to add user %d", new_user.user_id);
              app_set_temp_led(LED_COLOR_RED, LED_MODE_SOLID, 3000);
              hal_io_buzzer_beep(1000, 300, 3);
            }
            prog_state = PROG_STATE_ADD_USER;
          }
          input_len = 0;
        } else if (input_len < 18) {
          input_buffer[input_len++] = key;
          hal_io_buzzer_beep(2700, 50, 1);
        }
      } else if (prog_state == PROG_STATE_ADD_USER_BLOCK) {
        if (key == '#') {
          input_buffer[input_len] = '\0';
          if (input_len >= 8) {
            uint64_t start_card = strtoull(input_buffer, NULL, 10);
            bool success = true;
            for (uint16_t i = 0; i < pending_quantity; i++) {
              user_record_t new_user = {0};
              new_user.user_id = pending_user_id + i;
              new_user.is_active = true;
              snprintf(new_user.card_id, sizeof(new_user.card_id), "%llu",
                       (unsigned long long)(start_card + i));
              if (!nv_storage_add_user(&new_user)) {
                success = false;
              }
            }
            if (success) {
              ESP_LOGI(
                  TAG,
                  "DEBUG: %d Users ADDED starting from UserID: %d, Card: %llu",
                  pending_quantity, pending_user_id,
                  (unsigned long long)start_card);
              app_set_temp_led(LED_COLOR_GREEN, LED_MODE_SOLID, 1000);
              hal_io_buzzer_beep(2700, 100, 2);
            } else {
              ESP_LOGW(TAG, "DEBUG: Failed to add some users in block!");
              app_set_temp_led(LED_COLOR_RED, LED_MODE_SOLID, 3000);
              hal_io_buzzer_beep(1000, 300, 3);
            }
          }
          input_len = 0;
          prog_state = PROG_STATE_ADD_USER;
        } else if (input_len < 18) {
          input_buffer[input_len++] = key;
          hal_io_buzzer_beep(2700, 50, 1);
        }
      } else if (prog_state == PROG_STATE_DEL_USER) {
        if (key == '#') {
          input_buffer[input_len] = '\0';
          if (strcmp(input_buffer, config.master_code) == 0) {
            if (nv_storage_delete_all_users()) {
              ESP_LOGI(TAG, "DEBUG: ALL USERS DELETED via Master Code!");
              app_set_temp_led(LED_COLOR_GREEN, LED_MODE_SOLID, 1000);
              hal_io_buzzer_beep(2700, 100, 2);
            }
          } else if (input_len >= 8) {
            user_record_t u;
            if (nv_storage_find_user_by_card(input_buffer, &u)) {
              if (nv_storage_delete_user(u.user_id)) {
                ESP_LOGI(TAG, "DEBUG: User DELETED by Card - UserID: %d",
                         u.user_id);
                app_set_temp_led(LED_COLOR_GREEN, LED_MODE_SOLID, 1000);
                hal_io_buzzer_beep(2700, 100, 2);
              }
            } else {
              ESP_LOGW(TAG, "DEBUG: Failed to delete user - Card %s not found",
                       input_buffer);
              app_set_temp_led(LED_COLOR_RED, LED_MODE_SOLID, 3000);
              hal_io_buzzer_beep(1000, 300, 3);
            }
          } else {
            uint16_t uid = atoi(input_buffer);
            if (nv_storage_delete_user(uid)) {
              ESP_LOGI(TAG, "DEBUG: User DELETED by ID - UserID: %d", uid);
              app_set_temp_led(LED_COLOR_GREEN, LED_MODE_SOLID, 1000);
              hal_io_buzzer_beep(2700, 100, 2);
            } else {
              ESP_LOGW(TAG, "DEBUG: Failed to delete user - ID %d not found",
                       uid);
              app_set_temp_led(LED_COLOR_RED, LED_MODE_SOLID, 3000);
              hal_io_buzzer_beep(1000, 300, 3);
            }
          }
          prog_state = PROG_STATE_DEL_USER;
          input_len = 0;
        } else if (input_len < 18) {
          input_buffer[input_len++] = key;
          hal_io_buzzer_beep(2700, 50, 1);
        }
      } else if (prog_state == PROG_STATE_DOOR_DETECTION) {
        if (key == '#') {
          input_buffer[input_len] = '\0';
          int val = atoi(input_buffer);
          if (val == 3) {
            config.door_open_detection = false;
            nv_storage_set_config(&config);
            app_set_temp_led(LED_COLOR_GREEN, LED_MODE_SOLID, 1000);
            hal_io_buzzer_beep(2700, 100, 2);
            prog_state = PROG_STATE_ROOT;
          } else if (val == 4) {
            prog_state = PROG_STATE_DOOR_DETECTION_VAL;
            hal_io_buzzer_beep(2700, 100, 1);
          } else {
            app_set_temp_led(LED_COLOR_RED, LED_MODE_SOLID, 3000);
            hal_io_buzzer_beep(1000, 300, 3);
            prog_state = PROG_STATE_ROOT;
          }
          input_len = 0;
        } else if (input_len < 4) {
          input_buffer[input_len++] = key;
          hal_io_buzzer_beep(2700, 50, 1);
        }
      } else if (prog_state == PROG_STATE_DOOR_DETECTION_VAL) {
        if (key == '#') {
          input_buffer[input_len] = '\0';
          int val = atoi(input_buffer);
          if (val >= 0 && val <= 3) {
            config.door_open_detection = true;
            config.alarm_time = val;
            nv_storage_set_config(&config);
            app_set_temp_led(LED_COLOR_GREEN, LED_MODE_SOLID, 1000);
            hal_io_buzzer_beep(2700, 100, 2);
          } else {
            app_set_temp_led(LED_COLOR_RED, LED_MODE_SOLID, 3000);
            hal_io_buzzer_beep(1000, 300, 3);
          }
          prog_state = PROG_STATE_ROOT;
          input_len = 0;
        } else if (input_len < 4) {
          input_buffer[input_len++] = key;
          hal_io_buzzer_beep(2700, 50, 1);
        }
      } else {
        // Dynamic Configuration Menu Processing
        if (key == '#') {
          input_buffer[input_len] = '\0';
          int val = atoi(input_buffer);
          bool ok = false;

          switch (prog_state) {
          case PROG_STATE_RELAY:
            if (val <= 99) {
              config.relay_time = val;
              ok = true;
              ESP_LOGI(TAG, "Config: Relay Time = %d", val);
            }
            break;
          case PROG_STATE_ACCESS_MODE:
            if (val <= 6) {
              config.access_mode = val;
              ok = true;
              ESP_LOGI(TAG, "Config: Access Mode = %d", val);
            }
            break;
          case PROG_STATE_WORKING_MODE:
            if (val == 1 || val == 2 || val == 3) {
              uint8_t old_mode = config.working_mode;
              config.working_mode = val;
              ok = true;
              ESP_LOGI(TAG, "Config: Working Mode = %d", val);
              
              if ((old_mode == 3 && val != 3) || (old_mode != 3 && val == 3)) {
                  nv_storage_set_config(&config);
                  ESP_LOGW(TAG, "Working Mode crossed Reader Mode boundary. Rebooting to apply network stack changes!");
                  hal_io_buzzer_beep(2700, 100, 5);
                  vTaskDelay(pdMS_TO_TICKS(1000));
                  esp_restart();
              }
            } else if (val == 8) {
              if (config.is_online)
                ok = false; // Rejected, must factory reset
              else {
                config.is_online = false;
                ok = true;
                ESP_LOGI(TAG, "Config: Switched to Offline Mode");
              }
            } else if (val == 9) {
              config.is_online = true;
              ok = true;
              ESP_LOGI(TAG, "Config: Switched to Online Mode");
            }
            break;
          case PROG_STATE_AUDIO_VISUAL:
            if (val == 0) {
              config.sound_enabled = false;
              hal_io_set_sound_enabled(false);
              ESP_LOGI(TAG, "Config: Sound Disabled");
            } else if (val == 1) {
              config.sound_enabled = true;
              hal_io_set_sound_enabled(true);
              ESP_LOGI(TAG, "Config: Sound Enabled");
            } else if (val == 2) {
              config.led_always_on = false;
              ESP_LOGI(TAG, "Config: LED Always On Disabled");
            } else if (val == 3) {
              config.led_always_on = true;
              ESP_LOGI(TAG, "Config: LED Always On Enabled");
            }
            ok = true;
            break;
          case PROG_STATE_WIEGAND:
            if (val == 26 || val == 34) {
              config.wiegand_format = val;
              ok = true;
              ESP_LOGI(TAG, "Config: Wiegand Card Format = %d-bit", val);
            } else if (val == 4 || val == 8) {
              // Entered PIN format directly
              config.pin_wiegand_format = val;
              ok = true;
              ESP_LOGI(TAG, "Config: Wiegand PIN Format = %d-bit", val);
            } else if (val == 0 || val == 1) {
              // Entered parity setting directly
              config.wiegand_parity_en = (val == 1);
              ok = true;
              ESP_LOGI(TAG, "Config: Wiegand Parity = %s", val ? "ON" : "OFF");
            }
            break;
          case PROG_STATE_WIEGAND_PIN:
            if (val == 4 || val == 8) {
              config.pin_wiegand_format = val;
              ok = true;
              ESP_LOGI(TAG, "Config: Wiegand PIN Format = %d-bit", val);
            }
            break;
          case PROG_STATE_WIEGAND_PARITY:
            if (val == 0 || val == 1) {
              config.wiegand_parity_en = (val == 1);
              ok = true;
              ESP_LOGI(TAG, "Config: Wiegand Parity = %s", val ? "ON" : "OFF");
            }
            break;
          default:
            break;
          }

          if (ok) {
            nv_storage_set_config(&config);
            if (prog_state == PROG_STATE_WORKING_MODE) {
              // Apply direction immediately
              if (config.working_mode == 2)
                hal_wiegand_set_direction(false);
              else if (config.working_mode == 3)
                hal_wiegand_set_direction(true);
              else
                hal_wiegand_set_direction(false);
            }
            app_set_temp_led(LED_COLOR_GREEN, LED_MODE_SOLID, 1000);
            hal_io_buzzer_beep(2700, 100, 2);
          } else {
            app_set_temp_led(LED_COLOR_RED, LED_MODE_SOLID, 3000);
            hal_io_buzzer_beep(1000, 300, 3);
          }
          prog_state = PROG_STATE_ROOT;
        } else if (input_len < 10) {
          input_buffer[input_len++] = key;
          hal_io_buzzer_beep(2700, 50, 1);
        }
      }
    }

    // High-Speed RF/QR Handlers
    uint32_t rfid_uid = 0;
    bool rfid_ok = false;
#if ACTIVE_HW_VERSION != HW_VERSION_QR_ONLY
    rfid_ok = mfrc522_check_card(&rfid_uid);
#endif
    bool weigand_ok = (config.working_mode != 1) && hal_wiegand_available();
    if (rfid_ok || weigand_ok) {
      hal_shift_reg_wakeup_keypad();
      uint8_t bit_len = 0;
      uint32_t card_id =
          (rfid_uid != 0) ? rfid_uid : hal_wiegand_read(&bit_len);
      state_timer_start = esp_timer_get_time();
      char card_str[24];
      snprintf(card_str, sizeof(card_str), "%lu", (unsigned long)card_id);

      if (prog_state == PROG_STATE_ADD_MASTER_CARD) {
        ESP_LOGI(TAG, "[PROG] RFID read: %s -> Add Master Card (AutoID)", card_str);
        if (nv_storage_add_master_credential(0, card_str)) {
          ESP_LOGI(TAG, "[PROG] Master Card SAVED: %s", card_str);
          app_set_temp_led(LED_COLOR_GREEN, LED_MODE_SOLID, 1000);
          hal_io_buzzer_beep(2700, 100, 2);
        } else {
          ESP_LOGW(TAG, "[PROG] Master Card FAILED (already exists or full): %s", card_str);
          app_set_temp_led(LED_COLOR_RED, LED_MODE_SOLID, 3000);
          hal_io_buzzer_beep(1000, 300, 3);
          hal_shift_reg_play_anim(ANIM_ERROR_FLASH, 3000);
        }
        prog_state = PROG_STATE_ROOT;
      } else if (prog_state == PROG_STATE_ADD_MASTER_SPECIFIC) {
        ESP_LOGI(TAG, "[PROG] RFID read: %s -> Add Master Card (ID:%d)", card_str, pending_user_id);
        if (nv_storage_add_master_credential(pending_user_id, card_str)) {
          ESP_LOGI(TAG, "[PROG] Master Card SAVED: ID=%d, Card=%s", pending_user_id, card_str);
          app_set_temp_led(LED_COLOR_GREEN, LED_MODE_SOLID, 1000);
          hal_io_buzzer_beep(2700, 100, 2);
        } else {
          ESP_LOGW(TAG, "[PROG] Master Card FAILED: ID=%d, Card=%s", pending_user_id, card_str);
          app_set_temp_led(LED_COLOR_RED, LED_MODE_SOLID, 3000);
          hal_io_buzzer_beep(1000, 300, 3);
          hal_shift_reg_play_anim(ANIM_ERROR_FLASH, 3000);
        }
        prog_state = PROG_STATE_ROOT;
      } else if (prog_state == PROG_STATE_ADD_USER ||
                 prog_state == PROG_STATE_ADD_USER_SPECIFIC) {
        if (nv_storage_is_master_credential(card_str)) {
          ESP_LOGI(TAG, "[PROG] Master card scanned in Add User -> Exit to IDLE");
          app_set_state(STATE_IDLE);
        } else {
          user_record_t u;
          if (nv_storage_find_user_by_card(card_str, &u)) {
            ESP_LOGW(TAG, "[PROG] Card %s ALREADY registered to UserID:%d", card_str, u.user_id);
            app_set_temp_led(LED_COLOR_RED, LED_MODE_SOLID, 3000);
            hal_io_buzzer_beep(1000, 300, 3);
            hal_shift_reg_play_anim(ANIM_ERROR_FLASH, 3000);
          } else {
            user_record_t new_user = {0};
            if (prog_state == PROG_STATE_ADD_USER_SPECIFIC) {
              new_user.user_id = pending_user_id;
            } else {
              new_user.user_id = nv_storage_get_free_user_id();
            }
            new_user.is_active = true;
            strncpy(new_user.card_id, card_str, sizeof(new_user.card_id) - 1);
            ESP_LOGI(TAG, "[PROG] RFID read: %s -> Add User (UserID:%d)", card_str, new_user.user_id);
            if (nv_storage_add_user(&new_user)) {
              ESP_LOGI(TAG, "[PROG] User SAVED: UserID=%d, Card=%s", new_user.user_id, card_str);
              app_set_temp_led(LED_COLOR_GREEN, LED_MODE_SOLID, 1000);
              hal_io_buzzer_beep(2700, 100, 2);
            } else {
              ESP_LOGW(TAG, "[PROG] User FAILED to save: UserID=%d, Card=%s", new_user.user_id, card_str);
              app_set_temp_led(LED_COLOR_RED, LED_MODE_SOLID, 3000);
              hal_io_buzzer_beep(1000, 300, 3);
            }
          }
        }
      } else if (prog_state == PROG_STATE_DEL_USER) {
        ESP_LOGI(TAG, "[PROG] RFID read: %s -> Delete User", card_str);
        if (nv_storage_is_master_credential(card_str)) {
          int32_t master_id = nv_storage_get_master_id(card_str);
          ESP_LOGW(TAG, "[PROG] Master card (ID: %d) -> Delete ALL users", (int)master_id);
          if (nv_storage_delete_all_users()) {
            ESP_LOGI(TAG, "[PROG] ALL users DELETED");
            app_set_temp_led(LED_COLOR_GREEN, LED_MODE_SOLID, 1000);
            hal_io_buzzer_beep(2700, 100, 2);
          }
        } else {
          user_record_t u;
          if (nv_storage_find_user_by_card(card_str, &u)) {
            nv_storage_delete_user(u.user_id);
            ESP_LOGI(TAG, "[PROG] User DELETED: UserID=%d, Card=%s", u.user_id, card_str);
            app_set_temp_led(LED_COLOR_GREEN, LED_MODE_SOLID, 1000);
            hal_io_buzzer_beep(2700, 100, 2);
          } else {
            ESP_LOGW(TAG, "[PROG] Delete FAILED: Card %s not found in DB", card_str);
            app_set_temp_led(LED_COLOR_RED, LED_MODE_SOLID, 3000);
            hal_io_buzzer_beep(1000, 300, 3);
          }
        }
      }
    }

    char qr_str[64];
    if (hal_qr_read(qr_str, sizeof(qr_str))) {
      state_timer_start = esp_timer_get_time();
      hal_shift_reg_wakeup_keypad();
      if (prog_state == PROG_STATE_ADD_MASTER_CARD) {
        ESP_LOGI(TAG, "[PROG] QR read: %s -> Add Master Card (AutoID)", qr_str);
        if (nv_storage_add_master_credential(0, qr_str)) {
          ESP_LOGI(TAG, "[PROG] Master QR SAVED: %s", qr_str);
          app_set_temp_led(LED_COLOR_GREEN, LED_MODE_SOLID, 1000);
          hal_io_buzzer_beep(2700, 100, 2);
        } else {
          ESP_LOGW(TAG, "[PROG] Master QR FAILED (already exists or full): %s", qr_str);
          app_set_temp_led(LED_COLOR_RED, LED_MODE_SOLID, 3000);
          hal_io_buzzer_beep(1000, 300, 3);
          hal_shift_reg_play_anim(ANIM_ERROR_FLASH, 3000);
        }
        prog_state = PROG_STATE_ROOT;
      } else if (prog_state == PROG_STATE_ADD_MASTER_SPECIFIC) {
        ESP_LOGI(TAG, "[PROG] QR read: %s -> Add Master Card (ID:%d)", qr_str, pending_user_id);
        if (nv_storage_add_master_credential(pending_user_id, qr_str)) {
          ESP_LOGI(TAG, "[PROG] Master QR SAVED: ID=%d, QR=%s", pending_user_id, qr_str);
          app_set_temp_led(LED_COLOR_GREEN, LED_MODE_SOLID, 1000);
          hal_io_buzzer_beep(2700, 100, 2);
        } else {
          ESP_LOGW(TAG, "[PROG] Master QR FAILED: ID=%d, QR=%s", pending_user_id, qr_str);
          app_set_temp_led(LED_COLOR_RED, LED_MODE_SOLID, 3000);
          hal_io_buzzer_beep(1000, 300, 3);
          hal_shift_reg_play_anim(ANIM_ERROR_FLASH, 3000);
        }
        prog_state = PROG_STATE_ROOT;
      } else if (prog_state == PROG_STATE_ADD_USER ||
                 prog_state == PROG_STATE_ADD_USER_SPECIFIC) {
        if (nv_storage_is_master_credential(qr_str)) {
          ESP_LOGI(TAG, "[PROG] Master QR scanned in Add User -> Exit to IDLE");
          app_set_state(STATE_IDLE);
        } else {
          user_record_t u;
          if (nv_storage_find_user_by_qr(qr_str, &u)) {
            ESP_LOGW(TAG, "[PROG] QR %s ALREADY registered to UserID:%d", qr_str, u.user_id);
            app_set_temp_led(LED_COLOR_RED, LED_MODE_SOLID, 3000);
            hal_io_buzzer_beep(1000, 300, 3);
            hal_shift_reg_play_anim(ANIM_ERROR_FLASH, 3000);
          } else {
            user_record_t new_user = {0};
            if (prog_state == PROG_STATE_ADD_USER_SPECIFIC) {
              new_user.user_id = pending_user_id;
            } else {
              new_user.user_id = nv_storage_get_free_user_id();
            }
            new_user.is_active = true;
            strncpy(new_user.qr_id, qr_str, sizeof(new_user.qr_id) - 1);
            ESP_LOGI(TAG, "[PROG] QR read: %s -> Add User (UserID:%d)", qr_str, new_user.user_id);
            if (nv_storage_add_user(&new_user)) {
              ESP_LOGI(TAG, "[PROG] User SAVED: UserID=%d, QR=%s", new_user.user_id, qr_str);
              app_set_temp_led(LED_COLOR_GREEN, LED_MODE_SOLID, 1000);
              hal_io_buzzer_beep(2700, 100, 2);
            } else {
              ESP_LOGW(TAG, "[PROG] User FAILED to save: UserID=%d, QR=%s", new_user.user_id, qr_str);
              app_set_temp_led(LED_COLOR_RED, LED_MODE_SOLID, 3000);
              hal_io_buzzer_beep(1000, 300, 3);
            }
          }
        }
      } else if (prog_state == PROG_STATE_DEL_USER) {
        ESP_LOGI(TAG, "[PROG] QR read: %s -> Delete User", qr_str);
        if (nv_storage_is_master_credential(qr_str)) {
          ESP_LOGW(TAG, "[PROG] Master QR -> Delete ALL users");
          if (nv_storage_delete_all_users()) {
            ESP_LOGI(TAG, "[PROG] ALL users DELETED");
            app_set_temp_led(LED_COLOR_GREEN, LED_MODE_SOLID, 1000);
            hal_io_buzzer_beep(2700, 100, 2);
          }
        } else {
          user_record_t u;
          if (nv_storage_find_user_by_qr(qr_str, &u)) {
            nv_storage_delete_user(u.user_id);
            ESP_LOGI(TAG, "[PROG] User DELETED: UserID=%d, QR=%s", u.user_id, qr_str);
            app_set_temp_led(LED_COLOR_GREEN, LED_MODE_SOLID, 1000);
            hal_io_buzzer_beep(2700, 100, 2);
          } else {
            ESP_LOGW(TAG, "[PROG] Delete FAILED: QR '%s' not found in DB", qr_str);
            app_set_temp_led(LED_COLOR_RED, LED_MODE_SOLID, 3000);
            hal_io_buzzer_beep(1000, 300, 3);
          }
        }
      }
    }

    break;
  }
  case STATE_DOOR_OPEN:
    if (elapsed_ms >= (config.relay_time * 1000)) {
      hal_io_relay_set(false);
      app_set_state(STATE_IDLE);
    }
    break;
  case STATE_ALARM:
    // Simplified alarm clear
    if (config.alarm_time > 0 && elapsed_ms >= (config.alarm_time * 60000)) {
      app_set_state(STATE_IDLE);
    }
    break;
  default:
    break;
  }
}
