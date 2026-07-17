#include "ble_server.h"
#include "esp_log.h"
#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "ble_frame_parser.h"
#include "crypto_manager.h"
#include "cJSON.h"

static const char *TAG = "BLE_SERVER";
static uint8_t g_session_key[CRYPTO_AES_KEY_SIZE];

// 6E400101-B5A3-F393-E0A9-E50E24DCCA9E
static const ble_uuid128_t gatt_svc_uuid =
    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
                     0x93, 0xf3, 0xa3, 0xb5, 0x01, 0x01, 0x40, 0x6e);

// RX: 6E400102-B5A3-F393-E0A9-E50E24DCCA9E
static const ble_uuid128_t gatt_rx_uuid =
    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
                     0x93, 0xf3, 0xa3, 0xb5, 0x02, 0x01, 0x40, 0x6e);

// TX: 6E400103-B5A3-F393-E0A9-E50E24DCCA9E
static const ble_uuid128_t gatt_tx_uuid =
    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
                     0x93, 0xf3, 0xa3, 0xb5, 0x03, 0x01, 0x40, 0x6e);

static uint16_t tx_handle;
static uint16_t conn_handle;

static int gatt_svr_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt,
                               void *arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
        ESP_LOGI(TAG, "Data received on RX characteristic. Length: %d", len);
        
        uint8_t *rx_buf = malloc(len);
        if (rx_buf == NULL) return 0;
        os_mbuf_copydata(ctxt->om, 0, len, rx_buf);

        ble_frame_t rx_frame;
        if (ble_frame_parse(rx_buf, len, &rx_frame)) {
            ESP_LOGI(TAG, "Parsed Frame -> CMD: 0x%02X, TYPE: 0x%02X, SEQ: %d", rx_frame.cmd, rx_frame.type, rx_frame.seq);
            
            if (rx_frame.cmd == 0x71 && rx_frame.type == 0x01) { // KEX_INIT REQUEST
                ESP_LOGI(TAG, "Received KEX_INIT Request from Mobile App");
                
                // 1. Parse Mobile's Ephemeral PubKey
                if (rx_frame.len == 33) { // 32 bytes pubkey + 1 byte role
                    uint8_t peer_pub_key[CRYPTO_X25519_KEY_SIZE];
                    memcpy(peer_pub_key, rx_frame.payload, CRYPTO_X25519_KEY_SIZE);
                    
                    // 2. Generate our Ephemeral Keypair
                    uint8_t my_priv_key[CRYPTO_X25519_KEY_SIZE];
                    uint8_t my_pub_key[CRYPTO_X25519_KEY_SIZE];
                    if (crypto_generate_x25519_keypair(my_priv_key, my_pub_key)) {
                        
                        // 3. Compute Shared Secret
                        uint8_t shared_secret[CRYPTO_X25519_KEY_SIZE];
                        crypto_ecdh_shared_secret(my_priv_key, peer_pub_key, shared_secret);
                        
                        const uint8_t *info = (const uint8_t *)"onloi-gw-session-v1";
                        crypto_hkdf_sha256(shared_secret, sizeof(shared_secret), NULL, 0, info, strlen((char*)info), g_session_key, sizeof(g_session_key));
                        ESP_LOGI(TAG, "Derived ECDH Session Key via HKDF.");
                        
                        // 4. Build Response
                        uint8_t tx_buf[128];
                        uint16_t tx_len = ble_frame_build_resp(0x71, rx_frame.seq, 0x00, my_pub_key, CRYPTO_X25519_KEY_SIZE, tx_buf);
                        
                        // 5. Send via Notification (TX characteristic)
                        struct os_mbuf *om = ble_hs_mbuf_from_flat(tx_buf, tx_len);
                        ble_gatts_notify_custom(conn_handle, tx_handle, om);
                        ESP_LOGI(TAG, "Sent KEX_INIT Response");
                    }
                }
            }
            if (rx_frame.cmd == 0x72 && rx_frame.type == 0x01) { // KEX_DELIVER REQUEST
                ESP_LOGI(TAG, "Received KEX_DELIVER Request from Mobile App");
                if (rx_frame.len >= 28) { // 12 IV + 16 TAG + min 0 cipher
                    uint8_t iv[12];
                    uint8_t tag[16];
                    uint16_t cipher_len = rx_frame.len - 28;
                    uint8_t *ciphertext = rx_frame.payload + 12;
                    uint8_t *plaintext = malloc(cipher_len + 1);
                    
                    if (plaintext != NULL) {
                        memcpy(iv, rx_frame.payload, 12);
                        memcpy(tag, rx_frame.payload + 12 + cipher_len, 16);
                        
                        if (crypto_aes_gcm_decrypt(g_session_key, iv, ciphertext, cipher_len, NULL, 0, tag, plaintext)) {
                            plaintext[cipher_len] = '\0';
                            ESP_LOGI(TAG, "Decrypted KEX_DELIVER JSON: %s", plaintext);
                            
                            // Send KEX_DELIVER Response (Success)
                            uint8_t tx_buf[64];
                            uint16_t tx_len = ble_frame_build_resp(0x72, rx_frame.seq, 0x00, NULL, 0, tx_buf);
                            struct os_mbuf *om = ble_hs_mbuf_from_flat(tx_buf, tx_len);
                            ble_gatts_notify_custom(conn_handle, tx_handle, om);
                            ESP_LOGI(TAG, "Sent KEX_DELIVER Response");
                            
                            // Parse JSON for WiFi credentials
                            cJSON *json = cJSON_Parse((char*)plaintext);
                            if (json != NULL) {
                                cJSON *s_item = cJSON_GetObjectItemCaseSensitive(json, "s");
                                cJSON *p_item = cJSON_GetObjectItemCaseSensitive(json, "p");
                                cJSON *t_item = cJSON_GetObjectItemCaseSensitive(json, "t");
                                
                                if (cJSON_IsString(s_item) && cJSON_IsString(p_item) && cJSON_IsString(t_item)) {
                                    ESP_LOGI(TAG, "WiFi SSID: %s", s_item->valuestring);
                                    ESP_LOGI(TAG, "Claim Token: %s", t_item->valuestring);
                                    // TODO: Trigger NVS save, WiFi Connection, and API Claim Request
                                }
                                cJSON_Delete(json);
                            }
                        } else {
                            ESP_LOGE(TAG, "KEX_DELIVER Decryption Failed!");
                            uint8_t tx_buf[64];
                            uint16_t tx_len = ble_frame_build_resp(0x72, rx_frame.seq, 0x01, NULL, 0, tx_buf); // Fail
                            struct os_mbuf *om = ble_hs_mbuf_from_flat(tx_buf, tx_len);
                            ble_gatts_notify_custom(conn_handle, tx_handle, om);
                        }
                        free(plaintext);
                    }
                }
            }
        }
        free(rx_buf);
    }
    return 0;
}

static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &gatt_svc_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = &gatt_rx_uuid.u,
                .access_cb = gatt_svr_chr_access,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
            },
            {
                .uuid = &gatt_tx_uuid.u,
                .access_cb = gatt_svr_chr_access,
                .flags = BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &tx_handle,
            },
            {
                0, /* No more characteristics in this service */
            }
        },
    },
    {
        0, /* No more services */
    },
};

static int ble_server_gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        ESP_LOGI(TAG, "Peripheral connected; status=%d", event->connect.status);
        if (event->connect.status == 0) {
            conn_handle = event->connect.conn_handle;
        } else {
            ble_server_start_adv();
        }
        break;
    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "Peripheral disconnected; reason=%d", event->disconnect.reason);
        ble_server_start_adv();
        break;
    case BLE_GAP_EVENT_SUBSCRIBE:
        ESP_LOGI(TAG, "Client subscribed to TX");
        break;
    }
    return 0;
}

esp_err_t ble_server_init(void)
{
    int rc = ble_gatts_count_cfg(gatt_svr_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to count GATT services; rc=%d", rc);
        return ESP_FAIL;
    }

    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to add GATT services; rc=%d", rc);
        return ESP_FAIL;
    }

    return ESP_OK;
}

void ble_server_start_adv(void)
{
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    int rc;

    memset(&fields, 0, sizeof(fields));
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

    fields.name = (uint8_t *)"OG610";
    fields.name_len = strlen("OG610");
    fields.name_is_complete = 1;
    
    // Add manufacturer data: 0x4F 0x4B (OK) + "OG610" + 0x00 (Unclaimed)
    uint8_t mfg_data[8] = {0x4F, 0x4B, 'O', 'G', '6', '1', '0', 0x00};
    fields.mfg_data = mfg_data;
    fields.mfg_data_len = sizeof(mfg_data);

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error setting advertisement data; rc=%d", rc);
        return;
    }

    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    
    uint8_t own_addr_type;
    ble_hs_id_infer_auto(0, &own_addr_type);

    rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER,
                           &adv_params, ble_server_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error enabling advertisement; rc=%d", rc);
    } else {
        ESP_LOGI(TAG, "Started Advertising as Onloi Gateway Peripheral");
    }
}

void ble_server_stop_adv(void)
{
    ble_gap_adv_stop();
}
