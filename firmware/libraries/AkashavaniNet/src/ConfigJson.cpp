#include "ConfigJson.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace akashavani {
namespace {

void setError(char* buffer, size_t size, const char* message) {
  if (buffer != nullptr && size > 0) {
    std::snprintf(buffer, size, "%s", message == nullptr ? "" : message);
  }
}

bool isString(const JsonVariantConst& value) { return value.is<const char*>(); }

bool sameProfiles(const DeviceSettings& a, const DeviceSettings& b) {
  if (a.wifiProfileCount != b.wifiProfileCount) {
    return false;
  }
  for (uint8_t i = 0; i < a.wifiProfileCount; ++i) {
    if (std::strcmp(a.wifiProfiles[i].ssid, b.wifiProfiles[i].ssid) != 0 ||
        std::strcmp(a.wifiProfiles[i].password, b.wifiProfiles[i].password) != 0) {
      return false;
    }
  }
  return true;
}

}  // namespace

bool isValidTimezoneName(const char* value, size_t capacity) {
  if (value == nullptr || value[0] == '\0' || std::strlen(value) >= capacity) {
    return false;
  }
  for (const char* p = value; *p != '\0'; ++p) {
    const char c = *p;
    const bool allowed = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                         (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '+' ||
                         c == '/';
    if (!allowed) {
      return false;
    }
  }
  return true;
}

bool serializeDeviceConfigJson(const DeviceSettings& settings, String& output) {
  JsonDocument doc;
  doc["version"] = 1;
  doc["timezone"] = settings.timezone;
  if (settings.siteConfigured) {
    JsonObject site = doc["site"].to<JsonObject>();
    site["latitude"] = settings.latitude;
    site["longitude"] = settings.longitude;
    site["altitude_m"] = settings.altitudeM;
  } else {
    doc["site"] = nullptr;
  }
  JsonArray wifi = doc["wifi"].to<JsonArray>();
  for (uint8_t i = 0; i < settings.wifiProfileCount; ++i) {
    if (!settings.wifiProfiles[i].configured()) {
      continue;
    }
    JsonObject profile = wifi.add<JsonObject>();
    profile["ssid"] = settings.wifiProfiles[i].ssid;
    profile["password"] = settings.wifiProfiles[i].password;
  }
  doc["admin_password"] = settings.adminPassword;
  doc["upload_enabled"] = settings.uploadEnabled;
  doc["archive_enabled"] = settings.archiveEnabled;
  doc["upload_interval_seconds"] = settings.uploadIntervalSeconds;
  doc["command_poll_interval_seconds"] = settings.commandPollIntervalSeconds;

  output = "";
  serializeJson(doc, output);
  return output.length() > 0;
}

bool applyDeviceConfigJson(JsonVariantConst document, DeviceSettings& settings,
                           bool& networkChanged, char* error, size_t errorSize) {
  networkChanged = false;
  if (!document.is<JsonObjectConst>()) {
    setError(error, errorSize, "config must be an object");
    return false;
  }

  const int version = document["version"] | 0;
  if (version != 1) {
    setError(error, errorSize, "unsupported config version");
    return false;
  }

  const char* timezone = document["timezone"] | "";
  if (!isValidTimezoneName(timezone, sizeof(settings.timezone))) {
    setError(error, errorSize, "invalid timezone");
    return false;
  }

  DeviceSettings updated = settings;
  std::strncpy(updated.timezone, timezone, sizeof(updated.timezone) - 1);
  updated.timezone[sizeof(updated.timezone) - 1] = '\0';

  JsonVariantConst site = document["site"];
  if (!site.isNull()) {
    if (!site.is<JsonObjectConst>() || !site["latitude"].is<float>() ||
        !site["longitude"].is<float>() || !site["altitude_m"].is<float>()) {
      setError(error, errorSize, "site must carry latitude, longitude and altitude_m");
      return false;
    }
    const float latitude = site["latitude"] | 0.0f;
    const float longitude = site["longitude"] | 0.0f;
    const float altitude = site["altitude_m"] | 0.0f;
    if (!std::isfinite(latitude) || latitude < -90.0f || latitude > 90.0f) {
      setError(error, errorSize, "latitude out of range");
      return false;
    }
    if (!std::isfinite(longitude) || longitude < -180.0f || longitude > 180.0f) {
      setError(error, errorSize, "longitude out of range");
      return false;
    }
    if (!std::isfinite(altitude) || altitude < -500.0f || altitude > 9000.0f) {
      setError(error, errorSize, "altitude out of range");
      return false;
    }
    updated.siteConfigured = true;
    updated.latitude = latitude;
    updated.longitude = longitude;
    updated.altitudeM = altitude;
  } else if (document.containsKey("site")) {
    updated.siteConfigured = false;
    updated.latitude = 0.0f;
    updated.longitude = 0.0f;
    updated.altitudeM = 0.0f;
  }

  if (!document["wifi"].isNull()) {
    JsonArrayConst wifi = document["wifi"].as<JsonArrayConst>();
    if (wifi.isNull()) {
      setError(error, errorSize, "wifi must be an array");
      return false;
    }
    if (wifi.size() > kMaxWifiProfiles) {
      setError(error, errorSize, "at most 6 wifi profiles");
      return false;
    }
    updated.clearWifiProfiles();
    for (JsonVariantConst entry : wifi) {
      const char* ssid = entry["ssid"] | "";
      const char* password = entry["password"] | "";
      if (!isString(entry["ssid"]) || std::strlen(ssid) == 0 ||
          std::strlen(ssid) > 32) {
        setError(error, errorSize, "invalid wifi ssid");
        return false;
      }
      if (std::strlen(password) > 63) {
        setError(error, errorSize, "invalid wifi password");
        return false;
      }
      if (!updated.addWifiProfile(ssid, password)) {
        setError(error, errorSize, "could not store wifi profile");
        return false;
      }
    }
  }

  const char* adminPassword = document["admin_password"] | nullptr;
  if (adminPassword != nullptr) {
    if (std::strlen(adminPassword) >= sizeof(updated.adminPassword)) {
      setError(error, errorSize, "admin_password too long");
      return false;
    }
    std::strncpy(updated.adminPassword, adminPassword, sizeof(updated.adminPassword) - 1);
    updated.adminPassword[sizeof(updated.adminPassword) - 1] = '\0';
  }

  if (!document["upload_enabled"].isNull()) {
    updated.uploadEnabled = document["upload_enabled"] | updated.uploadEnabled;
  }
  if (!document["archive_enabled"].isNull()) {
    updated.archiveEnabled = document["archive_enabled"] | updated.archiveEnabled;
  }
  if (!document["upload_interval_seconds"].isNull()) {
    const int value = document["upload_interval_seconds"] | 0;
    if (value < 10 || value > 86400) {
      setError(error, errorSize, "upload_interval_seconds out of range");
      return false;
    }
    updated.uploadIntervalSeconds = static_cast<uint32_t>(value);
  }
  if (!document["command_poll_interval_seconds"].isNull()) {
    const int value = document["command_poll_interval_seconds"] | 0;
    if (value < 10 || value > 3600) {
      setError(error, errorSize, "command_poll_interval_seconds out of range");
      return false;
    }
    updated.commandPollIntervalSeconds = static_cast<uint16_t>(value);
  }

  networkChanged = !sameProfiles(settings, updated);
  updated.configVersion = settings.configVersion + 1;
  settings = updated;
  return true;
}

}  // namespace akashavani
