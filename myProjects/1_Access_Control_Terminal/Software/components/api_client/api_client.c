#include "api_client.h"
#include "esp_http_client.h"
#include "mqtt_client.h"
#include "esp_log.h"
#include "cJSON.h"
#include "nv_storage.h"
#include "esp_mac.h"
#include "esp_crt_bundle.h"
#include "esp_timer.h"
#include "wifi_manager.h"
#include "../app_state_machine/include/app_state_machine.h"
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>

static const char *TAG = "API_CLIENT";
static const char *CLAIM_URL = "https://api.onloi.com/v1/devices/claim";

#define SOF_BYTE 0x4F
#define VER_BYTE 0x02

#define MSG_TYPE_REQUEST 0x01
#define MSG_TYPE_RESPONSE 0x02
#define MSG_TYPE_EVENT 0x03
#define MSG_TYPE_COMMAND 0x04
#define MSG_TYPE_ACK 0x05

#define CMD_TIME_SYNC 0x33
#define CMD_REMOTE_OPEN 0x40
#define CMD_FACTORY_RESET 0x50
#define CMD_ACK 0x05
#define CMD_PIN_ENTERED 0x12
#define CMD_NFC_SCANNED 0x13
#define CMD_QR_SCANNED 0x14
#define CMD_LOCK_OPENED 0x10
#define CMD_SESSION_START 0x30
#define CMD_REGISTRATION 0x34

static esp_mqtt_client_handle_t mqtt_client = NULL;
static bool mqtt_connected = false;
static uint16_t mqtt_seq = 0;
static char device_id_str[32] = {0};
static char device_code_str[32] = "AC_V1";
static onloi_mqtt_config_t s_mqtt_cfg;

static char topic_cmd[128];
static char topic_reply[128];
static char topic_config[128];
static char topic_lifecycle[128];
static char topic_event[128];
static char topic_ack[128];

static void generate_device_id(void) {
    if (strlen(device_id_str) == 0) {
        uint8_t mac[6];
        esp_read_mac(mac, ESP_MAC_BT);
        uint32_t low = ((uint32_t)mac[3] << 16) | ((uint32_t)mac[4] << 8) | mac[5];
        snprintf(device_id_str, sizeof(device_id_str), "SMART_%06lX", (unsigned long)low);
    }
}

static uint16_t calculate_crc16(const uint8_t *data, size_t length) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < length; i++) {
        crc ^= (data[i] << 8);
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) crc = (crc << 1) ^ 0x1021;
            else crc <<= 1;
        }
    }
    return crc;
}

static void publish_mqtt_frame(uint8_t msg_type, uint8_t cmd, const uint8_t *payload, uint16_t payload_len, const char *topic, bool retain) {
    if (!mqtt_connected || mqtt_client == NULL) return;

    uint8_t frame[512];
    frame[0] = SOF_BYTE;
    frame[1] = VER_BYTE;
    frame[2] = msg_type;
    frame[3] = cmd;

    uint16_t seq = mqtt_seq++;
    frame[4] = (seq >> 8) & 0xFF;
    frame[5] = seq & 0xFF;
    frame[6] = (payload_len >> 8) & 0xFF;
    frame[7] = payload_len & 0xFF;
    frame[8] = 0x00; // STATUS

    uint64_t ts = (uint64_t)(esp_timer_get_time() / 1000ULL);
    for (int i = 0; i < 8; i++) {
        frame[9 + i] = (ts >> (56 - i * 8)) & 0xFF;
    }

    if (payload_len > 0 && payload != NULL) {
        memcpy(frame + 17, payload, payload_len);
    }

    uint16_t crc = calculate_crc16(frame, 17 + payload_len);
    frame[17 + payload_len] = (crc >> 8) & 0xFF;
    frame[17 + payload_len + 1] = crc & 0xFF;

    size_t total_len = 19 + payload_len;
    esp_mqtt_client_publish(mqtt_client, topic, (const char*)frame, total_len, 1, retain);
    ESP_LOGI(TAG, "MQTT TX CMD: 0x%02X, Seq: %d, Len: %zu", cmd, seq, total_len);
}

static void send_mqtt_ack(uint32_t requestId, uint8_t ackPhase, uint8_t cmdEcho, uint8_t detailCode) {
    uint8_t payload[7];
    payload[0] = (requestId >> 24) & 0xFF;
    payload[1] = (requestId >> 16) & 0xFF;
    payload[2] = (requestId >> 8) & 0xFF;
    payload[3] = requestId & 0xFF;
    payload[4] = ackPhase;
    payload[5] = cmdEcho;
    payload[6] = detailCode;
    publish_mqtt_frame(MSG_TYPE_ACK, CMD_ACK, payload, 7, topic_ack, false);
}

static void process_mqtt_message(const char *topic, size_t topic_len, const char *data, size_t data_len) {
    if (data_len < 19) return;
    const uint8_t *frame = (const uint8_t *)data;
    if (frame[0] != SOF_BYTE || frame[1] != VER_BYTE) return;

    uint16_t payload_len = (frame[6] << 8) | frame[7];
    uint8_t msg_type = frame[2];
    uint8_t cmd = frame[3];

    size_t total_expected = 19 + payload_len;
    if (msg_type == MSG_TYPE_COMMAND) total_expected += 8; // HMAC skipped but present in length

    if (data_len < total_expected) return;

    // We skip HMAC validation as requested, but we still validate CRC
    uint16_t received = (frame[total_expected - 2] << 8) | frame[total_expected - 1];
    uint16_t calculated = calculate_crc16(frame, total_expected - 2);

    if (calculated != received) {
        ESP_LOGE(TAG, "MQTT RX CRC Error. Calc: 0x%04X, Rx: 0x%04X", calculated, received);
        return;
    }

    ESP_LOGI(TAG, "MQTT RX CMD: 0x%02X", cmd);

    if (msg_type == MSG_TYPE_COMMAND) {
        const uint8_t *payload = frame + 17;
        uint32_t requestId = 0;
        if (payload_len >= 4) {
            requestId = (payload[0] << 24) | (payload[1] << 16) | (payload[2] << 8) | payload[3];
        }

        if (cmd == CMD_REMOTE_OPEN) {
            send_mqtt_ack(requestId, 0x01, CMD_REMOTE_OPEN, 0x00); // RECEIVED
            app_trigger_door_open();
            send_mqtt_ack(requestId, 0x03, CMD_REMOTE_OPEN, 0x00); // COMPLETED
        } else if (cmd == CMD_FACTORY_RESET) {
            send_mqtt_ack(requestId, 0x01, CMD_FACTORY_RESET, 0x00);
            app_set_state(STATE_FACTORY_RESET);
        }
    } else if (msg_type == MSG_TYPE_RESPONSE) {
        if (cmd == CMD_TIME_SYNC) {
            ESP_LOGI(TAG, "Time sync response received.");
        }
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT Connected!");
            mqtt_connected = true;
            esp_mqtt_client_subscribe(mqtt_client, topic_cmd, 1);
            esp_mqtt_client_subscribe(mqtt_client, topic_reply, 1);
            esp_mqtt_client_subscribe(mqtt_client, topic_config, 1);
            
            // Send Lifecycle events
            uint8_t reg_payload[3] = { (s_mqtt_cfg.session_epoch >> 8) & 0xFF, s_mqtt_cfg.session_epoch & 0xFF, 0x00 };
            publish_mqtt_frame(MSG_TYPE_EVENT, CMD_REGISTRATION, reg_payload, 3, topic_lifecycle, true);

            uint8_t sess_payload[3] = { 0x01, 85, 0x00 };
            publish_mqtt_frame(MSG_TYPE_EVENT, CMD_SESSION_START, sess_payload, 3, topic_lifecycle, true);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT Disconnected.");
            mqtt_connected = false;
            break;
        case MQTT_EVENT_DATA:
            process_mqtt_message(event->topic, event->topic_len, event->data, event->data_len);
            break;
        default:
            break;
    }
}

static void start_mqtt(void) {
    if (mqtt_client != NULL) return;

    snprintf(topic_cmd, sizeof(topic_cmd), "onloi/v1/d/%s/cmd", device_id_str);
    snprintf(topic_reply, sizeof(topic_reply), "onloi/v1/d/%s/reply", device_id_str);
    snprintf(topic_config, sizeof(topic_config), "onloi/v1/d/%s/config", device_id_str);
    snprintf(topic_lifecycle, sizeof(topic_lifecycle), "onloi/v1/d/%s/lifecycle", device_id_str);
    snprintf(topic_event, sizeof(topic_event), "onloi/v1/d/%s/event", device_id_str);
    snprintf(topic_ack, sizeof(topic_ack), "onloi/v1/d/%s/ack", device_id_str);

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.hostname = s_mqtt_cfg.host,
        .broker.address.port = s_mqtt_cfg.port,
        .broker.address.transport = (s_mqtt_cfg.port == 8883) ? MQTT_TRANSPORT_OVER_SSL : MQTT_TRANSPORT_OVER_TCP,
        .broker.verification.crt_bundle_attach = (s_mqtt_cfg.port == 8883) ? esp_crt_bundle_attach : NULL,
        .credentials.client_id = s_mqtt_cfg.client_id,
        .credentials.username = s_mqtt_cfg.username,
        .credentials.authentication.password = s_mqtt_cfg.password,
        .session.keepalive = 30,
        .session.last_will.topic = topic_lifecycle,
        .session.last_will.msg = "",
        .session.last_will.msg_len = 0,
        .session.last_will.qos = 1,
        .session.last_will.retain = 1
    };

    ESP_LOGI(TAG, "Starting MQTT: Host: %s, Port: %d", s_mqtt_cfg.host, s_mqtt_cfg.port);
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);
}

static esp_err_t _http_event_handle(esp_http_client_event_t *evt) {
    return ESP_OK;
}

static void execute_claim(const char *prov_token) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "deviceId", device_id_str);
    cJSON_AddStringToObject(root, "deviceCode", device_code_str);
    cJSON_AddStringToObject(root, "provisioningToken", prov_token);
    char *post_data = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    ESP_LOGI(TAG, "Claim Request: %s", post_data);

    esp_http_client_config_t config = {
        .url = CLAIM_URL,
        .event_handler = _http_event_handle,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK && esp_http_client_get_status_code(client) == 200) {
        int content_len = esp_http_client_get_content_length(client);
        char *resp_buf = malloc(content_len + 1);
        if (resp_buf) {
            esp_http_client_read_response(client, resp_buf, content_len);
            resp_buf[content_len] = '\0';
            ESP_LOGI(TAG, "Claim Response: %s", resp_buf);

            cJSON *res = cJSON_Parse(resp_buf);
            if (res) {
                cJSON *mqtt = cJSON_GetObjectItem(res, "mqtt");
                if (mqtt) {
                    strncpy(s_mqtt_cfg.host, cJSON_GetObjectItem(mqtt, "host")->valuestring, 127);
                    s_mqtt_cfg.port = cJSON_GetObjectItem(mqtt, "port")->valueint;
                    strncpy(s_mqtt_cfg.client_id, cJSON_GetObjectItem(mqtt, "clientId")->valuestring, 63);
                    strncpy(s_mqtt_cfg.username, cJSON_GetObjectItem(mqtt, "username")->valuestring, 63);
                    strncpy(s_mqtt_cfg.password, cJSON_GetObjectItem(mqtt, "password")->valuestring, 63);
                    strncpy(s_mqtt_cfg.secret, cJSON_GetObjectItem(res, "deviceSecret")->valuestring, 127);
                    s_mqtt_cfg.session_epoch = cJSON_GetObjectItem(res, "sessionEpoch")->valueint;
                    s_mqtt_cfg.is_claimed = true;

                    nv_storage_save_onloi_mqtt_config(&s_mqtt_cfg);
                    nv_storage_clear_prov_token();
                    ESP_LOGI(TAG, "Claim successful! Configs saved.");
                    start_mqtt();
                }
                cJSON_Delete(res);
            }
            free(resp_buf);
        }
    } else {
        ESP_LOGE(TAG, "Claim Request Failed: %d", esp_http_client_get_status_code(client));
    }
    esp_http_client_cleanup(client);
    free(post_data);
}

void api_client_init(void) {
    generate_device_id();
}

void api_client_tick(void) {
    static int64_t last_check = 0;
    int64_t now = esp_timer_get_time() / 1000;

    if (now - last_check > 5000) {
        last_check = now;
        if (wifi_manager_is_connected()) {
            if (!s_mqtt_cfg.is_claimed) {
                if (nv_storage_get_onloi_mqtt_config(&s_mqtt_cfg) && s_mqtt_cfg.is_claimed) {
                    start_mqtt();
                } else {
                    char token[65] = {0};
                    if (nv_storage_get_prov_token(token, sizeof(token)) && strlen(token) > 0) {
                        ESP_LOGI(TAG, "Found provisioning token, executing claim...");
                        execute_claim(token);
                    }
                }
            } else if (mqtt_client == NULL) {
                start_mqtt();
            }
        }
    }
}

int api_client_send_pass_event(const char* data_val) {
    if (!mqtt_connected) return -1;
    
    // Convert prefix to CMD
    uint8_t cmd = 0;
    const char *payload_data = NULL;

    if (strncmp(data_val, "KEYPAD:", 7) == 0) {
        cmd = CMD_PIN_ENTERED;
        payload_data = data_val + 7;
    } else if (strncmp(data_val, "RFID:", 5) == 0) {
        cmd = CMD_NFC_SCANNED;
        payload_data = data_val + 5;
    } else if (strncmp(data_val, "QR:", 3) == 0) {
        cmd = CMD_QR_SCANNED;
        payload_data = data_val + 3;
    } else {
        return -2; // Unknown
    }

    uint8_t payload[128];
    uint8_t len = strlen(payload_data);
    if (len > 120) len = 120;
    
    payload[0] = len;
    memcpy(payload + 1, payload_data, len);
    
    uint32_t reqId = mqtt_seq;
    payload[1 + len] = (reqId >> 24) & 0xFF;
    payload[1 + len + 1] = (reqId >> 16) & 0xFF;
    payload[1 + len + 2] = (reqId >> 8) & 0xFF;
    payload[1 + len + 3] = reqId & 0xFF;

    publish_mqtt_frame(MSG_TYPE_EVENT, cmd, payload, 1 + len + 4, topic_event, false);
    return 200; // Simulated success
}

int api_client_send_get_time(void) {
    if (!mqtt_connected) return -1;
    publish_mqtt_frame(MSG_TYPE_REQUEST, CMD_TIME_SYNC, NULL, 0, topic_cmd, false);
    return 200;
}
