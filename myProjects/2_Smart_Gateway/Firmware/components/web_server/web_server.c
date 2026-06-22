#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "config_manager.h"
#include "web_server.h"

static const char *TAG = "WEB_SERVER";
static httpd_handle_t server = NULL;

// Beautiful glassmorphic dark mode setup interface
static const char* setup_html = 
"<!DOCTYPE html>"
"<html>"
"<head>"
"<meta charset=\"utf-8\">"
"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
"<title>Barfas Smart Gateway Setup</title>"
"<style>"
"  body {"
"    background: linear-gradient(135deg, #0f172a, #1e1b4b);"
"    color: #f8fafc;"
"    font-family: 'Segoe UI', -apple-system, Roboto, sans-serif;"
"    display: flex;"
"    justify-content: center;"
"    align-items: center;"
"    min-height: 100vh;"
"    margin: 0;"
"  }"
"  .card {"
"    background: rgba(30, 41, 59, 0.45);"
"    backdrop-filter: blur(16px);"
"    -webkit-backdrop-filter: blur(16px);"
"    border: 1px solid rgba(255, 255, 255, 0.08);"
"    border-radius: 20px;"
"    padding: 32px;"
"    width: 100%;"
"    max-width: 400px;"
"    box-shadow: 0 20px 40px -15px rgba(0, 0, 0, 0.5);"
"    box-sizing: border-box;"
"  }"
"  h2 {"
"    text-align: center;"
"    margin-top: 0;"
"    margin-bottom: 24px;"
"    font-weight: 700;"
"    font-size: 24px;"
"    letter-spacing: -0.025em;"
"    background: linear-gradient(to right, #38bdf8, #818cf8);"
"    -webkit-background-clip: text;"
"    -webkit-text-fill-color: transparent;"
"  }"
"  .input-group {"
"    margin-bottom: 18px;"
"  }"
"  label {"
"    display: block;"
"    font-size: 13px;"
"    font-weight: 600;"
"    margin-bottom: 6px;"
"    color: #94a3b8;"
"    text-transform: uppercase;"
"    letter-spacing: 0.05em;"
"  }"
"  input {"
"    width: 100%;"
"    padding: 12px 14px;"
"    background: rgba(15, 23, 42, 0.5);"
"    border: 1px solid rgba(255, 255, 255, 0.1);"
"    border-radius: 10px;"
"    color: #f8fafc;"
"    font-size: 15px;"
"    box-sizing: border-box;"
"    transition: all 0.25s ease;"
"  }"
"  input:focus {"
"    outline: none;"
"    border-color: #6366f1;"
"    background: rgba(15, 23, 42, 0.8);"
"    box-shadow: 0 0 0 3px rgba(99, 102, 241, 0.25);"
"  }"
"  button {"
"    width: 100%;"
"    padding: 14px;"
"    background: linear-gradient(135deg, #4f46e5, #6366f1);"
"    border: none;"
"    border-radius: 10px;"
"    color: white;"
"    font-weight: 600;"
"    font-size: 16px;"
"    cursor: pointer;"
"    margin-top: 12px;"
"    box-shadow: 0 4px 14px rgba(99, 102, 241, 0.4);"
"    transition: all 0.2s ease;"
"  }"
"  button:hover {"
"    transform: translateY(-1.5px);"
"    box-shadow: 0 6px 20px rgba(99, 102, 241, 0.5);"
"  }"
"  button:active {"
"    transform: translateY(0.5px);"
"  }"
"  .footer {"
"    text-align: center;"
"    margin-top: 28px;"
"    font-size: 11px;"
"    color: #64748b;"
"    text-transform: uppercase;"
"    letter-spacing: 0.1em;"
"  }"
"</style>"
"</head>"
"<body>"
"<div class=\"card\">"
"  <h2>Smart Gateway Setup</h2>"
"  <form action=\"/config\" method=\"POST\">"
"    <div class=\"input-group\">"
"      <label for=\"ssid\">Wi-Fi SSID</label>"
"      <input type=\"text\" id=\"ssid\" name=\"ssid\" required placeholder=\"Wi-Fi ağ adı kiriniz\">"
"    </div>"
"    <div class=\"input-group\">"
"      <label for=\"pass\">Wi-Fi Şifresi</label>"
"      <input type=\"password\" id=\"pass\" name=\"pass\" placeholder=\"Şifre giriniz\">"
"    </div>"
"    <div class=\"input-group\">"
"      <label for=\"mqtt_uri\">MQTT Broker URI</label>"
"      <input type=\"text\" id=\"mqtt_uri\" name=\"mqtt_uri\" required placeholder=\"mqtt://broker.hivemq.com\">"
"    </div>"
"    <div class=\"input-group\">"
"      <label for=\"mqtt_port\">MQTT Port</label>"
"      <input type=\"number\" id=\"mqtt_port\" name=\"mqtt_port\" value=\"1883\" required>"
"    </div>"
"    <div class=\"input-group\">"
"      <label for=\"device_id\">Cihaz (Device) ID</label>"
"      <input type=\"text\" id=\"device_id\" name=\"device_id\" required placeholder=\"gateway_001\">"
"    </div>"
"    <button type=\"submit\">Kaydet ve Bağlan</button>"
"  </form>"
"  <div class=\"footer\">Barfas Smart Gateway © 2026</div>"
"</div>"
"</body>"
"</html>";



// URL decoding function (safe for in-place decoding)
static void url_decode(char *dst, const char *src)
{
    char a, b;
    while (*src) {
        if ((*src == '%') &&
            ((a = src[1]) && (b = src[2])) &&
            (isxdigit((int)a) && isxdigit((int)b))) {
            if (a >= 'a') a -= 'a'-'A';
            if (a >= 'A') a -= ('A' - 10);
            else a -= '0';
            if (b >= 'a') b -= 'a'-'A';
            if (b >= 'A') b -= ('A' - 10);
            else b -= '0';
            *dst++ = 16 * a + b;
            src += 3;
        } else if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

static esp_err_t get_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "GET Request received at '%s'", req->uri);
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, setup_html, HTTPD_RESP_USE_STRLEN);
}

// Delayed restart task to allow HTTP response to finish sending
static void restart_task(void *pvParameter)
{
    ESP_LOGI(TAG, "Rebooting system in 2 seconds...");
    vTaskDelay(pdMS_TO_TICKS(2000));
    ESP_LOGI(TAG, "Rebooting now!");
    esp_restart();
    vTaskDelete(NULL);
}

static esp_err_t post_config_handler(httpd_req_t *req)
{
    char content[512];
    size_t recv_size = req->content_len;
    
    if (recv_size >= sizeof(content)) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "POST Content too long");
        return ESP_FAIL;
    }

    int ret = httpd_req_recv(req, content, recv_size);
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }
    content[recv_size] = '\0';
    
    ESP_LOGI(TAG, "POST Request received. Content: %s", content);

    gateway_config_t new_config;
    memset(&new_config, 0, sizeof(gateway_config_t));
    
    char temp_port[10] = {0};
    
    if (httpd_query_key_value(content, "ssid", new_config.wifi_ssid, sizeof(new_config.wifi_ssid)) == ESP_OK) {
        url_decode(new_config.wifi_ssid, new_config.wifi_ssid);
    }
    if (httpd_query_key_value(content, "pass", new_config.wifi_pass, sizeof(new_config.wifi_pass)) == ESP_OK) {
        url_decode(new_config.wifi_pass, new_config.wifi_pass);
    }
    if (httpd_query_key_value(content, "mqtt_uri", new_config.mqtt_uri, sizeof(new_config.mqtt_uri)) == ESP_OK) {
        url_decode(new_config.mqtt_uri, new_config.mqtt_uri);
    }
    if (httpd_query_key_value(content, "mqtt_port", temp_port, sizeof(temp_port)) == ESP_OK) {
        url_decode(temp_port, temp_port);
    }
    if (httpd_query_key_value(content, "device_id", new_config.device_id, sizeof(new_config.device_id)) == ESP_OK) {
        url_decode(new_config.device_id, new_config.device_id);
    }
    
    new_config.mqtt_port = atoi(temp_port);
    new_config.is_configured = true;

    ESP_LOGI(TAG, "Parsed configuration parameters:");
    ESP_LOGI(TAG, "  SSID     : %s", new_config.wifi_ssid);
    ESP_LOGI(TAG, "  MQTT URI : %s", new_config.mqtt_uri);
    ESP_LOGI(TAG, "  MQTT Port: %d", new_config.mqtt_port);
    ESP_LOGI(TAG, "  Device ID: %s", new_config.device_id);

    // Save parameters to NVS
    esp_err_t err = config_manager_save(&new_config);
    if (err == ESP_OK) {
        const char *response = 
            "<html><head><meta charset=\"utf-8\"><title>Setup Success</title>"
            "<style>body{background:#0f172a;color:#f8fafc;font-family:sans-serif;text-align:center;padding-top:10%;}"
            "div{background:rgba(30,41,59,0.5);display:inline-block;padding:30px;border-radius:15px;border:1px solid rgba(255,255,255,0.1);}"
            "h2{color:#38bdf8;}</style></head><body>"
            "<div><h2>Ayarlar Başarıyla Kaydedildi!</h2>"
            "<p>Gateway yeniden başlatılıyor ve Wi-Fi ağına bağlanmaya çalışacak.</p>"
            "<p>Lütfen tarayıcınızı kapatıp cihazın durum LED'lerini kontrol edin.</p></div>"
            "</body></html>";
        httpd_resp_set_type(req, "text/html");
        httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
        
        // Spawn restart task
        xTaskCreate(&restart_task, "restart_task", 2048, NULL, 5, NULL);
    } else {
        ESP_LOGE(TAG, "Failed to save configuration: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Config Save Failed");
    }

    return ESP_OK;
}


// GET handler for /config (Redirects to / to avoid 405 error if user refreshes or visits via history)
static esp_err_t get_config_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "GET Request received at '/config', redirecting to '/'");
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/");
    return httpd_resp_send(req, NULL, 0);
}

// Captive Portal catch-all for unknown paths (404)
static esp_err_t not_found_handler(httpd_req_t *req, httpd_err_code_t err)
{
    ESP_LOGW(TAG, "404 Not Found at '%s'. Redirecting to '/'", req->uri);
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/");
    return httpd_resp_send(req, NULL, 0);
}

// Captive Portal catch-all for unknown methods (405) like HEAD
static esp_err_t method_not_allowed_handler(httpd_req_t *req, httpd_err_code_t err)
{
    ESP_LOGW(TAG, "405 Method Not Allowed at '%s'. Redirecting to '/'", req->uri);
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/");
    return httpd_resp_send(req, NULL, 0);
}

esp_err_t web_server_start(void)
{
    if (server != NULL) {
        ESP_LOGW(TAG, "Server already running.");
        return ESP_OK;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 4096;

    ESP_LOGI(TAG, "Starting HTTP Web Server on port %d...", config.server_port);
    esp_err_t err = httpd_start(&server, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(err));
        return err;
    }

    // Register handlers
    httpd_uri_t get_uri = {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = get_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(server, &get_uri);

    httpd_uri_t post_config_uri = {
        .uri       = "/config",
        .method    = HTTP_POST,
        .handler   = post_config_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(server, &post_config_uri);

    httpd_uri_t get_config_uri = {
        .uri       = "/config",
        .method    = HTTP_GET,
        .handler   = get_config_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(server, &get_config_uri);

    httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, not_found_handler);
    httpd_register_err_handler(server, HTTPD_405_METHOD_NOT_ALLOWED, method_not_allowed_handler);

    ESP_LOGI(TAG, "HTTP Web Server started successfully.");
    return ESP_OK;
}

esp_err_t web_server_stop(void)
{
    if (server == NULL) {
        return ESP_OK;
    }
    ESP_LOGI(TAG, "Stopping HTTP Web Server...");
    esp_err_t err = httpd_stop(server);
    if (err == ESP_OK) {
        server = NULL;
        ESP_LOGI(TAG, "HTTP Web Server stopped.");
    } else {
        ESP_LOGE(TAG, "Failed to stop HTTP server: %s", esp_err_to_name(err));
    }
    return err;
}
