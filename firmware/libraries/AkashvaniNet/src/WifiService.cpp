#include "WifiService.h"

#include <dhcpserver/dhcpserver.h>
#include <esp_mac.h>
#include <esp_netif.h>

#include <cstdio>
#include <cstring>

#include "Log.h"

namespace akashvani {
namespace {
constexpr uint32_t kStaRetryIntervalMs = 20000;
}

Status WifiService::begin(const DeviceSettings& settings, const WifiApConfig& apConfig,
                          uint32_t staTimeoutMs) {
  settings_ = settings;
  apConfig_ = apConfig;
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
  uint8_t mac[6] = {};
  // Read the factory station MAC from efuse. WiFi.macAddress() can still be
  // all zeros at this point because the interface has not started yet.
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  std::snprintf(apSsid_, sizeof(apSsid_), "%s%02x", apConfig_.ssidPrefix, mac[5]);

  const bool configured = WiFi.softAPConfig(IPAddress(apConfig_.ip), IPAddress(apConfig_.gateway),
                                            IPAddress(apConfig_.subnet));
  if (!configured) {
    AK_LOG_WARN("wifi", "softAPConfig failed; using defaults");
  }
  apStarted_ = WiFi.softAP(apSsid_, apConfig_.password, apConfig_.channel, false,
                           apConfig_.maxClients);
  if (!apStarted_) {
    AK_LOG_ERROR("wifi", "softAP failed for SSID %s", apSsid_);
    return;
  }
  if (!applyDhcpRange()) {
    AK_LOG_WARN("wifi", "could not set DHCP range %s-%s; using core default",
                apConfig_.dhcpFirst, apConfig_.dhcpLast);
  }
  AK_LOG_INFO("wifi", "AP up: %s (%s) DHCP %s-%s", apSsid_,
              WiFi.softAPIP().toString().c_str(), apConfig_.dhcpFirst,
              apConfig_.dhcpLast);
}

// arduino-esp32 exposes only the lease start in softAPConfig(). Set the exact
// end address through the netif DHCP-server API so the pool matches the
// configured range.
bool WifiService::applyDhcpRange() {
  esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
  if (netif == nullptr) {
    return false;
  }
  const esp_err_t stopped = esp_netif_dhcps_stop(netif);
  if (stopped != ESP_OK && stopped != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) {
    return false;
  }
  dhcps_lease_t lease = {};
  lease.enable = true;
  lease.start_ip.addr = static_cast<uint32_t>(IPAddress(apConfig_.dhcpFirst));
  lease.end_ip.addr = static_cast<uint32_t>(IPAddress(apConfig_.dhcpLast));
  const esp_err_t applied = esp_netif_dhcps_option(
      netif, ESP_NETIF_OP_SET, ESP_NETIF_REQUESTED_IP_ADDRESS, &lease, sizeof(lease));
  const esp_err_t started = esp_netif_dhcps_start(netif);
  return applied == ESP_OK && started == ESP_OK;
}

void WifiService::startSta() {
  if (!settings_.wifiConfigured()) {
    return;
  }
  if (status_.profileIndex >= settings_.wifiProfileCount) {
    status_.profileIndex = 0;
  }
  const WifiProfile& profile = settings_.wifiProfiles[status_.profileIndex];
  WiFi.begin(profile.ssid, profile.password);
  staStarted_ = true;
  lastStaAttemptMs_ = millis();
  AK_LOG_INFO("wifi", "connecting to %s (profile %u/%u)", profile.ssid,
              static_cast<unsigned>(status_.profileIndex + 1),
              static_cast<unsigned>(settings_.wifiProfileCount));
}

void WifiService::tryNextProfile() {
  if (settings_.wifiProfileCount == 0) {
    return;
  }
  status_.profileIndex = static_cast<uint8_t>(
      (status_.profileIndex + 1) % settings_.wifiProfileCount);
  ++status_.reconnectCount;
  AK_LOG_WARN("wifi", "retrying site Wi-Fi (%lu), next profile %s",
              static_cast<unsigned long>(status_.reconnectCount),
              settings_.wifiProfiles[status_.profileIndex].ssid);
  WiFi.disconnect();
  startSta();
}

void WifiService::loop() {
  if (!settings_.wifiConfigured()) {
    return;
  }
  if (WiFi.status() == WL_CONNECTED) {
    updateStatus();
    return;
  }
  if (millis() - lastStaAttemptMs_ >= kStaRetryIntervalMs) {
    tryNextProfile();
  }
  updateStatus();
}

void WifiService::updateStatus() {
  status_.staConnected = WiFi.status() == WL_CONNECTED;
  status_.apActive = apStarted_;
  status_.rssiDbm = status_.staConnected ? static_cast<int16_t>(WiFi.RSSI()) : 0;
  status_.stationCount = apStarted_ ? WiFi.softAPgetStationNum() : 0;
  if (status_.staConnected) {
    const String ssid = WiFi.SSID();
    std::snprintf(status_.staSsid, sizeof(status_.staSsid), "%s", ssid.c_str());
  } else {
    status_.staSsid[0] = '\0';
  }
  std::snprintf(status_.apSsid, sizeof(status_.apSsid), "%s", apSsid_);
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
