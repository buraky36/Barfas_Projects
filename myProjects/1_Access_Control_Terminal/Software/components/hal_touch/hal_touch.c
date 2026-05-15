#include "hal_touch.h"
#include "tsm12.h"
#include "hal_shift_reg.h"
#include "hw_config.h"

// Bit index 0-11 → character mapping, one table per hardware variant.
// Bit:                                  0     1     2     3     4     5     6     7     8     9    10    11
static const char key_map_qr  [12] = {'1',  '2',  '3',  '4',  '5',  '6',  '7',  '8',  '9',  '*',  '0',  '#'}; // PIN+QR  (original)
static const char key_map_rfid[12] = {'#',  '6',  '3',  '*',  '2',  '9',  '4',  '1',  '5',  '7',  '0',  '8'}; // PIN+RFID (corrected)
static uint16_t last_mask = 0;
static char injected_key = '\0';

void hal_touch_init(void) {
    tsm12_init();
}

void hal_touch_inject(char key) {
    injected_key = key;
}

bool hal_touch_read_key(char *key_out) {
    if (hal_shift_reg_is_blocking_anim_playing()) {
        uint16_t mask;
        while (tsm12_read_keys(&mask)); // drain queue
        last_mask = 0; // reset
        injected_key = '\0';
        return false;
    }

    if (injected_key != '\0') {
        if (key_out) *key_out = injected_key;
        injected_key = '\0'; // Clear after reading
        return true;
    }

    uint16_t mask;
    if (tsm12_read_keys(&mask)) {
        if (mask != 0 && mask != last_mask) {
            // Only look at keys that JUST became active (new bits vs previous state)
            // This prevents duplicate events during slide transitions (e.g. {1} -> {1|2} -> {2})
            uint16_t new_bits = mask & ~last_mask;
            if (new_bits == 0) new_bits = mask; // Fallback: all bits already set (same keys held)
            for (int i = 0; i < 12; i++) {
                if (new_bits & (1 << i)) {
                    const char *key_map = (active_hw_version == HW_VERSION_RFID_ONLY)
                                          ? key_map_rfid : key_map_qr;
                    if (key_out) *key_out = key_map[i];
                    last_mask = mask;
                    return true;
                }
            }
        }
        last_mask = mask;
    }
    return false;
}
