#include "SettingsStore.h"

#include <WiFi.h>

#include <cstdio>
#include <cstring>

#include "DeviceId.h"
#include "Log.h"

namespace akashvani {
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
  if (settings.deviceId[0] == '\0') {
    uint8_t mac[6] = {};
    WiFi.macAddress(mac);
    if (!makeDeviceIdFromMac(mac, settings.deviceId, sizeof(settings.deviceId))) {
      std::snprintf(settings.deviceId, sizeof(settings.deviceId), "ak-unknown");
    }
  }
  if (settings.apSsid[0] == '\0') {
    std::snprintf(settings.apSsid, sizeof(settings.apSsid), "Akashvani-%s",
                  settings.deviceId);
  }
  if (settings.apPassword[0] == '\0') {
    std::snprintf(settings.apPassword, sizeof(settings.apPassword), "akashvani-setup");
  }
  if (settings.adminPassword[0] == '\0') {
    std::snprintf(settings.adminPassword, sizeof(settings.adminPassword), "akashvani-admin");
  }
  if (settings.serverBaseUrl[0] == '\0') {
    std::snprintf(settings.serverBaseUrl, sizeof(settings.serverBaseUrl),
                  "http://192.168.1.100:8000");
  }
  if (settings.uploadIntervalSeconds < 10) {
    settings.uploadIntervalSeconds = 60;
  }
  if (settings.commandPollIntervalSeconds < 10) {
    settings.commandPollIntervalSeconds = 60;
  }
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
  copyString(loaded.serverBaseUrl, sizeof(loaded.serverBaseUrl),
             preferences_.getString("server", ""), "");
  copyString(loaded.wifiSsid, sizeof(loaded.wifiSsid),
             preferences_.getString("wifi_ssid", ""), "");
  copyString(loaded.wifiPassword, sizeof(loaded.wifiPassword),
             preferences_.getString("wifi_pass", ""), "");
  copyString(loaded.apPassword, sizeof(loaded.apPassword),
             preferences_.getString("ap_pass", ""), "");
  copyString(loaded.adminPassword, sizeof(loaded.adminPassword),
             preferences_.getString("admin_pw", ""), "");
  loaded.uploadEnabled = preferences_.getBool("upload", true);
  loaded.archiveEnabled = preferences_.getBool("archive", true);
  loaded.uploadIntervalSeconds = preferences_.getUShort("upl_int", 60);
  loaded.commandPollIntervalSeconds = preferences_.getUShort("cmd_int", 60);

  applyDefaults(loaded);
  settings = loaded;
  AK_LOG_INFO("settings", "device=%s provisioned=%s", settings.deviceId,
              settings.provisioned() ? "yes" : "no");
  return Status::Ok;
}

Status SettingsStore::save(const DeviceSettings& settings) {
  if (!open_) {
    return Status::NotReady;
  }
  if (!isValidDeviceId(settings.deviceId) || settings.apPassword[0] == '\0' ||
      settings.adminPassword[0] == '\0') {
    return Status::InvalidArgument;
  }
  preferences_.putString("dev_id", settings.deviceId);
  preferences_.putString("token", settings.deviceToken);
  preferences_.putString("server", settings.serverBaseUrl);
  preferences_.putString("wifi_ssid", settings.wifiSsid);
  preferences_.putString("wifi_pass", settings.wifiPassword);
  preferences_.putString("ap_pass", settings.apPassword);
  preferences_.putString("admin_pw", settings.adminPassword);
  preferences_.putBool("upload", settings.uploadEnabled);
  preferences_.putBool("archive", settings.archiveEnabled);
  preferences_.putUShort("upl_int", settings.uploadIntervalSeconds);
  preferences_.putUShort("cmd_int", settings.commandPollIntervalSeconds);
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

}  // namespace akashvani
