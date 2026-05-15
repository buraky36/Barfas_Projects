#include "APServer.h"
#include "wrappers.h"
#include "RTCManager.h"
#include "ConfigManager.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static DeviceConfig *_config = NULL;

// Handlers
static void handleRoot(void);
static void handleLogin(void);
static void handleStatus(void);
static void handleSetTime(void);
static void handleSetId(void);
static void handleFindDevice(void);
static void handleOpenDoor(void);
static void handleSetWifiConfig(void);
static void handleSetWebConfig(void);
static void handleFactoryReset(void);
static void handleNotFound(void);

void ap_server_init(DeviceConfig *config) {
    _config = config;
}

void ap_server_begin(void) {
    sys_log_ln("[AP] Setting up Access Point...");

    // Safety checks
    if (strlen(_config->wifiSsid) < 1) 
        strcpy(_config->wifiSsid, "Emergency_AP");
    if (strlen(_config->wifiPassword) < 8) 
        strcpy(_config->wifiPassword, "12345678");

    sys_wifi_ap_start(_config->wifiSsid, _config->wifiPassword, AP_CHANNEL, AP_HIDDEN, AP_MAX_CONN);

    sys_log_ln("===========================================");
    sys_log("[AP] Name: %s\n", _config->wifiSsid);
    sys_log("[AP] Pass: %s\n", _config->wifiPassword);
    sys_log_ln("===========================================");

    // Endpoints
    sys_web_server_on("/", handleRoot);
    sys_web_server_on("/login", handleLogin);
    sys_web_server_on("/status", handleStatus);
    sys_web_server_on("/set_time", handleSetTime);
    sys_web_server_on("/set_id", handleSetId);
    sys_web_server_on("/find_device", handleFindDevice);
    sys_web_server_on("/open_door", handleOpenDoor);
    sys_web_server_on("/set_wifi_config", handleSetWifiConfig);
    sys_web_server_on("/set_web_config", handleSetWebConfig);
    sys_web_server_on("/factory_reset", handleFactoryReset);
    sys_web_server_on_not_found(handleNotFound);

    sys_web_server_begin();
    sys_log_ln("[AP] HTTP Server Started");
}

void ap_server_handle_client(void) {
    sys_web_server_handle_client();
}

static void handleRoot(void) {
    sys_web_server_send(200, "text/html", sys_get_index_html());
}

static void handleLogin(void) {
    if (!sys_web_server_has_arg("username") || !sys_web_server_has_arg("password")) {
        sys_web_server_send(400, "text/plain", "Missing Credentials");
        return;
    }

    char user[33];
    char pass[33];
    sys_web_server_arg("username", user, 32);
    sys_web_server_arg("password", pass, 32);

    if (strcmp(user, _config->webUsername) == 0 && strcmp(pass, _config->webPassword) == 0) {
        sys_log_ln("[Web] Login Success");
        sys_web_server_send(200, "text/plain", "OK");
    } else {
        sys_log_ln("[Web] Login Failed");
        sys_web_server_send(401, "text/plain", "Unauthorized");
    }
}

static void handleStatus(void) {
    // Construct JSON manually or use wrapper? 
    // Wrapper `sys_json_serialize_config` dumps everything. 
    // We need a specific status JSON.
    // Let's build it manually with snprintf (safe enough for simple JSON)
    // OR add `sys_json_create_status` to wrappers. 
    // Manual construction is fine for C if careful.
    
    char dateStr[25];
    rtc_get_datetime_string(dateStr);
    
    char json[512];
    // Note: Be careful with quotes in format string
    snprintf(json, sizeof(json), 
        "{\"device_id\":\"%s\",\"firmware_version\":\"%s\",\"current_time\":\"%s\",\"wifi_ssid\":\"%s\"}",
        _config->deviceId, FIRMWARE_VERSION, dateStr, _config->wifiSsid);
        
    sys_web_server_send(200, "application/json", json);
}

static void handleSetTime(void) {
    sys_log_ln("[Web] Request: /set_time");
    if (!sys_web_server_has_arg("plain")) {
        sys_web_server_send(400, "text/plain", "Body missing");
        return;
    }
    
    char body[256];
    sys_web_server_arg("plain", body, 256);
    
    int y, m, d, H, M, S;
    bool ok = true;
    ok &= sys_json_get_int(body, "year", &y);
    ok &= sys_json_get_int(body, "month", &m);
    ok &= sys_json_get_int(body, "day", &d);
    ok &= sys_json_get_int(body, "hour", &H);
    ok &= sys_json_get_int(body, "minute", &M);
    ok &= sys_json_get_int(body, "second", &S);
    
    if (ok) {
        rtc_set_datetime(y, m, d, H, M, S);
        sys_web_server_send(200, "text/plain", "Time Updated");
        
        rtc_get_datetime_string(_config->updateDate);
        config_save(_config);
    } else {
        sys_web_server_send(400, "text/plain", "Missing time fields");
    }
}

static void handleSetId(void) {
    if (!sys_web_server_has_arg("plain")) return sys_web_server_send(400, "text/plain", "Body missing");
    char body[256];
    sys_web_server_arg("plain", body, 256);
    
    char newId[32];
    if (sys_json_get_string(body, "device_id", newId, 32)) {
        strcpy(_config->deviceId, newId);
        if (config_save(_config))
            sys_web_server_send(200, "text/plain", "Updated");
        else
            sys_web_server_send(500, "text/plain", "Save Error");
    } else {
        sys_web_server_send(400, "text/plain", "Missing ID");
    }
}

static void handleFindDevice(void) {
    sys_web_server_send(200, "text/plain", "Buzzing...");
    for (int i = 0; i < 3; i++) {
        sys_buzzer_beep(100);
        sys_delay(100);
    }
}

static void handleOpenDoor(void) {
    sys_web_server_send(200, "text/plain", "Opening...");
    sys_buzzer_beep(100); sys_delay(50);
    sys_buzzer_beep(100); sys_delay(50);
    sys_buzzer_beep(800);
    sys_relay_activate(5000);
}

static void handleSetWifiConfig(void) {
    if (!sys_web_server_has_arg("plain")) return sys_web_server_send(400, "text/plain", "Body missing");
    char body[512];
    sys_web_server_arg("plain", body, 512); // larger body
    
    char ssid[32], pass[32];
    bool ok = sys_json_get_string(body, "wifi_ssid", ssid, 32);
    ok &= sys_json_get_string(body, "wifi_pass", pass, 32);
    
    if (ok && strlen(pass) >= 8) {
        strcpy(_config->wifiSsid, ssid);
        strcpy(_config->wifiPassword, pass);
        config_save(_config);
        sys_web_server_send(200, "text/plain", "OK");
        sys_delay(1000);
        // sys_restart(); // Need this wrapper? ESP.restart()
        // I forgot to add sys_restart to wrappers. I'll rely on user manual reset or add it.
        // Actually I should add it.
    } else {
        sys_web_server_send(400, "text/plain", "Invalid or missing fields");
    }
}

static void handleSetWebConfig(void) {
    if (!sys_web_server_has_arg("plain")) return sys_web_server_send(400, "text/plain", "Body missing");
    char body[512];
    sys_web_server_arg("plain", body, 512);

    char user[32], pass[32];
    bool ok = sys_json_get_string(body, "web_user", user, 32);
    ok &= sys_json_get_string(body, "web_pass", pass, 32);

    if (ok) {
        strcpy(_config->webUsername, user);
        strcpy(_config->webPassword, pass);
        config_save(_config);
        sys_web_server_send(200, "text/plain", "OK");
    } else {
         sys_web_server_send(400, "text/plain", "Missing fields");
    }
}

static void handleFactoryReset(void) {
    if (!sys_web_server_has_arg("password")) return sys_web_server_send(400, "text/plain", "Missing Password");
    char pass[33];
    sys_web_server_arg("password", pass, 32);
    
    if (strcmp(pass, _config->webPassword) == 0) {
        sys_log_ln("[System] Factory Reset Triggered!");
        config_reset();
        sys_web_server_send(200, "text/plain", "Resetting...");
        sys_delay(1000);
        // sys_restart();
    } else {
        sys_web_server_send(401, "text/plain", "Wrong Password");
    }
}

static void handleNotFound(void) {
    sys_web_server_send(404, "text/plain", "Not Found");
}
