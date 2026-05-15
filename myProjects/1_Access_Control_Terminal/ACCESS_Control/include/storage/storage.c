#include "storage.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include <stdio.h>
#include <string.h>
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "STORAGE";
static const char *BASE_PATH = "/spiffs";
static const char *PARTITION_LABEL = "storage"; // Must match partition.csv

bool storage_init(void) {
  ESP_LOGI(TAG, "Initializing SPIFFS...");

  esp_vfs_spiffs_conf_t conf = {.base_path = BASE_PATH,
                                .partition_label = PARTITION_LABEL,
                                .max_files = 5,
                                .format_if_mount_failed = true};

  // Use settings defined above to initialize and mount SPIFFS filesystem.
  // Note: esp_vfs_spiffs_register is an all-in-one convenience function.
  esp_err_t ret = esp_vfs_spiffs_register(&conf);

  if (ret != ESP_OK) {
    if (ret == ESP_FAIL) {
      ESP_LOGE(TAG, "Failed to mount or format filesystem");
    } else if (ret == ESP_ERR_NOT_FOUND) {
      ESP_LOGE(TAG, "Failed to find SPIFFS partition");
    } else {
      ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
    }
    return false;
  }

  size_t total = 0, used = 0;
  ret = esp_spiffs_info(conf.partition_label, &total, &used);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)",
             esp_err_to_name(ret));
    return false;
  } else {
    ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
  }

  return true;
}

void storage_format(void) {
  ESP_LOGW(TAG, "Formatting SPIFFS partition...");
  esp_spiffs_format(PARTITION_LABEL);
  ESP_LOGI(TAG, "Format complete.");
}

bool storage_append_log(const char *log_entry) {
  if (log_entry == NULL)
    return false;

  char filepath[64];
  snprintf(filepath, sizeof(filepath), "%s/access_log.txt", BASE_PATH);

  FILE *f = fopen(filepath, "a");
  if (f == NULL) {
    ESP_LOGE(TAG, "Failed to open log file for appending");
    return false;
  }

  fprintf(f, "%s\n", log_entry);
  fclose(f);
  return true;
}

bool storage_db_check_card(uint32_t card_uid) {
  // Basic mock implementation of searching a local database file.
  // In production, you might read line by line or use a binary/JSON format.

  char filepath[64];
  snprintf(filepath, sizeof(filepath), "%s/users.csv", BASE_PATH);

  FILE *f = fopen(filepath, "r");
  if (f == NULL) {
    ESP_LOGW(TAG, "Users DB not found or failed to open");
    return false; // Safely fail closed
  }

  char line[128];
  bool found = false;
  while (fgets(line, sizeof(line), f) != NULL) {
    uint32_t db_uid;
    // Simple CSV format: UID,Name,AccessLevel
    if (sscanf(line, "%lu,", &db_uid) == 1) {
      if (db_uid == card_uid) {
        found = true;
        break;
      }
    }
  }

  fclose(f);
  return found;
}

bool storage_write_string(const char *key, const char *value) {
  nvs_handle_t my_handle;
  esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
  if (err != ESP_OK) {
      ESP_LOGE(TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
      return false;
  }
  
  err = nvs_set_str(my_handle, key, value);
  if (err == ESP_OK) {
      err = nvs_commit(my_handle);
  } else {
      ESP_LOGE(TAG, "Error setting NVS string: %s", esp_err_to_name(err));
  }
  nvs_close(my_handle);
  return (err == ESP_OK);
}
