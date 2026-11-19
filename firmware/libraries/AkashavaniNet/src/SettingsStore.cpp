#include "SettingsStore.h"

#include <WiFi.h>
#include <esp_mac.h>

#include <cstdio>
#include <cstring>

#include "DeviceId.h"
#include "Log.h"

namespace akashavani {
namespace {

void setError(char* buffer, size_t size, const char* message) {
  if (buffer != nullptr && size > 0) {
    std::snprintf(buffer, size, "%s", message == nullptr ? "" : message);
  }
}

}  // namespace

SettingsStore::SettingsStore(const char* name) : name_(name) {}

Status SettingsStore::begin() {
  if (open_) {
    return Status::Ok;
  }
  if (!preferences_.begin(name_, false)) {
    setError(lastError_, sizeof(lastError_), "nvs open failed");
    return Status::IoError;
  }
  open_ = true;
  return Status::Ok;
}

void SettingsStore::copyString(char* destination, size_t size, const String& value,
                               const char* fallback) {
  const char* source = value.length() > 0 ? value.c_str() : fallback;
  if (source == nullptr) {
    source = "";
  }
  std::strncpy(destination, source, size - 1);
  destination[size - 1] = '\0';
}

void SettingsStore::applyDefaults(DeviceSettings& settings) {
  uint8_t mac[6] = {};
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  std::snprintf(settings.deviceMac, sizeof(settings.deviceMac),
                "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3],
                mac[4], mac[5]);
  if (settings.deviceId[0] == '\0') {
    if (!makeDeviceIdFromMac(mac, settings.deviceId, sizeof(settings.deviceId))) {
      std::snprintf(settings.deviceId, sizeof(settings.deviceId), "ak-unknown");
    }
  }
  if (settings.adminPassword[0] == '\0') {
    std::snprintf(settings.adminPassword, sizeof(settings.adminPassword), "akashavani-admin");
  }
  if (settings.serverBaseUrl[0] == '\0') {
    std::snprintf(settings.serverBaseUrl, sizeof(settings.serverBaseUrl),
                  "http://192.168.1.100:8000");
  }
  if (settings.apiToken[0] == '\0') {
    std::snprintf(settings.apiToken, sizeof(settings.apiToken), "secret");
  }
  if (settings.timezone[0] == '\0') {
    std::snprintf(settings.timezone, sizeof(settings.timezone), "UTC");
  }
  if (settings.configVersion == 0) {
    settings.configVersion = 1;
  }
  if (settings.uploadIntervalSeconds < 10) {
    settings.uploadIntervalSeconds = 60;
  }
  if (settings.commandPollIntervalSeconds < 10) {
    settings.commandPollIntervalSeconds = 60;
  }
}

bool SettingsStore::configured() {
  if (!open_) {
    return false;
  }
  return preferences_.isKey("configured") || preferences_.isKey("wifi_ssid") ||
         preferences_.isKey("wifi_ssid_0") || preferences_.isKey("server");
}

Status SettingsStore::load(DeviceSettings& settings) {
  if (!open_) {
    return Status::NotReady;
  }
  DeviceSettings loaded;
  copyString(loaded.deviceId, sizeof(loaded.deviceId),
             preferences_.getString("dev_id", ""), "");
  copyString(loaded.deviceToken, sizeof(loaded.deviceToken),
             preferences_.getString("token", ""), "");
  copyString(loaded.apiToken, sizeof(loaded.apiToken),
             preferences_.getString("api_token", ""), "");
  copyString(loaded.serverBaseUrl, sizeof(loaded.serverBaseUrl),
             preferences_.getString("server", ""), "");
  copyString(loaded.timezone, sizeof(loaded.timezone), preferences_.getString("tz", ""),
             "UTC");
  loaded.siteConfigured = preferences_.getBool("site_ok", false);
  loaded.latitude = preferences_.getFloat("lat", 0.0f);
  loaded.longitude = preferences_.getFloat("lon", 0.0f);
  loaded.altitudeM = preferences_.getFloat("alt", 0.0f);
  loaded.configVersion = preferences_.getUInt("cfg_ver", 1);
  copyString(loaded.adminPassword, sizeof(loaded.adminPassword),
             preferences_.getString("admin_pw", ""), "");

  char key[16];
  for (uint8_t i = 0; i < kMaxWifiProfiles; ++i) {
    std::snprintf(key, sizeof(key), "wifi_ssid_%u", static_cast<unsigned>(i));
    const String ssid = preferences_.getString(key, "");
    if (ssid.length() == 0) {
      continue;
    }
    std::snprintf(key, sizeof(key), "wifi_pass_%u", static_cast<unsigned>(i));
    const String password = preferences_.getString(key, "");
    loaded.addWifiProfile(ssid.c_str(), password.c_str());
  }
  // Migrate the single-profile keys written by earlier firmware.
  if (loaded.wifiProfileCount == 0) {
    const String legacySsid = preferences_.getString("wifi_ssid", "");
    if (legacySsid.length() > 0) {
      const String legacyPassword = preferences_.getString("wifi_pass", "");
      loaded.addWifiProfile(legacySsid.c_str(), legacyPassword.c_str());
    }
  }

  loaded.uploadEnabled = preferences_.getBool("upload", true);
  loaded.archiveEnabled = preferences_.getBool("archive", true);
  loaded.uploadIntervalSeconds = preferences_.getUInt("upl_int", 60);
  loaded.commandPollIntervalSeconds = preferences_.getUShort("cmd_int", 30);
  // Migration: earlier firmware used a 60 s poll default with no UI to change
  // it. Move those devices to the 30 s cadence once.
  if (!preferences_.isKey("cmd_int_v2")) {
    loaded.commandPollIntervalSeconds = 30;
    preferences_.putUShort("cmd_int", 30);
    preferences_.putBool("cmd_int_v2", true);
  }

  applyDefaults(loaded);
  settings = loaded;
  AK_LOG_INFO("settings", "device=%s wifi_profiles=%u provisioned=%s",
              settings.deviceId, static_cast<unsigned>(settings.wifiProfileCount),
              settings.provisioned() ? "yes" : "no");
  return Status::Ok;
}

Status SettingsStore::save(const DeviceSettings& settings) {
  if (!open_) {
    return Status::NotReady;
  }
  if (!isValidDeviceId(settings.deviceId)) {
    return Status::InvalidArgument;
  }
  preferences_.putString("dev_id", settings.deviceId);
  preferences_.putString("token", settings.deviceToken);
  preferences_.putString("api_token", settings.apiToken);
  preferences_.putString("server", settings.serverBaseUrl);
  preferences_.putString("tz", settings.timezone);
  preferences_.putBool("site_ok", settings.siteConfigured);
  preferences_.putFloat("lat", settings.latitude);
  preferences_.putFloat("lon", settings.longitude);
  preferences_.putFloat("alt", settings.altitudeM);
  preferences_.putUInt("cfg_ver", settings.configVersion);
  preferences_.putString("admin_pw", settings.adminPassword);
  char key[16];
  for (uint8_t i = 0; i < kMaxWifiProfiles; ++i) {
    const bool used = i < settings.wifiProfileCount && settings.wifiProfiles[i].configured();
    std::snprintf(key, sizeof(key), "wifi_ssid_%u", static_cast<unsigned>(i));
    preferences_.putString(key, used ? settings.wifiProfiles[i].ssid : "");
    std::snprintf(key, sizeof(key), "wifi_pass_%u", static_cast<unsigned>(i));
    preferences_.putString(key, used ? settings.wifiProfiles[i].password : "");
  }
  preferences_.remove("wifi_ssid");
  preferences_.remove("wifi_pass");
  preferences_.putBool("upload", settings.uploadEnabled);
  preferences_.putBool("archive", settings.archiveEnabled);
  preferences_.putUInt("upl_int", settings.uploadIntervalSeconds);
  preferences_.putUShort("cmd_int", settings.commandPollIntervalSeconds);
  preferences_.putBool("configured", true);
  AK_LOG_INFO("settings", "saved");
  return Status::Ok;
}

Status SettingsStore::clear() {
  if (!open_) {
    return Status::NotReady;
  }
  preferences_.clear();
  return Status::Ok;
}

}  // namespace akashavani
