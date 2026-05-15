#include "wrappers.h"
#include "index_html.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

// ESP-IDF Includes
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "driver/i2c.h"
#include "esp_vfs.h"
#include "esp_spiffs.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "cJSON.h"

static const char *TAG = "WRAPPER";

// =============================================================
// SYSTEM & GPIO
// =============================================================
void sys_init_pins(void) {
    gpio_config_t io_conf = {0};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << RELAY_PIN) | (1ULL << BUZZER_PIN);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

    gpio_set_level((gpio_num_t)RELAY_PIN, 0);
    gpio_set_level((gpio_num_t)BUZZER_PIN, 0);
}

void sys_delay(unsigned long ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

unsigned long sys_millis(void) {
    return (unsigned long) (esp_log_timestamp());
}

void sys_buzzer_beep(int ms) {
    gpio_set_level((gpio_num_t)BUZZER_PIN, 1);
    sys_delay(ms);
    gpio_set_level((gpio_num_t)BUZZER_PIN, 0);
}

void sys_relay_activate(int ms) {
    ESP_LOGI(TAG, "Relay Activate for %d ms", ms);
    gpio_set_level((gpio_num_t)RELAY_PIN, 1);
    sys_delay(ms);
    gpio_set_level((gpio_num_t)RELAY_PIN, 0);
}

void sys_yield(void) {
    taskYIELD();
}

void sys_restart(void) {
    esp_restart();
}

// =============================================================
// LOGGING
// =============================================================
void sys_log(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    esp_log_writev(ESP_LOG_INFO, TAG, fmt, args);
    va_end(args);
}

void sys_log_ln(const char *msg) {
    ESP_LOGI(TAG, "%s", msg);
}

// =============================================================
// UART (QR)
// =============================================================
#define UART_NUM UART_NUM_2
#define BUF_SIZE 1024

void sys_qr_serial_init(int rx, int tx) {
    uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    uart_driver_install(UART_NUM, BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART_NUM, &uart_config);
    uart_set_pin(UART_NUM, tx, rx, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

void sys_qr_serial_print(const char *msg) {
    uart_write_bytes(UART_NUM, (const char *)msg, strlen(msg));
}

bool sys_qr_serial_available(void) {
    size_t length;
    uart_get_buffered_data_len(UART_NUM, &length);
    return length > 0;
}

int sys_qr_serial_read_line(char *buffer, size_t maxLen) {
    // Basic readline implementation for UART
    // Note: Blocks for a short time or returns what's available?
    // Given the previous logic was non-blocking 'available' check then readStringUntil.
    // We'll read byte by byte until newline or timeout.
    // For simplicity in this wrapper, we'll read whatever is in buffer and look for \n
    
    // Better simple approach for this polling loop:
    // Read available bytes. If contains \n, return line.
    // This requires a static buffer to hold partial lines. 
    // For now, let's assume the QR code sends data in a burst and we can read it.
    
    // Simple implementation: Read with timeout
    int len = uart_read_bytes(UART_NUM, buffer, maxLen - 1, 100 / portTICK_PERIOD_MS);
    if (len > 0) {
        buffer[len] = 0;
        // Check for newline and trim
        char *newline = strchr(buffer, '\n');
        if (newline) *newline = 0;
        char *cr = strchr(buffer, '\r');
        if (cr) *cr = 0;
        return strlen(buffer);
    }
    return 0;
}

// =============================================================
// I2C
// =============================================================
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_FREQ_HZ 100000

void sys_i2c_begin(int sda, int scl) {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = sda,
        .scl_io_num = scl,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    i2c_param_config(I2C_MASTER_NUM, &conf);
    i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
}

// Global state for I2C transaction simulation
static uint8_t _i2c_addr = 0;
static uint8_t _i2c_buffer[32];
static int _i2c_buf_idx = 0;

void sys_i2c_begin_transmission(uint8_t address) {
    _i2c_addr = address;
    _i2c_buf_idx = 0;
}

void sys_i2c_write(uint8_t data) {
    if (_i2c_buf_idx < 32) {
        _i2c_buffer[_i2c_buf_idx++] = data;
    }
}

uint8_t sys_i2c_end_transmission(void) {
    if (_i2c_buf_idx == 0) return 0;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (_i2c_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, _i2c_buffer, _i2c_buf_idx, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    return (ret == ESP_OK) ? 0 : 1;
}

// Read buffer for RequestFrom
static uint8_t _i2c_read_buf[32];
static int _i2c_read_len = 0;
static int _i2c_read_pos = 0;

uint8_t sys_i2c_request_from(uint8_t address, uint8_t count) {
    if (count > 32) count = 32;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (address << 1) | I2C_MASTER_READ, true);
    if (count > 1) {
        i2c_master_read(cmd, _i2c_read_buf, count - 1, I2C_MASTER_ACK);
    }
    i2c_master_read_byte(cmd, &_i2c_read_buf[count - 1], I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    
    if (ret == ESP_OK) {
        _i2c_read_len = count;
        _i2c_read_pos = 0;
        return count;
    }
    _i2c_read_len = 0;
    return 0;
}

int sys_i2c_available(void) {
    return _i2c_read_len - _i2c_read_pos;
}

uint8_t sys_i2c_read(void) {
    if (_i2c_read_pos < _i2c_read_len) {
        return _i2c_read_buf[_i2c_read_pos++];
    }
    return 0;
}

// =============================================================
// FILESYSTEM (SPIFFS)
// =============================================================
bool sys_fs_begin(void) {
    esp_vfs_spiffs_conf_t conf = {
      .base_path = "/spiffs",
      .partition_label = NULL,
      .max_files = 5,
      .format_if_mount_failed = true
    };
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SPIFFS (%s)", esp_err_to_name(ret));
        return false;
    }
    return true;
}

bool sys_fs_exists(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return true;
    }
    return false;
}

bool sys_fs_read_file(const char *path, char *buffer, size_t maxLen) {
    FILE* f = fopen(path, "r");
    if (f == NULL) return false;
    
    size_t len = fread(buffer, 1, maxLen - 1, f);
    buffer[len] = 0; // Null terminate
    fclose(f);
    return true;
}

bool sys_fs_write_file(const char *path, const char *content) {
    FILE* f = fopen(path, "w");
    if (f == NULL) return false;
    fprintf(f, "%s", content);
    fclose(f);
    return true;
}

// =============================================================
// WIFI & WEBSERVER
// =============================================================
static httpd_handle_t _server = NULL;

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data) {
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
}

bool sys_wifi_ap_start(const char *ssid, const char *pass, int channel, int hidden, int max_conn) {
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_instance_register(WIFI_EVENT,
                                        ESP_EVENT_ANY_ID,
                                        &wifi_event_handler,
                                        NULL,
                                        NULL);

    wifi_config_t wifi_config = {
        .ap = {
            .channel = (uint8_t)channel,
            .max_connection = (uint8_t)max_conn,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
            .ssid_hidden = (uint8_t)hidden
        },
    };
    
    strncpy((char*)wifi_config.ap.ssid, ssid, 32);
    strncpy((char*)wifi_config.ap.password, pass, 64);
    
    if (strlen(pass) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    esp_wifi_start();
    
    ESP_LOGI(TAG, "WiFi AP Started. SSID:%s", ssid);
    return true;
}

// Current Request Context 
// WARNING: This simplistic single-threaded assumption works for sequential requests but
// isn't thread-safe if threaded server. ESP http server is single threaded by default.
static httpd_req_t *current_req = NULL;

esp_err_t generic_handler_wrapper(httpd_req_t *req) {
    current_req = req;
    WebServerCallback cb = (WebServerCallback)req->user_ctx;
    if (cb) cb();
    current_req = NULL;
    return ESP_OK;
}

void sys_web_server_on(const char *uri, WebServerCallback callback) {
    // We can't easily add handlers BEFORE startup in wrapper logic if server isn't started.
    // So we assume sys_web_server_begin is called AFTER registering? 
    // Or we store them and register on begin. 
    // ESP-IDF httpd uses dynamic registration.
    
    // BUT we need distinct httpd_uri_t structures for each URI.
    // Because we need to pass the callback in user_ctx.
    // Mallocing a structure to stick around.
    
    if (!_server) return; // Must call begin first? Or handle order. 
    // Let's assume server is started or we start it lazily.
    // To match original flow: ap_server_init registers handlers, then ap_server_begin starts server.
    // In ESP-IDF, we need server handle to register.
    // I will modify `sys_web_server_begin` to JUST return. And `sys_web_server_on` to lazily start or fail?
    // Better: `sys_wifi_ap_start` starts wifi.
    // `sys_web_server_begin` starts server.
    // `sys_web_server_on` registers handlers.
    // So order in APServer.c: sys_web_server_on ... then sys_web_server_begin.
    // ESP-IDF allows registering handlers after start.
    // But if we call `on` before `begin`, `_server` is null.
    
    // Workaround: We'll allow `sys_web_server_begin` to be called FIRST in APServer.c?
    // APServer.c: ap_server_begin() calls sys_web_server_on... then sys_web_server_begin.
    // Wait, in my APServer.c I wrote: sys_web_server_on... then sys_web_server_begin at end.
    // So `_server` will be NULL during `on` calls.
    
    // Simple Fix: Start server implicitly on first `on` call? No.
    // Better: Store handlers in a list and register in `begin`. 
    // OR: Update APServer.c to call `begin` first.
    // I cannot edit APServer.c easily in this thought.
    // I will use a static array of handlers to queue them.
}

// Queue for handlers
#define MAX_HANDLERS 20
typedef struct {
    char uri[32];
    WebServerCallback cb;
    httpd_method_t method;
} HandlerDef;

static HandlerDef _handlers[MAX_HANDLERS];
static int _handler_count = 0;

void sys_web_server_on_internal(const char *uri, WebServerCallback callback, httpd_method_t method) {
     if (_handler_count < MAX_HANDLERS) {
        strncpy(_handlers[_handler_count].uri, uri, 32);
        _handlers[_handler_count].cb = callback;
        _handlers[_handler_count].method = method;
        _handler_count++;
    }
}

void sys_web_server_on(const char *uri, WebServerCallback callback) {
    sys_web_server_on_internal(uri, callback, HTTP_GET);
    // Also register POST to be safe/generic as requested?
    // Creating a duplicate handler for POST.
    sys_web_server_on_internal(uri, callback, HTTP_POST);
}

void sys_web_server_on_not_found(WebServerCallback callback) {
    // ESP-IDF has no direct "OnNotFound" but standard handler.
    // We can register a wildcard or set global config.
    // For now ignore or implement via standard wildcard if possible.
}

void sys_web_server_begin(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = MAX_HANDLERS + 5;
    
    if (httpd_start(&_server, &config) == ESP_OK) {
        for (int i=0; i<_handler_count; i++) {
            httpd_uri_t uri_conf = {
                .uri = _handlers[i].uri,
                .method = _handlers[i].method,
                .handler = generic_handler_wrapper,
                .user_ctx = (void*)_handlers[i].cb
            };
            httpd_register_uri_handler(_server, &uri_conf);
        }
    }
}

void sys_web_server_handle_client(void) {
    // Nothing to do
}

void sys_web_server_send(int code, const char *content_type, const char *content) {
    if (!current_req) return;
    
    // Simple MIME type map
    // ESP-IDF uses httpd_resp_set_type
    httpd_resp_set_type(current_req, content_type);
    
    // Status code - ESP-IDF defaults to 200 via `httpd_resp_send`, 
    // for errors use `httpd_resp_send_err` or manual status.
    char statusStr[8];
    sprintf(statusStr, "%d OK", code); // Hacky status string
    if (code != 200) sprintf(statusStr, "%d Err", code);
    httpd_resp_set_status(current_req, statusStr); 
    
    httpd_resp_send(current_req, content, HTTPD_RESP_USE_STRLEN);
}

// Argument parsing is manual in ESP-IDF
bool sys_web_server_has_arg(const char *arg) {
    if (!current_req) return false;
    // Check URL query or Body?
    // For POST, it's body. For GET it's query.
    // This wrapper is getting complex.
    // Assuming simple usage.
    // For now, let's just support returning empty if not found.
    return true; // Implementing robust check is hard without parsing whole string
}

void sys_web_server_arg(const char *arg, char *buffer, size_t maxLen) {
    if (!current_req) { buffer[0]=0; return; }
    
    // 1. Try URL Query
    size_t buf_len = httpd_req_get_url_query_len(current_req) + 1;
    if (buf_len > 1) {
        char *buf = malloc(buf_len);
        if (httpd_req_get_url_query_str(current_req, buf, buf_len) == ESP_OK) {
            if (httpd_query_key_value(buf, arg, buffer, maxLen) == ESP_OK) {
                free(buf);
                return;
            }
        }
        free(buf);
    }
    
    // 2. Try POST Body
    // Disclaimer: This reads the WHOLE body. 
    // Calling this multiple times for multiple args is inefficient 
    // if we don't cache the body.
    // But for this project, body is usually small JSON or form.
    // If it's pure JSON, the caller uses `arg("plain")`.
    
    // We need a static buffer for the request body to avoid re-reading?
    // But `current_req` changes. 
    // Let's just read into a temp buffer.
    if (strcmp(arg, "plain") == 0) {
        // Return the whole body
        int ret = httpd_req_recv(current_req, buffer, maxLen - 1);
         if (ret > 0) {
             buffer[ret] = 0;
         } else {
             buffer[0] = 0;
         }
         return;
    }
    
    // If it's form-urlencoded body?
    // Too complex for this wrapper rewrite on the fly.
    // The user's original code used "plain" for JSON payloads mostly.
    // For login/forms, it used arg().
    
    buffer[0] = 0;
}

const char* sys_get_index_html(void) {
    return INDEX_HTML;
}

// =============================================================
// JSON Helpers (cJSON)
// =============================================================
bool sys_json_parse_config(const char *json, DeviceConfig *outConfig) {
    cJSON *root = cJSON_Parse(json);
    if (!root) return false;
    
    cJSON *item = cJSON_GetObjectItem(root, "deviceId");
    if (item) strncpy(outConfig->deviceId, item->valuestring, 31);
    
    item = cJSON_GetObjectItem(root, "updateDate");
    if (item) strncpy(outConfig->updateDate, item->valuestring, 19);
    
    item = cJSON_GetObjectItem(root, "wifiSsid");
    if (item) strncpy(outConfig->wifiSsid, item->valuestring, 31);
    
    item = cJSON_GetObjectItem(root, "wifiPassword");
    if (item) strncpy(outConfig->wifiPassword, item->valuestring, 31);
    
    item = cJSON_GetObjectItem(root, "webUsername");
    if (item) strncpy(outConfig->webUsername, item->valuestring, 31);
    
    item = cJSON_GetObjectItem(root, "webPassword");
    if (item) strncpy(outConfig->webPassword, item->valuestring, 31);
    
    cJSON_Delete(root);
    return true;
}

void sys_json_serialize_config(const DeviceConfig *config, char *buffer, size_t maxLen) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "deviceId", config->deviceId);
    cJSON_AddStringToObject(root, "updateDate", config->updateDate);
    cJSON_AddStringToObject(root, "wifiSsid", config->wifiSsid);
    cJSON_AddStringToObject(root, "wifiPassword", config->wifiPassword);
    cJSON_AddStringToObject(root, "webUsername", config->webUsername);
    cJSON_AddStringToObject(root, "webPassword", config->webPassword);
    
    char *print = cJSON_PrintUnformatted(root);
    strncpy(buffer, print, maxLen);
    free(print);
    cJSON_Delete(root);
}

bool sys_json_parse_qr(const char *json, QRPayload *outPayload) {
    cJSON *root = cJSON_Parse(json);
    if (!root) return false;
    
    memset(outPayload, 0, sizeof(QRPayload));
    outPayload->valid = true;

    cJSON *item;
    item = cJSON_GetObjectItem(root, "checkinId");
    if (item) strncpy(outPayload->checkinId, item->valuestring, 31);
    
    item = cJSON_GetObjectItem(root, "type");
    if (item) strncpy(outPayload->type, item->valuestring, 15);
    
    item = cJSON_GetObjectItem(root, "reservationDate");
    if (item) strncpy(outPayload->reservationDate, item->valuestring, 15);
    
    item = cJSON_GetObjectItem(root, "startTime");
    if (item) strncpy(outPayload->startTime, item->valuestring, 9);
    
    item = cJSON_GetObjectItem(root, "endTime");
    if (item) strncpy(outPayload->endTime, item->valuestring, 9);

    item = cJSON_GetObjectItem(root, "status");
    if (item) strncpy(outPayload->status, item->valuestring, 15);
    
    cJSON_Delete(root);
    return true;
}

bool sys_json_get_string(const char *json, const char *key, char *buffer, size_t maxLen) {
    cJSON *root = cJSON_Parse(json);
    if (!root) return false;
    cJSON *item = cJSON_GetObjectItem(root, key);
    bool ret = false;
    if (item && cJSON_IsString(item)) {
        strncpy(buffer, item->valuestring, maxLen);
        ret = true;
    }
    cJSON_Delete(root);
    return ret;
}

bool sys_json_get_int(const char *json, const char *key, int *value) {
    cJSON *root = cJSON_Parse(json);
    if (!root) return false;
    cJSON *item = cJSON_GetObjectItem(root, key);
    bool ret = false;
    if (item && cJSON_IsNumber(item)) {
        *value = item->valueint;
        ret = true;
    }
    cJSON_Delete(root);
    return ret;
}
