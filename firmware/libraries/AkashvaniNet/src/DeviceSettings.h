#pragma once

#include <cstdint>
#include <cstring>

namespace akashvani {

struct DeviceSettings {
  char deviceId[33] = {};
  char deviceToken[65] = {};
  char serverBaseUrl[129] = {};

  char wifiSsid[33] = {};
  char wifiPassword[65] = {};

  char apSsid[33] = {};
  char apPassword[65] = {};
  char adminPassword[33] = {};

  bool uploadEnabled = true;
  bool archiveEnabled = true;
  uint16_t uploadIntervalSeconds = 60;
  uint16_t commandPollIntervalSeconds = 60;

  bool wifiConfigured() const { return wifiSsid[0] != '\0'; }
  bool cloudConfigured() const {
    return deviceToken[0] != '\0' && serverBaseUrl[0] != '\0';
  }
  bool provisioned() const { return wifiConfigured() && cloudConfigured(); }
};

}  // namespace akashvani
