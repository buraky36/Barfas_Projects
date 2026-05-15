#include "APServer.h"
#include "index_html.h"

APServer::APServer(ConfigManager &configMgr, RTCManager &rtcMgr,
                   DeviceConfig &config)
    : _server(80), _configMgr(configMgr), _rtcMgr(rtcMgr), _config(config) {}

void APServer::begin() {
  Serial.println("[AP] Setting up Access Point...");

  // Use Configured SSID and PASS
  WiFi.mode(WIFI_AP);

  // Safety check just in case config strings are broken
  if (strlen(_config.wifiSsid) < 1)
    strcpy(_config.wifiSsid, "Emergency_AP");
  if (strlen(_config.wifiPassword) < 8)
    strcpy(_config.wifiPassword, "12345678");

  WiFi.softAP(_config.wifiSsid, _config.wifiPassword, AP_CHANNEL, AP_HIDDEN,
              AP_MAX_CONN);

  Serial.println("===========================================");
  Serial.printf("   AP Name:     %s\n", _config.wifiSsid);
  Serial.printf("   AP Pass:     %s\n", _config.wifiPassword);
  Serial.print("   AP IP:       ");
  Serial.println(WiFi.softAPIP());
  Serial.println("===========================================");

  // Endpoints
  _server.on("/", HTTP_GET, std::bind(&APServer::handleRoot, this));
  // Public Info endpoint removed as username is no longer public
  _server.on("/login", HTTP_POST, std::bind(&APServer::handleLogin, this));

  _server.on("/status", HTTP_GET, std::bind(&APServer::handleStatus, this));
  _server.on("/set_time", HTTP_POST, std::bind(&APServer::handleSetTime, this));
  _server.on("/set_id", HTTP_POST, std::bind(&APServer::handleSetId, this));

  _server.on("/find_device", HTTP_POST,
             std::bind(&APServer::handleFindDevice, this));
  _server.on("/open_door", HTTP_POST,
             std::bind(&APServer::handleOpenDoor, this));

  _server.on("/set_wifi_config", HTTP_POST,
             std::bind(&APServer::handleSetWifiConfig, this));
  _server.on("/set_web_config", HTTP_POST,
             std::bind(&APServer::handleSetWebConfig, this));
  _server.on("/factory_reset", HTTP_POST,
             std::bind(&APServer::handleFactoryReset, this));

  _server.onNotFound(std::bind(&APServer::handleNotFound, this));

  _server.begin();
  Serial.println("[AP] HTTP Server Started");
}

void APServer::handleClient() { _server.handleClient(); }

void APServer::handleRoot() { _server.send(200, "text/html", INDEX_HTML); }

void APServer::handleLogin() {
  if (!_server.hasArg("username") || !_server.hasArg("password")) {
    return _server.send(400, "text/plain", "Missing Credentials");
  }

  String user = _server.arg("username");
  String pass = _server.arg("password");

  if (user == _config.webUsername && pass == _config.webPassword) {
    Serial.println("[Web] Login Success");
    _server.send(200, "text/plain", "OK");
  } else {
    Serial.println("[Web] Login Failed");
    _server.send(401, "text/plain", "Unauthorized");
  }
}

void APServer::handleStatus() {
  StaticJsonDocument<512> doc;
  doc["device_id"] = _config.deviceId;
  doc["firmware_version"] = FIRMWARE_VERSION;
  doc["current_time"] = _rtcMgr.getDateTimeString();

  // Send current WiFi SSID (but not password)
  doc["wifi_ssid"] = _config.wifiSsid;

  String response;
  serializeJson(doc, response);
  _server.send(200, "application/json", response);
}

void APServer::handleSetWifiConfig() {
  if (!_server.hasArg("plain"))
    return _server.send(400, "text/plain", "Body missing");

  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, _server.arg("plain"));
  if (error)
    return _server.send(400, "text/plain", "Invalid JSON");

  if (doc.containsKey("wifi_ssid") && doc.containsKey("wifi_pass")) {
    const char *s = doc["wifi_ssid"];
    const char *p = doc["wifi_pass"];

    if (strlen(p) < 8)
      return _server.send(400, "text/plain", "Pass too short");

    strlcpy(_config.wifiSsid, s, sizeof(_config.wifiSsid));
    strlcpy(_config.wifiPassword, p, sizeof(_config.wifiPassword));

    _configMgr.saveConfig(_config);
    _server.send(200, "text/plain", "OK");

    delay(1000);
    ESP.restart();
  } else {
    _server.send(400, "text/plain", "Missing fields");
  }
}

void APServer::handleSetWebConfig() {
  if (!_server.hasArg("plain"))
    return _server.send(400, "text/plain", "Body missing");

  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, _server.arg("plain"));
  if (error)
    return _server.send(400, "text/plain", "Invalid JSON");

  if (doc.containsKey("web_user") && doc.containsKey("web_pass")) {
    strlcpy(_config.webUsername, doc["web_user"], sizeof(_config.webUsername));
    strlcpy(_config.webPassword, doc["web_pass"], sizeof(_config.webPassword));

    _configMgr.saveConfig(_config);
    _server.send(200, "text/plain", "OK");
  } else {
    _server.send(400, "text/plain", "Missing fields");
  }
}

void APServer::handleFactoryReset() {
  if (!_server.hasArg("password"))
    return _server.send(400, "text/plain", "Missing Password");

  String pass = _server.arg("password");
  if (pass == _config.webPassword) {
    Serial.println("[System] Factory Reset Triggered!");
    _configMgr.resetConfig();
    _server.send(200, "text/plain", "Resetting...");
    delay(1000);
    ESP.restart();
  } else {
    _server.send(401, "text/plain", "Wrong Password");
  }
}

void APServer::handleSetTime() {
  Serial.println("[Web] Request: /set_time");
  if (!_server.hasArg("plain"))
    return _server.send(400, "text/plain", "Body missing");
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, _server.arg("plain"));
  if (error)
    return _server.send(400, "text/plain", "Invalid JSON");

  if (doc.containsKey("year") && doc.containsKey("month")) {
    _rtcMgr.setDateTime(doc["year"], doc["month"], doc["day"], doc["hour"],
                        doc["minute"], doc["second"]);
    _server.send(200, "text/plain", "Time Updated");
    String dateStr = _rtcMgr.getDateTimeString();
    dateStr.toCharArray(_config.updateDate, sizeof(_config.updateDate));
    _configMgr.saveConfig(_config);
  } else
    _server.send(400, "text/plain", "Missing time fields");
}

void APServer::handleSetId() {
  if (!_server.hasArg("plain"))
    return _server.send(400, "text/plain", "Body missing");
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, _server.arg("plain"));
  if (error)
    return _server.send(400, "text/plain", "Invalid JSON");

  if (doc.containsKey("device_id")) {
    strlcpy(_config.deviceId, doc["device_id"], sizeof(_config.deviceId));
    if (_configMgr.saveConfig(_config))
      _server.send(200, "text/plain", "Updated");
    else
      _server.send(500, "text/plain", "Save Error");
  } else
    _server.send(400, "text/plain", "Missing ID");
}

void APServer::handleFindDevice() {
  _server.send(200, "text/plain", "Buzzing...");
  for (int i = 0; i < 3; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(100);
    digitalWrite(BUZZER_PIN, LOW);
    delay(100);
  }
}

void APServer::handleOpenDoor() {
  _server.send(200, "text/plain", "Opening...");
  digitalWrite(BUZZER_PIN, HIGH);
  delay(100);
  digitalWrite(BUZZER_PIN, LOW);
  delay(50);
  digitalWrite(BUZZER_PIN, HIGH);
  delay(100);
  digitalWrite(BUZZER_PIN, LOW);
  delay(50);
  digitalWrite(BUZZER_PIN, HIGH);
  delay(800);
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(RELAY_PIN, HIGH);
  delay(5000);
  digitalWrite(RELAY_PIN, LOW);
}

void APServer::handleNotFound() {
  _server.send(404, "text/plain", "Not Found");
}
