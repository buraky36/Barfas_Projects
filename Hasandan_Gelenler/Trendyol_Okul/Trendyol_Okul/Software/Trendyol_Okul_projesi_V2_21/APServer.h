#ifndef APSERVER_H
#define APSERVER_H

#include "Config.h"
#include "ConfigManager.h"
#include "RTCManager.h"
#include <ArduinoJson.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>


class APServer {
public:
  APServer(ConfigManager &configMgr, RTCManager &rtcMgr, DeviceConfig &config);
  void begin();
  void handleClient();

private:
  ESP8266WebServer _server;
  ConfigManager &_configMgr;
  RTCManager &_rtcMgr;
  DeviceConfig &_config;

  void handleRoot();
  void handlePublicInfo(); // New: To show username on login page
  void handleLogin();
  void handleStatus();

  // Actions
  void handleSetTime();
  void handleSetId();
  void handleFindDevice();
  void handleOpenDoor();

  // Config Changes
  void handleSetWifiConfig(); // Replaces set_wifi_pass
  void handleSetWebConfig();
  void handleFactoryReset();

  void handleNotFound();
};

#endif
