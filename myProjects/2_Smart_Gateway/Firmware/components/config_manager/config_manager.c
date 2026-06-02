#include <string.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "config_manager.h"

static const char *TAG = "CONFIG_MGR";
static const char *NVS_NAMESPACE = "storage";
static const char *NVS_KEY = "gw_config";

esp_err_t config_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing NVS Flash...");
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS Flash needs format. Erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "NVS Flash initialized successfully.");
    } else {
        ESP_LOGE(TAG, "Failed to initialize NVS Flash: %s", esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t config_manager_load(gateway_config_t *config)
{
    nvs_handle_t handle;
    esp_err_t err;

    ESP_LOGI(TAG, "Loading configuration from NVS...");
    memset(config, 0, sizeof(gateway_config_t));

    err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Could not open NVS namespace (might be empty/first boot): %s", esp_err_to_name(err));
        config->is_configured = false;
        return err;
    }

    size_t required_size = sizeof(gateway_config_t);
    err = nvs_get_blob(handle, NVS_KEY, config, &required_size);
    nvs_close(handle);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Configuration loaded successfully:");
        ESP_LOGI(TAG, "  - Configured: %s", config->is_configured ? "Yes" : "No");
        ESP_LOGI(TAG, "  - Wi-Fi SSID: %s", config->wifi_ssid);
        ESP_LOGI(TAG, "  - MQTT URI  : %s", config->mqtt_uri);
        ESP_LOGI(TAG, "  - MQTT Port : %d", config->mqtt_port);
        ESP_LOGI(TAG, "  - Device ID : %s", config->device_id);
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Configuration blob not found in NVS (first run).");
        config->is_configured = false;
    } else {
        ESP_LOGE(TAG, "Error reading configuration blob: %s", esp_err_to_name(err));
    }

    return err;
}

esp_err_t config_manager_save(const gateway_config_t *config)
{
    nvs_handle_t handle;
    esp_err_t err;

    ESP_LOGI(TAG, "Saving configuration to NVS...");
    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not open NVS namespace for writing: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_blob(handle, NVS_KEY, config, sizeof(gateway_config_t));
    if (err == ESP_OK) {
        err = nvs_commit(handle);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Configuration saved and committed successfully.");
        } else {
            ESP_LOGE(TAG, "Failed to commit NVS changes: %s", esp_err_to_name(err));
        }
    } else {
        ESP_LOGE(TAG, "Failed to write NVS config blob: %s", esp_err_to_name(err));
    }

    nvs_close(handle);
    return err;
}

esp_err_t config_manager_reset(void)
{
    nvs_handle_t handle;
    esp_err_t err;

    ESP_LOGW(TAG, "Resetting/Erasıng gateway configuration in NVS...");
    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not open NVS namespace: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_erase_key(handle, NVS_KEY);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Configuration reset successfully.");
        } else {
            ESP_LOGE(TAG, "Failed to commit configuration erase: %s", esp_err_to_name(err));
        }
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "No configuration found to erase.");
        err = ESP_OK; // Safe to ignore
    } else {
        ESP_LOGE(TAG, "Failed to erase configuration key: %s", esp_err_to_name(err));
    }

    nvs_close(handle);
    return err;
}
