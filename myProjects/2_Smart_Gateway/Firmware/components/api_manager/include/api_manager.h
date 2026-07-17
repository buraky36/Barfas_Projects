#ifndef API_MANAGER_H
#define API_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define API_BASE_URL "https://api.onloi.com"

typedef struct {
    char mqtt_uri[64];
    int mqtt_port;
    char mqtt_username[64];
    char mqtt_password[64];
    char device_id[32];
    char auth_token[64]; // Returned gatewayAuthToken
} api_claim_response_t;

/**
 * @brief Post to /v1/gateways/claim to register the gateway and get MQTT credentials
 * 
 * @param device_id Optional: The device ID if known (or MAC derived)
 * @param device_code Model code (e.g., "OKGWX1")
 * @param out_response Struct to hold the parsed response
 * @return esp_err_t ESP_OK on success
 */
esp_err_t api_manager_claim_gateway(const char *device_id, const char *device_code, api_claim_response_t *out_response);

/**
 * @brief Post to /v1/gateways/{id}/local-key to sync the Gateway's local key
 * 
 * @param device_id Gateway's device ID
 * @param auth_token Gateway's auth token
 * @param local_key_b64 Base64 encoded local key
 * @return esp_err_t ESP_OK on success
 */
esp_err_t api_manager_sync_local_key(const char *device_id, const char *auth_token, const char *local_key_b64);

/**
 * @brief Fetch a sub-device's local key from the server
 * 
 * @param sub_device_id The ID of the lock/sensor
 * @param gateway_auth_token Gateway's auth token for authorization
 * @param out_local_key_b64 Buffer to hold the fetched base64 local key
 * @param out_len Size of the output buffer
 * @return esp_err_t ESP_OK on success
 */
esp_err_t api_manager_fetch_device_key(const char *sub_device_id, const char *gateway_auth_token, char *out_local_key_b64, size_t out_len);

#ifdef __cplusplus
}
#endif

#endif // API_MANAGER_H
