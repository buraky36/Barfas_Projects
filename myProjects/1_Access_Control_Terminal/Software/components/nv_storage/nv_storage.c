#include "nv_storage.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "NV_STORAGE";
static const char *NAMESPACE_CFG = "config";
static const char *NAMESPACE_USERS = "users";

static uint8_t user_active_map[1250]; // 1250 bytes * 8 bits = 10000 bits. Bit 1 represents UserID 1, etc.

static void nv_storage_save_active_map(void) {
    nvs_handle_t handle;
    if (nvs_open(NAMESPACE_CFG, NVS_READWRITE, &handle) == ESP_OK) {
        nvs_set_blob(handle, "usr_map", user_active_map, sizeof(user_active_map));
        nvs_commit(handle);
        nvs_close(handle);
    }
}

void nv_storage_init(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    ESP_LOGI(TAG, "NVS Initialized");

    // Ensure default config exists
    sys_config_t conf;
    nv_storage_get_config(&conf);

    // Load or rebuild active users map
    nvs_handle_t handle;
    if (nvs_open(NAMESPACE_CFG, NVS_READONLY, &handle) == ESP_OK) {
        size_t size = sizeof(user_active_map);
        err = nvs_get_blob(handle, "usr_map", user_active_map, &size);
        nvs_close(handle);
    } else {
        err = ESP_FAIL;
    }

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Active user map not found. Rebuilding from NVS... (This will take a few seconds)");
        memset(user_active_map, 0, sizeof(user_active_map));
        if (nvs_open(NAMESPACE_USERS, NVS_READONLY, &handle) == ESP_OK) {
            char key[16];
            size_t size;
            for (uint16_t i = 1; i <= 9988; i++) {
                snprintf(key, sizeof(key), "u%u", i);
                if (nvs_get_blob(handle, key, NULL, &size) == ESP_OK) {
                    user_active_map[i / 8] |= (1 << (i % 8));
                }
            }
            nvs_close(handle);
        }
        nv_storage_save_active_map();
        ESP_LOGI(TAG, "Active user map rebuilt successfully.");
    }
}

void nv_storage_get_config(sys_config_t *config) {
    // 1. Set Factory Defaults first
    memset(config, 0, sizeof(sys_config_t));
    strcpy(config->master_code, "123456");
    config->master_card_count = 0;
    config->relay_time = 5;
    config->access_mode = 6; 
    config->alarm_time = 1;
    config->sound_enabled = true;
    config->led_always_on = true;
    config->working_mode = 1; // Default to Standalone mode
    config->door_open_detection = true;
    config->wiegand_format = 34;
    config->pin_wiegand_format = 4;
    config->wiegand_parity_en = true;

    nvs_handle_t handle;
    if (nvs_open(NAMESPACE_CFG, NVS_READONLY, &handle) == ESP_OK) {
        size_t size = sizeof(sys_config_t);
        // Load only if size exactly matches
        if (nvs_get_blob(handle, "sys", config, &size) != ESP_OK) {
            ESP_LOGW(TAG, "Failed reading config, using defaults");
        }
        nvs_close(handle);
    }
}

void nv_storage_set_config(sys_config_t *config) {
    nvs_handle_t handle;
    if (nvs_open(NAMESPACE_CFG, NVS_READWRITE, &handle) == ESP_OK) {
        nvs_set_blob(handle, "sys", config, sizeof(sys_config_t));
        nvs_commit(handle);
        nvs_close(handle);
    }
}

bool nv_storage_add_user(const user_record_t *user) {
    nvs_handle_t handle;
    if (nvs_open(NAMESPACE_USERS, NVS_READWRITE, &handle) != ESP_OK) return false;
    
    char key[16];
    snprintf(key, sizeof(key), "u%u", user->user_id);
    
    esp_err_t err = nvs_set_blob(handle, key, user, sizeof(user_record_t));
    if (err == ESP_OK) {
        nvs_commit(handle);
        user_active_map[user->user_id / 8] |= (1 << (user->user_id % 8));
        nv_storage_save_active_map();
    }
    nvs_close(handle);
    return err == ESP_OK;
}

bool nv_storage_get_user(uint16_t user_id, user_record_t *user_out) {
    nvs_handle_t handle;
    if (nvs_open(NAMESPACE_USERS, NVS_READONLY, &handle) != ESP_OK) return false;
    
    char key[16];
    snprintf(key, sizeof(key), "u%u", user_id);
    
    size_t size = sizeof(user_record_t);
    esp_err_t err = nvs_get_blob(handle, key, user_out, &size);
    nvs_close(handle);
    return err == ESP_OK;
}

bool nv_storage_delete_user(uint16_t user_id) {
    nvs_handle_t handle;
    if (nvs_open(NAMESPACE_USERS, NVS_READWRITE, &handle) != ESP_OK) return false;
    
    char key[16];
    snprintf(key, sizeof(key), "u%u", user_id);
    
    esp_err_t err = nvs_erase_key(handle, key);
    if (err == ESP_OK || err == ESP_ERR_NVS_NOT_FOUND) {
        nvs_commit(handle);
        user_active_map[user_id / 8] &= ~(1 << (user_id % 8));
        nv_storage_save_active_map();
        err = ESP_OK;
    }
    nvs_close(handle);
    return err == ESP_OK;
}

bool nv_storage_delete_all_users(void) {
    nvs_handle_t handle;
    if (nvs_open(NAMESPACE_USERS, NVS_READWRITE, &handle) != ESP_OK) return false;
    esp_err_t err = nvs_erase_all(handle);
    if (err == ESP_OK) {
        nvs_commit(handle);
        memset(user_active_map, 0, sizeof(user_active_map));
        nv_storage_save_active_map();
    }
    nvs_close(handle);
    return err == ESP_OK;
}

bool nv_storage_find_user_by_pin(const char *input_pin_buffer, user_record_t *out_user) {
    nvs_handle_t handle;
    if (nvs_open(NAMESPACE_USERS, NVS_READONLY, &handle) != ESP_OK) return false;
    
    char key[16];
    size_t size = sizeof(user_record_t);
    bool found = false;
    
    for (uint16_t i = 1; i <= 9988; i++) {
        if (!(user_active_map[i / 8] & (1 << (i % 8)))) continue; // Skip if bit is not set

        snprintf(key, sizeof(key), "u%u", i);
        size = sizeof(user_record_t);
        if (nvs_get_blob(handle, key, out_user, &size) == ESP_OK) {
            if (out_user->is_active && strlen(out_user->pin) >= 4) {
                if (strstr(input_pin_buffer, out_user->pin) != NULL) {
                    found = true;
                    break;
                }
            }
        }
    }
    nvs_close(handle);
    return found;
}

bool nv_storage_find_user_by_card(const char *card_id_str, user_record_t *out_user) {
    nvs_handle_t handle;
    if (nvs_open(NAMESPACE_USERS, NVS_READONLY, &handle) != ESP_OK) return false;
    
    char key[16];
    size_t size = sizeof(user_record_t);
    bool found = false;
    
    for (uint16_t i = 1; i <= 9988; i++) {
        if (!(user_active_map[i / 8] & (1 << (i % 8)))) continue;

        snprintf(key, sizeof(key), "u%u", i);
        size = sizeof(user_record_t);
        if (nvs_get_blob(handle, key, out_user, &size) == ESP_OK) {
            if (out_user->is_active && strcmp(out_user->card_id, card_id_str) == 0) {
                found = true;
                break;
            }
        }
    }
    nvs_close(handle);
    return found;
}

bool nv_storage_find_user_by_qr(const char *qr_str, user_record_t *out_user) {
    nvs_handle_t handle;
    if (nvs_open(NAMESPACE_USERS, NVS_READONLY, &handle) != ESP_OK) return false;
    
    char key[16];
    size_t size = sizeof(user_record_t);
    bool found = false;
    
    for (uint16_t i = 1; i <= 9988; i++) {
        if (!(user_active_map[i / 8] & (1 << (i % 8)))) continue;

        snprintf(key, sizeof(key), "u%u", i);
        size = sizeof(user_record_t);
        if (nvs_get_blob(handle, key, out_user, &size) == ESP_OK) {
            if (out_user->is_active && (strcmp(out_user->qr_id, qr_str) == 0 || strcmp(out_user->card_id, qr_str) == 0)) {
                found = true;
                break;
            }
        }
    }
    nvs_close(handle);
    return found;
}

bool nv_storage_is_master_credential(const char *cred) {
    if (cred == NULL || strlen(cred) == 0) return false;
    
    sys_config_t conf;
    nv_storage_get_config(&conf);
    
    for (uint8_t i = 0; i < conf.master_card_count; i++) {
        if (strcmp(conf.master_cards[i], cred) == 0) {
            return true;
        }
    }
    return false;
}

int32_t nv_storage_get_master_id(const char *cred) {
    if (cred == NULL || strlen(cred) == 0) return -1;
    
    sys_config_t conf;
    nv_storage_get_config(&conf);
    
    for (uint8_t i = 0; i < conf.master_card_count; i++) {
        if (strcmp(conf.master_cards[i], cred) == 0) {
            return conf.master_ids[i];
        }
    }
    return -1;
}

bool nv_storage_add_master_credential(uint16_t id, const char *cred) {
    if (cred == NULL || strlen(cred) == 0) return false;

    sys_config_t conf;
    nv_storage_get_config(&conf);

    for (uint8_t i = 0; i < conf.master_card_count; i++) {
        if (strcmp(conf.master_cards[i], cred) == 0) {
            return true;
        }
    }

    if (conf.master_card_count >= 10) return false;

    uint16_t new_id = id;
    if (new_id == 0) {
        new_id = 10000;
        bool found = true;
        while (found && new_id <= 10010) {
            found = false;
            for (uint8_t i = 0; i < conf.master_card_count; i++) {
                if (conf.master_ids[i] == new_id) {
                    found = true;
                    new_id++;
                    break;
                }
            }
        }
        if (new_id > 10010) return false;
    }

    strncpy(conf.master_cards[conf.master_card_count], cred, sizeof(conf.master_cards[0]) - 1);
    conf.master_cards[conf.master_card_count][sizeof(conf.master_cards[0]) - 1] = '\0';
    conf.master_ids[conf.master_card_count] = new_id;
    conf.master_card_count++;

    nv_storage_set_config(&conf);
    return true;
}

void nv_storage_clear_master_credentials(void) {
    sys_config_t conf;
    nv_storage_get_config(&conf);
    conf.master_card_count = 0;
    memset(conf.master_cards, 0, sizeof(conf.master_cards));
    memset(conf.master_ids, 0, sizeof(conf.master_ids));
    nv_storage_set_config(&conf);
}

bool nv_storage_save_api_keys(const char *device_key, const char *pair_key) {
    nvs_handle_t handle;
    if (nvs_open(NAMESPACE_CFG, NVS_READWRITE, &handle) != ESP_OK) return false;
    if (device_key) nvs_set_str(handle, "dev_k", device_key);
    if (pair_key) nvs_set_str(handle, "pair_k", pair_key);
    esp_err_t err = nvs_commit(handle);
    nvs_close(handle);
    return err == ESP_OK;
}

bool nv_storage_get_api_keys(char *device_key, char *pair_key) {
    nvs_handle_t handle;
    if (nvs_open(NAMESPACE_CFG, NVS_READONLY, &handle) != ESP_OK) return false;
    size_t size;
    if (device_key) { size = 64; nvs_get_str(handle, "dev_k", device_key, &size); }
    if (pair_key) { size = 64; nvs_get_str(handle, "pair_k", pair_key, &size); }
    nvs_close(handle);
    return true;
}

uint16_t nv_storage_get_free_user_id(void) {
    for (uint16_t i = 1; i <= 9988; i++) {
        if (!(user_active_map[i / 8] & (1 << (i % 8)))) {
            return i;
        }
    }
    return 0; // Full
}
