#include "nvs_manager.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"


static const char *TAG = "NVS_MANAGER";
#define STORAGE_NAMESPACE "storage"
#define KEY_FACTORY_TESTED "factory_tested"

esp_err_t nvs_manager_init(void) {
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
      err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    // NVS partition was truncated and needs to be erased
    // Retry nvs_flash_init
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);
  return err;
}

bool nvs_manager_is_factory_tested(void) {
  nvs_handle_t my_handle;
  esp_err_t err;
  uint8_t tested = 0; // Default false

  err = nvs_open(STORAGE_NAMESPACE, NVS_READONLY, &my_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
    return false;
  }

  err = nvs_get_u8(my_handle, KEY_FACTORY_TESTED, &tested);
  switch (err) {
  case ESP_OK:
    ESP_LOGI(TAG, "Factory tested = %d", tested);
    break;
  case ESP_ERR_NVS_NOT_FOUND:
    ESP_LOGI(TAG, "Factory tested flag is not initialized yet!");
    tested = 0;
    break;
  default:
    ESP_LOGE(TAG, "Error (%s) reading!", esp_err_to_name(err));
  }

  nvs_close(my_handle);
  return (tested == 1);
}

esp_err_t nvs_manager_set_factory_tested(bool passed) {
  nvs_handle_t my_handle;
  esp_err_t err;

  err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
  if (err != ESP_OK)
    return err;

  uint8_t val = passed ? 1 : 0;
  err = nvs_set_u8(my_handle, KEY_FACTORY_TESTED, val);
  if (err != ESP_OK) {
    nvs_close(my_handle);
    return err;
  }

  err = nvs_commit(my_handle);
  nvs_close(my_handle);
  return err;
}

uint16_t nvs_manager_get_auth_interval(void) {
  // Stub
  return 30;
}

uint16_t nvs_manager_get_max_failed_attempts(void) {
  // Stub
  return 3;
}
