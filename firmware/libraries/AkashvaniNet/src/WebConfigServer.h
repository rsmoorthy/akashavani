#pragma once

#include <Arduino.h>
#include <WebServer.h>

#include <cstdint>

#include "DeviceSettings.h"
#include "SettingsStore.h"
#include "Status.h"
#include "TimeService.h"
#include "WifiService.h"

namespace akashvani {

// Local admin surface, reachable on the provisioning AP.
// All mutating endpoints require the X-Admin-Token header.
class WebConfigServer {
 public:
  WebConfigServer(DeviceSettings& settings, SettingsStore& store, WifiService& wifi,
                  TimeService& time);
  ~WebConfigServer();

  Status begin();
  void loop();

  void setSubsystemStatus(bool radioReady, bool archiveReady);
  void setCounters(uint32_t queuedUploads, uint32_t recordsToday);

 private:
  void handleRoot();
  void handleNotFound();
  void handleStatus();
  void handleGetSettings();
  void handleSaveSettings();
  void handleReboot();

  bool authorized();
  bool rejectUnauthorized();
  void sendJson(int code, const String& body);

  DeviceSettings& settings_;
  SettingsStore& store_;
  WifiService& wifi_;
  TimeService& time_;
  WebServer server_;
  bool radioReady_ = false;
  bool archiveReady_ = false;
  uint32_t queuedUploads_ = 0;
  uint32_t recordsToday_ = 0;
};

}  // namespace akashvani
