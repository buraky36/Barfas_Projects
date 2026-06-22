#include <string.h>
#include "esp_log.h"
#include "esp_system.h"
#include "esp_https_ota.h"
#include "esp_crt_bundle.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ota_manager.h"

static const char *TAG = "OTA_MGR";

static void ota_task(void *pvParameter)
{
    char *url = (char *)pvParameter;
    ESP_LOGI(TAG, "Starting OTA update from %s", url);

    esp_http_client_config_t config = {
        .url = url,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .skip_cert_common_name_check = true,
        .keep_alive_enable = true,
    };

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
    esp_https_ota_config_t ota_config = {
        .http_config = &config,
    };
#pragma GCC diagnostic pop

    ESP_LOGI(TAG, "Attempting to download update from %s", config.url);
    esp_err_t ret = esp_https_ota(&ota_config);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "OTA Update successful! Rebooting...");
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart();
    } else {
        ESP_LOGE(TAG, "OTA Update failed! Error: %s", esp_err_to_name(ret));
    }

    free(url);
    vTaskDelete(NULL);
}

esp_err_t ota_manager_init(void)
{
    ESP_LOGI(TAG, "OTA Manager initialized.");
    return ESP_OK;
}

esp_err_t ota_manager_start_update(const char *url)
{
    if (url == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    char *url_copy = strdup(url);
    if (url_copy == NULL) {
        return ESP_ERR_NO_MEM;
    }

    xTaskCreate(&ota_task, "ota_task", 8192, url_copy, 5, NULL);
    return ESP_OK;
}
