#pragma once

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <HTTPClient.h>
#include <Update.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

#include "ApiClient.h"
#include "DeviceSettings.h"
#include "Status.h"

namespace akashvani {

class OtaService {
 public:
  explicit OtaService(const DeviceSettings& settings);

  // LAN OTA: ArduinoOTA on the site network.
  Status begin();

  // Remote OTA: ask the server, then pull the image over HTTP(S).
  Status checkAndApply(const char* channel = "stable");
  Status applyImage(const char* url, const char* expectedSha256 = nullptr);

  void loop();
  bool inProgress() const { return inProgress_; }

 private:
  void logProgress(unsigned int progress, unsigned int total);

  const DeviceSettings& settings_;
  bool started_ = false;
  bool inProgress_ = false;
  unsigned lastProgressPercent_ = 0;
};

}  // namespace akashvani
