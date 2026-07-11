#include "ota_manager.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_crt_bundle.h"
#include "app_state_machine.h"

static const char *TAG = "OTA_MGR";

static void ota_task(void *pvParameter) {
    char *url = (char *)pvParameter;
    ESP_LOGI(TAG, "Starting OTA from URL: %s", url);

    app_set_state(STATE_OTA_UPDATING);

    esp_http_client_config_t config = {
        .url = url,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 15000,
        .keep_alive_enable = true,
    };

    esp_https_ota_config_t ota_config = {
        .http_config = &config,
    };

    esp_err_t ret = esp_https_ota(&ota_config);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "OTA Update successful! Rebooting in 2 seconds...");
        vTaskDelay(pdMS_TO_TICKS(2000));
        esp_restart();
    } else {
        ESP_LOGE(TAG, "OTA Update failed! Error: %s", esp_err_to_name(ret));
        app_set_state(STATE_IDLE);
    }

    free(url);
    vTaskDelete(NULL);
}

bool ota_manager_start(const char *url) {
    if (url == NULL || strlen(url) == 0) return false;

    char *url_copy = strdup(url);
    if (!url_copy) return false;

    BaseType_t ret = xTaskCreate(ota_task, "ota_task", 8192, (void *)url_copy, 5, NULL);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create OTA task");
        free(url_copy);
        return false;
    }
    return true;
}
