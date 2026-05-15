#include "network_manager.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "state_manager.h"


static const char *TAG = "NETWORK";

esp_err_t network_manager_init(void) {
  ESP_LOGI(TAG, "Initializing Network Stack");
  // esp_netif_init();
  // esp_event_loop_create_default();
  // wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  // esp_wifi_init(&cfg);

  return ESP_OK;
}

void network_manager_start_provisioning(void) {
  ESP_LOGI(TAG, "Starting AP Mode / BLE Provisioning...");
  // esp_wifi_set_mode(WIFI_MODE_APSTA);
  // start_web_server();
  // Initialize BLE NimBLE stack if used

  // Simulate successful provisioning after a delay
  vTaskDelay(pdMS_TO_TICKS(3000));
  ESP_LOGI(TAG, "Provisioning Success! Connecting to Wi-Fi...");
  state_manager_send_event(EVENT_PROVISIONING_SUCCESS);
}

void network_manager_connect_wifi(void) {
  ESP_LOGI(TAG, "Connecting to saved Wi-Fi network...");
  // esp_wifi_start();
  // esp_wifi_connect();
}

bool network_manager_is_connected(void) {
  // Return true if IP acquired
  return false;
}
