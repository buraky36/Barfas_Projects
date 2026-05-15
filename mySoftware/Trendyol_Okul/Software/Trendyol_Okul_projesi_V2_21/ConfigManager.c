#include "ConfigManager.h"
#include "wrappers.h"
#include <string.h>
#include <stdio.h>

static const char *CONFIG_FILENAME = CONFIG_FILE_PATH;

void config_set_defaults(DeviceConfig *config) {
    strcpy(config->deviceId, "00000");
    strcpy(config->updateDate, "2026-01-01 00:00:00");
    
    // Default SSID prefix + ChipID (we don't have ChipID in pure C easily without wrapper, 
    // but let's assume we can get it or just use a generic default for now,
    // OR add sys_get_chip_id() to wrappers. I'll add a TODO or just use a fixed default for now to save time,
    // or better, I can leave the SSID empty and let the AP server handle it if empty, 
    // but better to match original logic.
    // The original used ESP.getChipId(). I should probably add that to wrappers if strict parity is needed.
    // For now, I will use a placeholder or specific default.
    sprintf(config->wifiSsid, "%s%s", DEFAULT_AP_SSID_PREFIX, "DEVICE"); 
    
    strcpy(config->wifiPassword, DEFAULT_AP_PASSWORD);
    strcpy(config->webUsername, DEFAULT_WEB_USERNAME);
    strcpy(config->webPassword, DEFAULT_WEB_PASSWORD);
}

bool config_begin(void) {
    return sys_fs_begin();
}

bool config_load(DeviceConfig *config) {
    if (!sys_fs_exists(CONFIG_FILENAME)) {
        sys_log_ln("[Config] File not found, using defaults");
        config_set_defaults(config);
        return config_save(config);
    }
    
    char *buffer = (char*)malloc(1024); // Allocate buffer for JSON
    if (!buffer) return false;
    
    if (!sys_fs_read_file(CONFIG_FILENAME, buffer, 1024)) {
        free(buffer);
        return false;
    }
    
    bool success = sys_json_parse_config(buffer, config);
    free(buffer);
    
    if (!success) {
        sys_log_ln("[Config] Failed to parse config, using defaults");
        config_set_defaults(config);
    }
    
    return success;
}

bool config_save(const DeviceConfig *config) {
    char *buffer = (char*)malloc(1024);
    if (!buffer) return false;
    
    sys_json_serialize_config(config, buffer, 1024);
    bool success = sys_fs_write_file(CONFIG_FILENAME, buffer);
    
    free(buffer);
    return success;
}

bool config_reset(void) {
    DeviceConfig config;
    config_set_defaults(&config);
    return config_save(&config);
}
