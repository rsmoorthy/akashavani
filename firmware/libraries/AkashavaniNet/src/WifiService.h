#pragma once

#include <Arduino.h>
#include <WiFi.h>

#include "DeviceSettings.h"
#include "Status.h"

namespace akashavani {

// Access-point settings. Everything here can be changed in the sketch
// configuration; the SSID suffix is always derived from the Wi-Fi MAC so two
// devices on the same bench never collide.
struct WifiApConfig {
  const char* ssidPrefix = "akashavani-";
  const char* password = "doordarshan";
  uint8_t channel = 6;
  uint8_t maxClients = 4;
  const char* ip = "10.90.0.1";
  const char* gateway = "10.90.0.1";
  const char* subnet = "255.255.255.0";
  const char* dhcpFirst = "10.90.0.11";
  const char* dhcpLast = "10.90.0.20";
};

struct WifiStatus {
  bool staConnected = false;
  bool apActive = false;
  char staSsid[33] = {};
  char apSsid[33] = {};
  char staIp[16] = "0.0.0.0";
  char apIp[16] = "0.0.0.0";
  int16_t rssiDbm = 0;
  uint32_t reconnectCount = 0;
  uint8_t stationCount = 0;
  uint8_t profileIndex = 0;
};

// Brings up the provisioning AP and, when configured, the site STA link.
// Both interfaces stay active: the AP is the local admin path even when STA is
// connected. The STA side tries the configured profiles in order.
class WifiService {
 public:
  Status begin(const DeviceSettings& settings, const WifiApConfig& apConfig,
               uint32_t staTimeoutMs = 15000);
  void loop();

  bool staConnected() const;
  bool apActive() const;
  const WifiStatus& status();

 private:
  void updateStatus();
  void startSta();
  void tryNextProfile();
  void startAp();
  bool applyDhcpRange();

  DeviceSettings settings_;
  WifiApConfig apConfig_;
  WifiStatus status_;
  char apSsid_[33] = {};
  uint32_t lastStaAttemptMs_ = 0;
  bool apStarted_ = false;
  bool staStarted_ = false;
};

}  // namespace akashavani
