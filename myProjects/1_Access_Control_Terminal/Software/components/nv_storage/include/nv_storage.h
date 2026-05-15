#ifndef NV_STORAGE_H
#define NV_STORAGE_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint16_t user_id;
    bool is_active;
    char pin[8];        // 4-6 digits + null
    char card_id[24];   // 8/10/17 digits + null
    char qr_id[32];     // For QR codes
} user_record_t;

typedef struct {
    char master_code[8];
    uint8_t master_card_count;
    char master_cards[10][32]; // Accommodates RFID or QR up to 31 chars
    uint16_t master_ids[10];   // Stores corresponding Master IDs (10000-10010)
    uint8_t relay_time;
    uint8_t access_mode;
    uint8_t alarm_time;
    bool sound_enabled;
    bool led_always_on;
    uint8_t working_mode;       // Menu 6: 1=Standalone, 2=Controller, 3=Reader
    bool is_online;             // Menu 6: Online or Offline
    bool door_open_detection;   // Menu 7
    uint8_t wiegand_format;     // Menu 9
    uint8_t pin_wiegand_format; // Menu 9
    bool wiegand_parity_en;     // Menu 9
} sys_config_t;

void nv_storage_init(void);

// Config Ops
void nv_storage_get_config(sys_config_t *config);
void nv_storage_set_config(sys_config_t *config);

// API Keys (Onloi Protocol)
bool nv_storage_save_api_keys(const char *device_key, const char *pair_key);
bool nv_storage_get_api_keys(char *device_key, char *pair_key);

// Master Credential Ops
bool nv_storage_is_master_credential(const char *cred);
int32_t nv_storage_get_master_id(const char *cred);
bool nv_storage_add_master_credential(uint16_t id, const char *cred);
void nv_storage_clear_master_credentials(void);

// User Ops
bool nv_storage_add_user(const user_record_t *user);
bool nv_storage_get_user(uint16_t user_id, user_record_t *user_out);
bool nv_storage_delete_user(uint16_t user_id);
bool nv_storage_delete_all_users(void);
uint16_t nv_storage_get_free_user_id(void);
bool nv_storage_find_user_by_pin(const char *input_pin_buffer, user_record_t *out_user);
bool nv_storage_find_user_by_card(const char *card_id_str, user_record_t *out_user);
bool nv_storage_find_user_by_qr(const char *qr_str, user_record_t *out_user);

#endif
