#include "ble_prov.h"
#include "../hal_io/include/hw_config.h"
#include "cJSON.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "host/ble_hs.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "nv_storage.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "wifi_manager.h"
#include "../api_client/include/api_client.h"

static const char *TAG = "BLE_PROV";
static uint8_t ble_rx_buffer[2048];
static int ble_rx_len = 0;
static uint16_t conn_handle = BLE_HS_CONN_HANDLE_NONE;
static uint16_t char_val_handle;
static uint8_t nimble_port_own_addr_type;
static char device_id_str[32] = "SMART_000000";
static char ble_name_str[64] = "Onloi_SMARTaccess_000000";

// 4fafc201-1fb5-459e-8fcc-c5c9c331914b
static const ble_uuid128_t gatt_svr_svc_uuid =
    BLE_UUID128_INIT(0x4b, 0x91, 0x31, 0xc3, 0xc9, 0xc5, 0xcc, 0x8f, 0x9e, 0x45,
                     0xb5, 0x1f, 0x01, 0xc2, 0xaf, 0x4f);

// beb5483e-36e1-4688-b7f5-ea07361b26a8
static const ble_uuid128_t gatt_svr_chr_uuid =
    BLE_UUID128_INIT(0xa8, 0x26, 0x1b, 0x36, 0x07, 0xea, 0xf5, 0xb7, 0x88, 0x46,
                     0xe1, 0x36, 0x3e, 0x48, 0xb5, 0xbe);

static void ble_prov_process_json(const char *json_str) {
  ESP_LOGI(TAG, "Parsing incoming JSON: %s", json_str);
  cJSON *root = cJSON_Parse(json_str);
  if (!root) {
    ESP_LOGE(TAG, "Invalid JSON");
    return;
  }

  cJSON *cmd = cJSON_GetObjectItem(root, "command");
  if (!cmd || !cJSON_IsString(cmd)) {
    cJSON_Delete(root);
    return;
  }

  const char *command = cmd->valuestring;
  ESP_LOGI(TAG, "Command: %s", command);

  if (strcmp(command, "handshake") == 0) {
    char resp[128];
    int64_t timestamp_ms = esp_timer_get_time() / 1000ULL;
    snprintf(
        resp, sizeof(resp),
        "{\"command\":\"handshake\",\"deviceId\":\"%s\",\"createdAt\":%lld}",
        device_id_str, (long long)timestamp_ms);
    ble_prov_send_response(resp);
  } else if (strcmp(command, "pair") == 0) {
    cJSON *d_key = cJSON_GetObjectItem(root, "deviceKey");
    cJSON *p_key = cJSON_GetObjectItem(root, "pairKey");
    if (d_key && p_key && d_key->valuestring && p_key->valuestring) {
      nv_storage_save_api_keys(d_key->valuestring, p_key->valuestring);
      ESP_LOGI(TAG, "Pair keys received and saved to NVS");
      ble_prov_send_response("{\"command\":\"pair\",\"result\":\"succeeded\"}");
      
      // If Wi-Fi is already connected, ping the server so it knows we are online with the new keys
      if (wifi_manager_is_connected()) {
          wifi_manager_trigger_time_sync();
      }
    } else {
      ble_prov_send_response("{\"command\":\"pair\",\"result\":\"failed\"}");
    }
  } else if (strcmp(command, "getSsids") == 0) {
    wifi_manager_request_scan();
    // The super loop or callback will fetch the response naturally
    // But BLE protocol in the sketch expects response asynchronously.
    // Wait, app_state_machine_tick needs to poll wifi_manager_is_scan_ready.
    // I will implement that polling in app_state_machine instead.
  } else if (strcmp(command, "connectWifi") == 0) {
    cJSON *ssid = cJSON_GetObjectItem(root, "ssid");
    cJSON *pass = cJSON_GetObjectItem(root, "pass");
    if (ssid && pass && ssid->valuestring && pass->valuestring) {
      // Optional: load macAddress
      ESP_LOGI(TAG, "Connecting to SSID: %s", ssid->valuestring);
      wifi_manager_connect(ssid->valuestring, pass->valuestring);
      // Save to standard config if needed
      ble_prov_send_response(
          "{\"command\":\"connectWifi\",\"result\":\"succeeded\"}");
    } else {
      ble_prov_send_response(
          "{\"command\":\"connectWifi\",\"result\":\"missing_credentials\"}");
    }
  } else {
    ble_prov_send_response(
        "{\"command\":\"error\",\"result\":\"unknown command\"}");
  }

  cJSON_Delete(root);
}

static int gatts_access_cb(uint16_t conn, uint16_t attr_handle,
                           struct ble_gatt_access_ctxt *ctxt, void *arg) {
  if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
    uint16_t mbuf_len = OS_MBUF_PKTLEN(ctxt->om);
    uint8_t *data = OS_MBUF_DATA(ctxt->om, uint8_t *);

    for (int i = 0; i < mbuf_len; i++) {
      if (data[i] == '\0') {
        ble_prov_process_json((char *)ble_rx_buffer);
        ble_rx_len = 0;
        memset(ble_rx_buffer, 0, sizeof(ble_rx_buffer));
      } else {
        if (ble_rx_len < sizeof(ble_rx_buffer) - 1) {
          ble_rx_buffer[ble_rx_len++] = data[i];
          ble_rx_buffer[ble_rx_len] = '\0';
        }
      }
    }
  }
  return 0;
}

static const struct ble_gatt_svc_def gatt_svcs[] = {
    {.type = BLE_GATT_SVC_TYPE_PRIMARY,
     .uuid = &gatt_svr_svc_uuid.u,
     .characteristics =
         (struct ble_gatt_chr_def[]){{
                                         .uuid = &gatt_svr_chr_uuid.u,
                                         .access_cb = gatts_access_cb,
                                         .flags = BLE_GATT_CHR_F_READ |
                                                  BLE_GATT_CHR_F_WRITE |
                                                  BLE_GATT_CHR_F_NOTIFY,
                                         .val_handle = &char_val_handle,
                                     },
                                     {0}}},
    {0}};

void ble_prov_send_response(const char *json_data) {
  if (conn_handle == BLE_HS_CONN_HANDLE_NONE) {
    ESP_LOGE(TAG, "No connection to notify");
    return;
  }
  const int MTU_PAYLOAD = 19;
  int len = strlen(json_data);
  ESP_LOGI(TAG, "Sending Response (len:%d)", len);

  for (int pos = 0; pos < len; pos += MTU_PAYLOAD) {
    int chunkSize = (len - pos < MTU_PAYLOAD) ? (len - pos) : MTU_PAYLOAD;
    bool isLast = (pos + chunkSize >= len);

    struct os_mbuf *txom = ble_hs_mbuf_from_flat(json_data + pos, chunkSize);
    if (isLast) {
      // Append explicit \0 as the sketch did (chunk += char(0x00))
      uint8_t term = '\0';
      os_mbuf_append(txom, &term, 1);
    }
    ble_gatts_notify_custom(conn_handle, char_val_handle, txom);

    // Slight delay can be yielding to FreeRTOS so NimBLE pushes data to queues
    vTaskDelay(pdMS_TO_TICKS(40));
  }
}

static int ble_gap_event(struct ble_gap_event *event, void *arg) {
  switch (event->type) {
  case BLE_GAP_EVENT_CONNECT:
    ESP_LOGI(TAG, "Connected");
    conn_handle = event->connect.conn_handle;
    break;
  case BLE_GAP_EVENT_DISCONNECT:
    ESP_LOGI(TAG, "Disconnected");
    conn_handle = BLE_HS_CONN_HANDLE_NONE;
    // restart advertising
    struct ble_gap_adv_params adv_params;
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    ble_gap_adv_start(nimble_port_own_addr_type, NULL, BLE_HS_FOREVER,
                      &adv_params, ble_gap_event, NULL);
    break;
  }
  return 0;
}

static void ble_app_on_sync(void) {
  ESP_LOGI(TAG, "BLE synced");
  ble_hs_id_infer_auto(0, &nimble_port_own_addr_type);

  struct ble_gap_adv_params adv_params;
  memset(&adv_params, 0, sizeof(adv_params));
  adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
  adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

  struct ble_hs_adv_fields fields;
  memset(&fields, 0, sizeof(fields));
  fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
  fields.uuids128 = (ble_uuid128_t[]){gatt_svr_svc_uuid};
  fields.num_uuids128 = 1;
  fields.uuids128_is_complete = 1;
  ble_gap_adv_set_fields(&fields);

  struct ble_hs_adv_fields scanned_fields;
  memset(&scanned_fields, 0, sizeof(scanned_fields));
  scanned_fields.name = (uint8_t *)ble_name_str;
  scanned_fields.name_len = strlen(ble_name_str);
  scanned_fields.name_is_complete = 1;
  ble_gap_adv_rsp_set_fields(&scanned_fields);

  ble_gap_adv_start(nimble_port_own_addr_type, NULL, BLE_HS_FOREVER,
                    &adv_params, ble_gap_event, NULL);
}

static void ble_host_task(void *param) {
  ESP_LOGI(TAG, "BLE Host Task Started");
  nimble_port_run();
  nimble_port_freertos_deinit();
}

void ble_prov_init(void) {
  ESP_LOGI(TAG, "Initializing BLE Provisioning");

  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_BT);
  uint32_t low = ((uint32_t)mac[3] << 16) | ((uint32_t)mac[4] << 8) | mac[5];
  snprintf(device_id_str, sizeof(device_id_str), "SMART_%06lX",
           (unsigned long)low);

  if (active_hw_version == HW_VERSION_QR_ONLY) {
    snprintf(ble_name_str, sizeof(ble_name_str), "Onloi_QR_%s", device_id_str);
  } else {
    snprintf(ble_name_str, sizeof(ble_name_str), "Onloi_RFID_%s",
             device_id_str);
  }

  ESP_LOGI(TAG, "Generated Device ID: %s", device_id_str);
  ESP_LOGI(TAG, "Generated BLE Name: %s", ble_name_str);

  // nimble_port_init expects nvs_flash_init to be called beforehand (already in
  // main)
  nimble_port_init();

  ble_gatts_count_cfg(gatt_svcs);
  ble_gatts_add_svcs(gatt_svcs);

  ble_hs_cfg.sync_cb = ble_app_on_sync;

  nimble_port_freertos_init(ble_host_task);
}
