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

// Last decoded weather frame, surfaced on the local status page. The Davis
// decoder is still a stub, so this stays unavailable until it is ported.
struct WeatherSnapshot {
  bool available = false;
  int64_t observedAtEpoch = 0;
  float temperatureC = 0.0f;
  float humidityPct = 0.0f;
  float pressureHpa = 0.0f;
  float inTemperatureC = 0.0f;
  float inHumidityPct = 0.0f;
  float windSpeedMs = 0.0f;
  float windDirectionDeg = 0.0f;
  float windGustMs = 0.0f;
  float rainDayMm = 0.0f;
};

// Local admin surface, reachable on the provisioning AP. The AP is the
// security boundary; the page exposes no cloud token and only saves the
// settings shown on it.
class WebConfigServer {
 public:
  WebConfigServer(DeviceSettings& settings, SettingsStore& store, WifiService& wifi,
                  TimeService& time);
  ~WebConfigServer();

  Status begin();
  void loop();

  void setSubsystemStatus(bool radioReady, bool archiveReady);
  void setCounters(uint32_t queuedUploads, uint32_t recordsToday);
  void setWeather(const WeatherSnapshot& weather);

 private:
  void handleRoot();
  void handleNotFound();
  void handleStatus();
  void handleGetSettings();
  void handleSaveSettings();
  void handleReboot();
  void scheduleRestart(uint32_t delayMs);
  void sendJson(int code, const String& body);

  DeviceSettings& settings_;
  SettingsStore& store_;
  WifiService& wifi_;
  TimeService& time_;
  WebServer server_;
  WeatherSnapshot weather_;
  bool radioReady_ = false;
  bool archiveReady_ = false;
  uint32_t queuedUploads_ = 0;
  uint32_t recordsToday_ = 0;
  bool restartScheduled_ = false;
  uint32_t restartAtMs_ = 0;
};

}  // namespace akashvani
