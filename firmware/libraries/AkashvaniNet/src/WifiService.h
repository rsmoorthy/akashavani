#pragma once

#include <Arduino.h>
#include <WiFi.h>

#include "DeviceSettings.h"
#include "Status.h"

namespace akashvani {

struct WifiStatus {
  bool staConnected = false;
  bool apActive = false;
  char staIp[16] = "0.0.0.0";
  char apIp[16] = "0.0.0.0";
  int16_t rssiDbm = 0;
  uint32_t reconnectCount = 0;
  uint8_t stationCount = 0;
};

// Brings up the provisioning AP and, when configured, the site STA link.
// Both interfaces stay active: the AP is the local admin path even when STA is
// connected.
class WifiService {
 public:
  Status begin(const DeviceSettings& settings, uint32_t staTimeoutMs = 15000);
  void loop();

  bool staConnected() const;
  bool apActive() const;
  const WifiStatus& status();

 private:
  void updateStatus();
  void startSta();
  void startAp();

  DeviceSettings settings_;
  WifiStatus status_;
  uint32_t lastStaAttemptMs_ = 0;
  bool apStarted_ = false;
  bool staStarted_ = false;
};

}  // namespace akashvani
