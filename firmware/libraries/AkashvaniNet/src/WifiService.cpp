#include "WifiService.h"

#include <cstdio>
#include <cstring>

#include "Log.h"

namespace akashvani {
namespace {
constexpr uint32_t kStaRetryIntervalMs = 30000;
constexpr uint32_t kApChannel = 6;
}

Status WifiService::begin(const DeviceSettings& settings, uint32_t staTimeoutMs) {
  settings_ = settings;
  if (settings_.wifiConfigured()) {
    WiFi.mode(WIFI_AP_STA);
  } else {
    WiFi.mode(WIFI_AP);
  }
  WiFi.setHostname(settings_.deviceId);
  WiFi.setSleep(false);

  startAp();
  if (!apStarted_) {
    return Status::IoError;
  }

  if (!settings_.wifiConfigured()) {
    AK_LOG_WARN("wifi", "no site Wi-Fi configured; AP-only mode");
    updateStatus();
    return Status::Ok;
  }

  startSta();
  const uint32_t started = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - started < staTimeoutMs) {
    delay(250);
  }
  updateStatus();
  if (!status_.staConnected) {
    AK_LOG_WARN("wifi", "site Wi-Fi not connected yet; will retry in background");
  }
  return Status::Ok;
}

void WifiService::startAp() {
  const char* password = settings_.apPassword[0] != '\0' ? settings_.apPassword
                                                         : "akashvani-setup";
  apStarted_ = WiFi.softAP(settings_.apSsid, password, kApChannel, false, 4);
  if (!apStarted_) {
    AK_LOG_ERROR("wifi", "softAP failed for SSID %s", settings_.apSsid);
    return;
  }
  AK_LOG_INFO("wifi", "AP up: %s (%s)", settings_.apSsid,
              WiFi.softAPIP().toString().c_str());
}

void WifiService::startSta() {
  if (!settings_.wifiConfigured()) {
    return;
  }
  WiFi.begin(settings_.wifiSsid, settings_.wifiPassword);
  staStarted_ = true;
  lastStaAttemptMs_ = millis();
  AK_LOG_INFO("wifi", "connecting to %s", settings_.wifiSsid);
}

void WifiService::loop() {
  if (!settings_.wifiConfigured()) {
    return;
  }
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }
  if (millis() - lastStaAttemptMs_ >= kStaRetryIntervalMs) {
    ++status_.reconnectCount;
    AK_LOG_WARN("wifi", "retrying site Wi-Fi (%lu)", static_cast<unsigned long>(
                                                       status_.reconnectCount));
    WiFi.disconnect();
    startSta();
  }
  updateStatus();
}

void WifiService::updateStatus() {
  status_.staConnected = WiFi.status() == WL_CONNECTED;
  status_.apActive = apStarted_;
  status_.rssiDbm = status_.staConnected ? static_cast<int16_t>(WiFi.RSSI()) : 0;
  status_.stationCount = apStarted_ ? WiFi.softAPgetStationNum() : 0;
  std::snprintf(status_.staIp, sizeof(status_.staIp), "%s",
                WiFi.localIP().toString().c_str());
  std::snprintf(status_.apIp, sizeof(status_.apIp), "%s",
                WiFi.softAPIP().toString().c_str());
}

bool WifiService::staConnected() const { return WiFi.status() == WL_CONNECTED; }

bool WifiService::apActive() const { return apStarted_; }

const WifiStatus& WifiService::status() {
  updateStatus();
  return status_;
}

}  // namespace akashvani
