#ifndef WRAPPERS_H
#define WRAPPERS_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "Config.h" // For struct DeviceConfig

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================
// SYSTEM & GPIO
// =============================================================
void sys_init_pins(void);
void sys_delay(unsigned long ms);
unsigned long sys_millis(void);
void sys_buzzer_beep(int ms);
void sys_relay_activate(int ms);
void sys_yield(void);
void sys_restart(void);

// =============================================================
// LOGGING
// =============================================================
void sys_log(const char *fmt, ...);
void sys_log_ln(const char *msg);

// =============================================================
// UART (QR) - Replaces SoftwareSerial
// =============================================================
void sys_qr_serial_init(int rx, int tx);
void sys_qr_serial_print(const char *msg);
bool sys_qr_serial_available(void);
int sys_qr_serial_read_line(char *buffer, size_t maxLen);

// =============================================================
// I2C
// =============================================================
void sys_i2c_begin(int sda, int scl);
void sys_i2c_begin_transmission(uint8_t address);
void sys_i2c_write(uint8_t data);
uint8_t sys_i2c_end_transmission(void);
uint8_t sys_i2c_request_from(uint8_t address, uint8_t count);
int sys_i2c_available(void);
uint8_t sys_i2c_read(void);

// =============================================================
// FILESYSTEM (SPIFFS / LittleFS)
// =============================================================
bool sys_fs_begin(void);
bool sys_fs_exists(const char *path);
bool sys_fs_read_file(const char *path, char *buffer, size_t maxLen);
bool sys_fs_write_file(const char *path, const char *content);

// =============================================================
// WIFI & WEBSERVER
// =============================================================
bool sys_wifi_ap_start(const char *ssid, const char *pass, int channel, int hidden, int max_conn);

// Callback type adapted for ESP-IDF
// ESP-IDF handlers take `httpd_req_t *req`.
// However, to keep APServer.c generic, we will stick to void/void callback
// and manage the request context internally in wrappers.c (using a static current request pointer).
typedef void (*WebServerCallback)(void);

void sys_web_server_on(const char *uri, WebServerCallback callback); // GET by default in simple wrapper
void sys_web_server_on_post(const char *uri, WebServerCallback callback); // Explicit POST needed?
// Original APServer.c used sys_web_server_on for both, but `wrappers.cpp` mapped them.
// APServer.c does `sys_web_server_on("/login", handleLogin)` but expects it to handle POST?
// Let's modify wrappers.c to check method or map correctly.
// Actually, APServer.c doesn't specify method in call to `sys_web_server_on`. 
// The original C++ `_server.on("/login", HTTP_POST, ...)` had explicit method.
// My previous C conversion `sys_web_server_on` didn't take method arg.
// I should update `wrappers.h` to accept method or assume GET/POST mixed?
// ESP-IDF is strict. I will add `sys_web_server_on_post`.
// But wait, `APServer.c` code I wrote calls `sys_web_server_on` for EVERYTHING.
// I should update `wrappers.c` to register generic handlers or update `APServer.c`?
// I'll update `APServer.c` later if needed, OR I can register BOTH GET and POST for the same URI in wrappers.c
// to mimic "any" handler if possible, or just register based on known paths.
// Better: make wrappers generic.

void sys_web_server_on_not_found(WebServerCallback callback);
void sys_web_server_begin(void);
void sys_web_server_handle_client(void); // No-op in ESP-IDF (async), but needed for compat

void sys_web_server_send(int code, const char *content_type, const char *content);
bool sys_web_server_has_arg(const char *arg);
void sys_web_server_arg(const char *arg, char *buffer, size_t maxLen);

// =============================================================
// JSON Helpers (cJSON based)
// =============================================================
bool sys_json_parse_config(const char *json, struct DeviceConfig *outConfig);
void sys_json_serialize_config(const struct DeviceConfig *config, char *buffer, size_t maxLen);

typedef struct QRPayload {
    char checkinId[32];
    char type[16];
    char reservationDate[16];
    char startTime[10];
    char endTime[10];
    char status[16];
    bool valid; 
} QRPayload;

bool sys_json_parse_qr(const char *json, struct QRPayload *outPayload);
bool sys_json_get_string(const char *json, const char *key, char *buffer, size_t maxLen);
bool sys_json_get_int(const char *json, const char *key, int *value);

// =============================================================
// RESOURCES
// =============================================================
const char* sys_get_index_html(void);

#ifdef __cplusplus
}
#endif

#endif
