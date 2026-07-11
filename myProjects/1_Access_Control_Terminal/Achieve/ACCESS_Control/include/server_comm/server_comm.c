#include "server_comm.h"
#include "cJSON.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <string.h>


static const char *TAG = "SERVER_COMM";

#define SERVER_URL "https://api.onloi.com/api/device/runtime"
#define NVS_NAMESPACE "sys_cfg"

static server_comm_config_t current_config;
static bool is_configured = false;

// HTTP event handler
esp_err_t _http_event_handler(esp_http_client_event_t *evt) {
  switch (evt->event_id) {
  case HTTP_EVENT_ERROR:
    break;
  case HTTP_EVENT_ON_CONNECTED:
    break;
  case HTTP_EVENT_HEADER_SENT:
    break;
  case HTTP_EVENT_ON_HEADER:
    break;
  case HTTP_EVENT_ON_DATA:
    break;
  case HTTP_EVENT_ON_FINISH:
    break;
  case HTTP_EVENT_DISCONNECTED:
    break;
  case HTTP_EVENT_REDIRECT:
    break;
  }
  return ESP_OK;
}

// Function to perform HTTP POST
static bool http_post_json(const char *json_str) {
  esp_http_client_config_t config = {
      .url = SERVER_URL,
      .event_handler = _http_event_handler,
      .method = HTTP_METHOD_POST,
      .timeout_ms = 15000,
      // Referans projede (.setInsecure) sertifika dogrulamasi kapatilmis.
      // ESP-IDF'te crt_bundle attach etmek veya transport type belirtmek gerek.
      // Eger crt bundle yoksa ve insecure isteniyorsa (Test amacli):
      .skip_cert_common_name_check = true,
  };

  esp_http_client_handle_t client = esp_http_client_init(&config);
  if (client == NULL) {
    ESP_LOGE(TAG, "Failed to initialize HTTP client");
    return false;
  }

  esp_http_client_set_header(client, "Content-Type", "application/json");
  esp_http_client_set_post_field(client, json_str, strlen(json_str));

  esp_err_t err = esp_http_client_perform(client);
  bool status = false;

  if (err == ESP_OK) {
    int status_code = esp_http_client_get_status_code(client);
    ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %lld", status_code,
             esp_http_client_get_content_length(client));
    if (status_code == 200) {
      status = true;
    }
  } else {
    ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
  }

  esp_http_client_cleanup(client);
  return status;
}

// Ortak JSON temelini olusturan yardimci fonksiyon
static cJSON *create_base_json(void) {
  cJSON *root = cJSON_CreateObject();
  if (root == NULL)
    return NULL;

  if (is_configured) {
    cJSON_AddNumberToObject(root, "hotelId", current_config.hotelId);
    cJSON_AddNumberToObject(root, "roomId", current_config.roomId);
    cJSON_AddNumberToObject(root, "zoneId", current_config.zoneId);
    cJSON_AddStringToObject(root, "masterSecret", current_config.masterSecret);
    cJSON_AddStringToObject(root, "authCode", current_config.authCode);
  }

  return root;
}

void server_comm_init(void) {
  // NVS Init usually happens in app_main.c, but we can do a load check here
  if (server_comm_load_config(&current_config)) {
    is_configured = true;
    ESP_LOGI(TAG, "Configuration loaded successfully. Hotel ID: %lu",
             current_config.hotelId);
  } else {
    ESP_LOGW(TAG,
             "No configuration found in NVS. Filling with default values.");
    // Gecici degerler atiyoruz
    current_config.hotelId = 1001;
    current_config.roomId = 105;
    current_config.zoneId = 2;
    strcpy(current_config.masterSecret, "HIDDEN_KEY_123");
    strcpy(current_config.authCode, "192837");
    is_configured = true;

    server_comm_save_config(&current_config); // Save defaults for testing
  }
}

bool server_comm_save_config(const server_comm_config_t *config) {
  nvs_handle_t my_handle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &my_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
    return false;
  }

  nvs_set_u32(my_handle, "hotelId", config->hotelId);
  nvs_set_u32(my_handle, "roomId", config->roomId);
  nvs_set_u32(my_handle, "zoneId", config->zoneId);
  nvs_set_str(my_handle, "masterSecret", config->masterSecret);
  nvs_set_str(my_handle, "authCode", config->authCode);

  nvs_commit(my_handle);
  nvs_close(my_handle);

  memcpy(&current_config, config, sizeof(server_comm_config_t));
  is_configured = true;

  return true;
}

bool server_comm_load_config(server_comm_config_t *config) {
  nvs_handle_t my_handle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &my_handle);
  if (err != ESP_OK)
    return false;

  size_t required_size;

  err = nvs_get_u32(my_handle, "hotelId", &config->hotelId);
  if (err != ESP_OK)
    goto _fail;

  err = nvs_get_u32(my_handle, "roomId", &config->roomId);
  if (err != ESP_OK)
    goto _fail;

  err = nvs_get_u32(my_handle, "zoneId", &config->zoneId);
  if (err != ESP_OK)
    goto _fail;

  required_size = sizeof(config->masterSecret);
  err = nvs_get_str(my_handle, "masterSecret", config->masterSecret,
                    &required_size);
  if (err != ESP_OK)
    goto _fail;

  required_size = sizeof(config->authCode);
  err = nvs_get_str(my_handle, "authCode", config->authCode, &required_size);
  if (err != ESP_OK)
    goto _fail;

  nvs_close(my_handle);
  return true;

_fail:
  nvs_close(my_handle);
  return false;
}

bool server_comm_send_setup(void) {
  cJSON *root = create_base_json();
  if (root == NULL)
    return false;

  cJSON_AddStringToObject(root, "role", "FULL_SETUP");

  char *json_str = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);

  ESP_LOGI(TAG, "Sending Setup: %s", json_str);
  bool res = http_post_json(json_str);
  cJSON_free(json_str);
  return res;
}

bool server_comm_send_event(server_event_type_t type, const char *data) {
  cJSON *root = create_base_json();
  if (root == NULL)
    return false;

  cJSON_AddStringToObject(root, "role", "EVENT_LOG");
  cJSON_AddNumberToObject(root, "dpid", (int)type);

  if (data != NULL) {
    cJSON_AddStringToObject(root, "value", data);
  } else {
    cJSON_AddStringToObject(root, "value", "");
  }

  char *json_str = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);

  ESP_LOGI(TAG, "Sending Event: %s", json_str);
  bool res = http_post_json(json_str);
  cJSON_free(json_str);
  return res;
}

bool server_comm_send_alarm(server_alarm_type_t alarm_type) {
  cJSON *root = create_base_json();
  if (root == NULL)
    return false;

  cJSON_AddStringToObject(root, "role", "ALARM_LOG");
  cJSON_AddNumberToObject(root, "alarm_code", (int)alarm_type);

  char *json_str = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);

  ESP_LOGI(TAG, "Sending Alarm: %s", json_str);
  bool res = http_post_json(json_str);
  cJSON_free(json_str);
  return res;
}
