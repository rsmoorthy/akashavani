#pragma once

#include <cstdint>
#include <cstring>

namespace akashvani {

constexpr uint8_t kMaxWifiProfiles = 6;

// One site Wi-Fi credential pair. Fixed-size so the settings struct stays
// copyable and allocation-free at runtime.
struct WifiProfile {
  char ssid[33] = {};
  char password[65] = {};

  bool configured() const { return ssid[0] != '\0'; }
};

struct DeviceSettings {
  char deviceId[33] = {};
  char deviceToken[65] = {};
  // Shared bearer token for the archive API. It is also accepted by the
  // server's admin API. The hotspot page edits this value.
  char apiToken[65] = {};
  char serverBaseUrl[129] = {};
  char deviceMac[18] = {};
  uint16_t stationId = 0;

  // Ordered list of site networks. The AP is always available; the STA side
  // tries these in order until one connects.
  WifiProfile wifiProfiles[kMaxWifiProfiles];
  uint8_t wifiProfileCount = 0;
  char adminPassword[33] = {};

  bool uploadEnabled = true;
  bool archiveEnabled = true;
  uint16_t uploadIntervalSeconds = 60;
  uint16_t commandPollIntervalSeconds = 60;

  bool wifiConfigured() const {
    return wifiProfileCount > 0 && wifiProfiles[0].configured();
  }
  bool cloudConfigured() const {
    return deviceToken[0] != '\0' && serverBaseUrl[0] != '\0';
  }
  bool provisioned() const { return wifiConfigured() && cloudConfigured(); }

  void clearWifiProfiles() {
    for (uint8_t i = 0; i < kMaxWifiProfiles; ++i) {
      wifiProfiles[i] = WifiProfile();
    }
    wifiProfileCount = 0;
  }

  // Appends a profile when the SSID is non-empty. Returns false when the list
  // is full or the SSID is empty.
  bool addWifiProfile(const char* ssid, const char* password) {
    if (ssid == nullptr || ssid[0] == '\0' || wifiProfileCount >= kMaxWifiProfiles) {
      return false;
    }
    WifiProfile& profile = wifiProfiles[wifiProfileCount++];
    std::strncpy(profile.ssid, ssid, sizeof(profile.ssid) - 1);
    profile.ssid[sizeof(profile.ssid) - 1] = '\0';
    if (password != nullptr) {
      std::strncpy(profile.password, password, sizeof(profile.password) - 1);
      profile.password[sizeof(profile.password) - 1] = '\0';
    }
    return true;
  }
};

}  // namespace akashvani
