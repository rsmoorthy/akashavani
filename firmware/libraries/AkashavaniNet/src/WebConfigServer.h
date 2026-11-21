#pragma once

#include <Arduino.h>
#include <WebServer.h>

#include <cstdint>

#include "DeviceSettings.h"
#include "SettingsStore.h"
#include "Status.h"
#include "TimeService.h"
#include "WifiService.h"

namespace akashavani {

// One last-known weather reading. `updatedAtMs` is a millis() timestamp so the
// page can show relative ages without depending on a valid wall clock.
struct WeatherReading {
  bool valid = false;
  float value = 0.0f;
  uint32_t updatedAtMs = 0;
};

// Last known weather values, surfaced on the local status page. The flat fields
// describe the most recent frame; the `WeatherReading` members cache the latest
// value of each parameter because Davis frames carry one message-specific field
// at a time.
struct WeatherSnapshot {
  bool available = false;
  int64_t observedAtEpoch = 0;
  uint32_t observedAtMs = 0;
  float temperatureC = 0.0f;
  float humidityPct = 0.0f;
  float pressureHpa = 0.0f;
  float inTemperatureC = 0.0f;
  float inHumidityPct = 0.0f;
  float windSpeedMs = 0.0f;
  float windDirectionDeg = 0.0f;
  float windGustMs = 0.0f;
  float rainRateMmH = 0.0f;
  WeatherReading temperature;
  WeatherReading inTemperature;
  WeatherReading windSpeed;
  WeatherReading rainRate;
  uint32_t framesExpected = 0;
  uint32_t framesValid = 0;
  uint32_t framesInvalid = 0;
  // Current radio RSSI (bucket average, or the last valid frame when the
  // current bucket has no samples yet).
  bool rssiValid = false;
  int16_t rssiDbm = 0;
  bool rssiFromBucket = false;
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
  void setLastSentEpoch(int64_t epoch) { lastSentEpoch_ = epoch; }
  void setRawFrames(uint32_t count) { rawFrames_ = count; }
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
  int64_t lastSentEpoch_ = 0;
  uint32_t rawFrames_ = 0;
  bool radioReady_ = false;
  bool archiveReady_ = false;
  uint32_t queuedUploads_ = 0;
  uint32_t recordsToday_ = 0;
  bool restartScheduled_ = false;
  uint32_t restartAtMs_ = 0;
};

}  // namespace akashavani
