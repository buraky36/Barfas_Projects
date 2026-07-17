#include "api_manager.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "cJSON.h"
#include <string.h>

static const char *TAG = "API_MGR";

#define HTTP_MAX_RECEIVE_BUFFER_SIZE 2048

esp_err_t api_manager_claim_gateway(const char *device_id, const char *device_code, api_claim_response_t *out_response)
{
    esp_err_t ret = ESP_FAIL;
    char url[128];
    snprintf(url, sizeof(url), "%s/v1/gateways/claim", API_BASE_URL);

    cJSON *req_json = cJSON_CreateObject();
    if (device_id && strlen(device_id) > 0) {
        cJSON_AddStringToObject(req_json, "deviceId", device_id);
    }
    cJSON_AddStringToObject(req_json, "deviceCode", device_code);
    
    char *post_data = cJSON_PrintUnformatted(req_json);
    cJSON_Delete(req_json);

    esp_http_client_config_t config = {
        .url = url,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 10000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    char *resp_buf = malloc(HTTP_MAX_RECEIVE_BUFFER_SIZE);
    memset(resp_buf, 0, HTTP_MAX_RECEIVE_BUFFER_SIZE);
    
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        int len = esp_http_client_read_response(client, resp_buf, HTTP_MAX_RECEIVE_BUFFER_SIZE - 1);
        ESP_LOGI(TAG, "Claim POST Status: %d, Length: %d", status, len);
        
        if (status == 200 || status == 201) {
            cJSON *resp_json = cJSON_Parse(resp_buf);
            if (resp_json) {
                cJSON *mqtt = cJSON_GetObjectItem(resp_json, "mqtt");
                cJSON *token = cJSON_GetObjectItem(resp_json, "gatewayAuthToken");
                cJSON *dev_id = cJSON_GetObjectItem(resp_json, "deviceId");
                
                if (mqtt && token && dev_id) {
                    strncpy(out_response->mqtt_uri, cJSON_GetStringValue(cJSON_GetObjectItem(mqtt, "uri")), sizeof(out_response->mqtt_uri)-1);
                    out_response->mqtt_port = cJSON_GetNumberValue(cJSON_GetObjectItem(mqtt, "port"));
                    strncpy(out_response->mqtt_username, cJSON_GetStringValue(cJSON_GetObjectItem(mqtt, "username")), sizeof(out_response->mqtt_username)-1);
                    strncpy(out_response->mqtt_password, cJSON_GetStringValue(cJSON_GetObjectItem(mqtt, "password")), sizeof(out_response->mqtt_password)-1);
                    strncpy(out_response->device_id, cJSON_GetStringValue(dev_id), sizeof(out_response->device_id)-1);
                    strncpy(out_response->auth_token, cJSON_GetStringValue(token), sizeof(out_response->auth_token)-1);
                    ret = ESP_OK;
                }
                cJSON_Delete(resp_json);
            }
        }
    } else {
        ESP_LOGE(TAG, "HTTP POST failed: %s", esp_err_to_name(err));
    }

    free(resp_buf);
    free(post_data);
    esp_http_client_cleanup(client);
    return ret;
}

esp_err_t api_manager_sync_local_key(const char *device_id, const char *auth_token, const char *local_key_b64)
{
    esp_err_t ret = ESP_FAIL;
    char url[256];
    snprintf(url, sizeof(url), "%s/v1/gateways/%s/local-key", API_BASE_URL, device_id);

    cJSON *req_json = cJSON_CreateObject();
    cJSON_AddStringToObject(req_json, "localKey", local_key_b64);
    char *post_data = cJSON_PrintUnformatted(req_json);
    cJSON_Delete(req_json);

    esp_http_client_config_t config = {
        .url = url,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 10000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    
    char auth_header[128];
    snprintf(auth_header, sizeof(auth_header), "Bearer %s", auth_token);
    esp_http_client_set_header(client, "Authorization", auth_header);
    
    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "Sync Key POST Status: %d", status);
        if (status == 200 || status == 201) {
            ret = ESP_OK;
        }
    } else {
        ESP_LOGE(TAG, "HTTP POST failed: %s", esp_err_to_name(err));
    }

    free(post_data);
    esp_http_client_cleanup(client);
    return ret;
}

esp_err_t api_manager_fetch_device_key(const char *sub_device_id, const char *gateway_auth_token, char *out_local_key_b64, size_t out_len)
{
    esp_err_t ret = ESP_FAIL;
    char url[256];
    snprintf(url, sizeof(url), "%s/v1/devices/%s/local-key", API_BASE_URL, sub_device_id);

    esp_http_client_config_t config = {
        .url = url,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 10000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    
    char auth_header[128];
    snprintf(auth_header, sizeof(auth_header), "Bearer %s", gateway_auth_token);
    esp_http_client_set_header(client, "Authorization", auth_header);

    char *resp_buf = malloc(HTTP_MAX_RECEIVE_BUFFER_SIZE);
    memset(resp_buf, 0, HTTP_MAX_RECEIVE_BUFFER_SIZE);
    
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        int len = esp_http_client_read_response(client, resp_buf, HTTP_MAX_RECEIVE_BUFFER_SIZE - 1);
        ESP_LOGI(TAG, "Fetch Key GET Status: %d, Length: %d", status, len);
        
        if (status == 200) {
            cJSON *resp_json = cJSON_Parse(resp_buf);
            if (resp_json) {
                cJSON *key_item = cJSON_GetObjectItem(resp_json, "localKey");
                if (key_item && cJSON_IsString(key_item)) {
                    strncpy(out_local_key_b64, cJSON_GetStringValue(key_item), out_len - 1);
                    ret = ESP_OK;
                }
                cJSON_Delete(resp_json);
            }
        }
    } else {
        ESP_LOGE(TAG, "HTTP GET failed: %s", esp_err_to_name(err));
    }

    free(resp_buf);
    esp_http_client_cleanup(client);
    return ret;
}
