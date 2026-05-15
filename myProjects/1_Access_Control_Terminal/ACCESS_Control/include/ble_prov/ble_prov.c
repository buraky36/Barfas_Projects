#include "ble_prov.h"
#include "cJSON.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "storage.h"
#include "wifi_manager.h"
#include <string.h>


// NimBLE Includes
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "host/util/util.h"

static const char *TAG = "BLE_PROV";
static uint8_t ble_addr_type;
static char device_id[32] = "SMART_000000";
static char ble_name[64] = "Onloi_SMART_000000";

// Reference UUIDs
// Service: 4fafc201-1fb5-459e-8fcc-c5c9c331914b
// Char:    beb5483e-36e1-4688-b7f5-ea07361b26a8

static const ble_uuid128_t prov_svc_uuid =
    BLE_UUID128_INIT(0x4b, 0x91, 0x31, 0xc3, 0xc9, 0xc5, 0xcc, 0x8f, 0x9e, 0x45,
                     0xb5, 0x1f, 0x01, 0xc2, 0xaf, 0x4f);

static const ble_uuid128_t prov_chr_uuid =
    BLE_UUID128_INIT(0xa8, 0x26, 0x1b, 0x36, 0x07, 0xea, 0xf5, 0xb7, 0x88, 0x46,
                     0xe1, 0x36, 0x3e, 0x48, 0xb5, 0xbe);

static uint16_t prov_chr_val_handle;

// Helper to send a string response back via Notify
static void ble_prov_send_response(const char *response_json) {
  if (prov_chr_val_handle == 0)
    return;

  struct os_mbuf *om =
      ble_hs_mbuf_from_flat(response_json, strlen(response_json));
  if (!om) {
    ESP_LOGE(TAG, "Failed to allocate mbuf for response");
    return;
  }

  // We assume the connection handle is 1 for a single active central (standard
  // in NimBLE when connected) A more robust implementation would track
  // connection handles in the GAP event callback To be perfectly safe, we
  // iterate through active connections
  int rc = ble_gatts_notify_custom(1, prov_chr_val_handle, om);
  if (rc == 0) {
    ESP_LOGI(TAG, "Sent notification: %s", response_json);
  } else {
    ESP_LOGW(TAG, "Failed to notify (rc=%d), client might not be listening.",
             rc);
  }
}

// Characteristic Write Callback
static int ble_prov_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg) {
  if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
    // Collect incoming data
    uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
    if (len == 0)
      return 0;

    char *buf = malloc(len + 1);
    if (!buf)
      return BLE_ATT_ERR_INSUFFICIENT_RES;

    os_mbuf_copydata(ctxt->om, 0, len, buf);
    buf[len] = '\0';

    ESP_LOGI(TAG, "Received BLE JSON: %s", buf);

    // Parse JSON
    cJSON *json = cJSON_Parse(buf);
    free(buf);

    if (!json) {
      ESP_LOGE(TAG, "Invalid JSON received");
      return 0; // Return 0 to acknowledge GATT write, even if bad payload
    }

    cJSON *cmd = cJSON_GetObjectItemCaseSensitive(json, "command");
    if (cJSON_IsString(cmd) && (cmd->valuestring != NULL)) {

      if (strcmp(cmd->valuestring, "handshake") == 0) {
        // {"command":"handshake","deviceId":"SMART_123456","createdAt":123456}
        // Generate handshake response
        char response[128];
        snprintf(
            response, sizeof(response),
            "{\"command\":\"handshake\",\"deviceId\":\"%s\",\"createdAt\":%lu}",
            device_id,
            (unsigned long)(xTaskGetTickCount() * portTICK_PERIOD_MS));
        ble_prov_send_response(response);

      } else if (strcmp(cmd->valuestring, "pair") == 0) {
        // {"command":"pair","deviceKey":"...","pairKey":"..."}
        cJSON *deviceKey = cJSON_GetObjectItemCaseSensitive(json, "deviceKey");
        cJSON *pairKey = cJSON_GetObjectItemCaseSensitive(json, "pairKey");

        if (cJSON_IsString(deviceKey) && cJSON_IsString(pairKey)) {
          ESP_LOGI(TAG, "Saving Pairing Keys...");
          // Assuming NVS namespace "keys" exists in storage component
          storage_write_string("deviceKey", deviceKey->valuestring);
          storage_write_string("pairKey", pairKey->valuestring);
          ble_prov_send_response(
              "{\"command\":\"pair\",\"result\":\"succeeded\"}");
        } else {
          ble_prov_send_response(
              "{\"command\":\"pair\",\"result\":\"failed\"}");
        }

      } else if (strcmp(cmd->valuestring, "getSsids") == 0) {
        // {"command":"getSsids"}
        // For now, return a dummy list to prevent breaking the flow before we
        // write wifi scanning
        ble_prov_send_response(
            "{\"command\":\"getSsids\",\"result\":{\"0\":{\"macAddress\":\"00:"
            "00:00:00:00:00\",\"rssi\":-50,\"frequency\":\"2.4\",\"name\":"
            "\"DefaultNetwork\"}}}");

      } else if (strcmp(cmd->valuestring, "connectWifi") == 0) {
        // {"command":"connectWifi","ssid":"...","pass":"...","macAddress":"..."}
        cJSON *ssid = cJSON_GetObjectItemCaseSensitive(json, "ssid");
        cJSON *pass = cJSON_GetObjectItemCaseSensitive(json, "pass");

        if (cJSON_IsString(ssid) && cJSON_IsString(pass)) {
          ESP_LOGI(TAG, "Wi-Fi Config received: SSID: %s", ssid->valuestring);

          // Reply first before doing potentially blocking work
          ble_prov_send_response(
              "{\"command\":\"connectWifi\",\"result\":\"succeeded\"}");

          // Connect and save credentials
          wifi_manager_connect(ssid->valuestring, pass->valuestring);

          // The old project restared or stopped BLE here. We can just stop
          // advertising. Let the main RTOS logic dictate full behavior.
        } else {
          ble_prov_send_response("{\"command\":\"connectWifi\",\"result\":"
                                 "\"missing_credentials\"}");
        }
      } else {
        ble_prov_send_response(
            "{\"command\":\"error\",\"result\":\"unknown command\"}");
      }
    }

    cJSON_Delete(json);
  }
  return 0;
}

// GATT Services Table
static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &prov_svc_uuid.u,
        .characteristics =
            (struct ble_gatt_chr_def[]){
                {
                    .uuid = &prov_chr_uuid.u,
                    .access_cb = ble_prov_chr_access,
                    .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE |
                             BLE_GATT_CHR_F_NOTIFY,
                    .val_handle = &prov_chr_val_handle,
                },
                {0} // No more characteristics in this service
            },
    },
    {0} // No more services
};

// GAP Event Callback
static int ble_prov_gap_event(struct ble_gap_event *event, void *arg) {
  switch (event->type) {
  case BLE_GAP_EVENT_CONNECT:
    ESP_LOGI(TAG, "Connection %s; status=%d",
             event->connect.status == 0 ? "established" : "failed",
             event->connect.status);
    if (event->connect.status != 0) {
      // Restart advertising
      ble_prov_init();
    }
    break;

  case BLE_GAP_EVENT_DISCONNECT:
    ESP_LOGI(TAG, "Disconnect; reason=%d", event->disconnect.reason);
    prov_chr_val_handle = 0; // Invalidate handle
    // Restart advertising
    ble_prov_init();
    break;

  case BLE_GAP_EVENT_ADV_COMPLETE:
    ESP_LOGI(TAG, "Adv complete; reason=%d", event->adv_complete.reason);
    break;

  default:
    break;
  }
  return 0;
}

// Host Task
static void ble_prov_host_task(void *param) {
  ESP_LOGI(TAG, "BLE Host Task Started");
  nimble_port_run(); // This function blocks until nimble_port_stop() is called
  nimble_port_freertos_deinit();
  vTaskDelete(NULL);
}

// Core Initialization
static void ble_prov_on_reset(int reason) {
  ESP_LOGE(TAG, "Resetting state; reason=%d", reason);
}

static void ble_prov_on_sync(void) {
  int rc;

  // Figure out address to use (Identity Address)
  rc = ble_hs_util_ensure_addr(0);
  assert(rc == 0);
  rc = ble_hs_id_infer_auto(0, &ble_addr_type);
  assert(rc == 0);

  struct ble_gap_adv_params adv_params;
  memset(&adv_params, 0, sizeof(adv_params));
  adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
  adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

  struct ble_hs_adv_fields fields;
  memset(&fields, 0, sizeof fields);

  // Set General Discoverable Flags
  fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

  // Populate Device Name in Scan Response or Main Adv (doing main here for
  // speed)
  fields.name = (uint8_t *)ble_name;
  fields.name_len = strlen(ble_name);
  fields.name_is_complete = 1;

  rc = ble_gap_adv_set_fields(&fields);
  if (rc != 0) {
    ESP_LOGE(TAG, "Error setting adv fields: %d", rc);
    return;
  }

  rc = ble_gap_adv_start(ble_addr_type, NULL, BLE_HS_FOREVER, &adv_params,
                         ble_prov_gap_event, NULL);
  if (rc != 0) {
    ESP_LOGE(TAG, "Error starting advertising: %d", rc);
  } else {
    ESP_LOGI(TAG, "BLE Advertising Started as %s", ble_name);
  }
}

bool ble_prov_init(void) {
  // 1. Generate Mac Base Device ID
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);

  // Create SMART_XXXXXX format matching old code
  snprintf(device_id, sizeof(device_id), "SMART_%02X%02X%02X", mac[3], mac[4],
           mac[5]);
  snprintf(ble_name, sizeof(ble_name), "Onloi_%s", device_id);

  ESP_LOGI(TAG, "Starting NimBLE for %s", ble_name);

  // Initialize the NimBLE port
  if (nimble_port_init() != 0) {
    ESP_LOGE(TAG, "Failed to init NimBLE port");
    return false;
  }

  // Initialize Host Config
  ble_hs_cfg.reset_cb = ble_prov_on_reset;
  ble_hs_cfg.sync_cb = ble_prov_on_sync;
  ble_hs_cfg.gatts_register_cb =
      NULL; // We don't strictly need a register cb for basic things
  ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

  // Set Name
  ble_svc_gap_device_name_set(ble_name);

  // Initialize GATT
  ble_svc_gap_init();
  ble_svc_gatt_init();

  // Register our custom service
  int rc = ble_gatts_count_cfg(gatt_svr_svcs);
  assert(rc == 0);

  rc = ble_gatts_add_svcs(gatt_svr_svcs);
  if (rc != 0) {
    ESP_LOGE(TAG, "Error registering GATT services: %d", rc);
  }

  // Start the Host Task
  nimble_port_freertos_init(ble_prov_host_task);

  return true;
}

void ble_prov_stop(void) {
  ESP_LOGI(TAG, "Stopping BLE Provisioning");
  nimble_port_stop();
}
