#include "auth_engine.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "storage.h"      // For local DB queries
#include "wifi_manager.h" // For checking connection status
#include <stdio.h>

static const char *TAG = "AUTH_ENGINE";

void auth_engine_init(void) { ESP_LOGI(TAG, "Auth Engine Initialized."); }

auth_result_t auth_engine_validate_card(uint32_t card_uid) {
  ESP_LOGI(TAG, "Validating Card: %lu", card_uid);

  // 1. Try Remote Server if connected
  if (wifi_manager_is_connected()) {
    ESP_LOGI(TAG, "Checking remote server via Wi-Fi...");
    // Simulate network delay
    vTaskDelay(pdMS_TO_TICKS(300));

    // Simulate server response (e.g., grant if even for testing)
    if (card_uid % 2 == 0) {
      ESP_LOGI(TAG, "Server GRANTED access.");
      return AUTH_RESULT_GRANTED;
    } else {
      ESP_LOGI(TAG, "Server DENIED access.");
      return AUTH_RESULT_DENIED;
    }
  }

  // 2. Try Local Storage if offline or server timeout
  ESP_LOGW(
      TAG,
      "Device Offline or Server Timeout. Checking local storage database...");

  if (storage_db_check_card(card_uid)) {
    ESP_LOGI(TAG, "Local DB GRANTED access.");
    // Log access
    char log_buf[64];
    snprintf(log_buf, sizeof(log_buf), "Card: %lu, Access: GRANTED (LOCAL)",
             card_uid);
    storage_append_log(log_buf);
    return AUTH_RESULT_GRANTED;
  }

  ESP_LOGW(TAG, "Local DB DENIED access.");
  char log_buf[64];
  snprintf(log_buf, sizeof(log_buf), "Card: %lu, Access: DENIED (LOCAL)",
           card_uid);
  storage_append_log(log_buf);

  return AUTH_RESULT_DENIED;
}

auth_result_t auth_engine_validate_pin(uint16_t pin_code) {
  ESP_LOGI(TAG, "Validating PIN: %d", pin_code);
  // Simple mock logic
  if (pin_code == 1234) {
    return AUTH_RESULT_GRANTED;
  }
  return AUTH_RESULT_DENIED;
}
