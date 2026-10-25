#include "WebConfigServer.h"

#include <ArduinoJson.h>

#include <cstdio>
#include <cstring>

#include "Log.h"
#include "Version.h"

namespace akashvani {
namespace {

const char kIndexHtml[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Akashvani setup</title>
<style>
body{font-family:system-ui,sans-serif;max-width:44rem;margin:2rem auto;padding:0 1rem;background:#f5f6f8;color:#17202a}
h1{font-size:1.4rem} fieldset{border:1px solid #ccd2da;border-radius:8px;margin:1rem 0;padding:1rem;background:#fff}
label{display:block;margin:.6rem 0 .2rem;font-weight:600} input{width:100%;padding:.5rem;border:1px solid #b7c0cc;border-radius:5px}
button{margin:.8rem .4rem 0 0;padding:.55rem .9rem;border:0;border-radius:5px;background:#1f6feb;color:#fff;font-weight:600}
button.secondary{background:#57606a} pre{background:#fff;padding:.8rem;border-radius:8px;overflow:auto;font-size:.82rem}
.muted{color:#57606a;font-size:.9rem}
</style></head><body>
<h1>Akashvani local admin</h1>
<p class="muted">Device <b id="dev">-</b> &middot; firmware <span id="fw">-</span></p>
<fieldset><legend>Status</legend><pre id="status">loading...</pre>
<button onclick="refresh()">Refresh</button></fieldset>
<fieldset><legend>Admin</legend>
<label>Admin password</label><input id="admin" type="password" autocomplete="current-password">
<button onclick="loadSettings()">Unlock settings</button></fieldset>
<fieldset><legend>Site Wi-Fi &amp; cloud</legend>
<label>Wi-Fi SSID</label><input id="ssid">
<label>Wi-Fi password (leave blank to keep)</label><input id="wifipass" type="password">
<label>Cloud base URL</label><input id="server" placeholder="http://192.168.1.100:8000">
<label>Device token (leave blank to keep)</label><input id="token" type="password">
<label>Upload interval (seconds)</label><input id="interval" type="number" min="10" max="3600">
<label><input id="upload" type="checkbox" style="width:auto"> Upload to cloud</label>
<label><input id="archive" type="checkbox" style="width:auto"> Archive to SD card</label>
<button onclick="save()">Save</button>
<button class="secondary" onclick="reboot()">Reboot</button>
<pre id="message"></pre></fieldset>
<script>
const $ = id => document.getElementById(id);
const headers = () => ({'X-Admin-Token': $('admin').value, 'Content-Type': 'application/json'});
async function refresh(){
  try { const r = await fetch('/api/status'); const j = await r.json();
    $('dev').textContent = j.device_id; $('fw').textContent = j.firmware;
    $('status').textContent = JSON.stringify(j, null, 2);
  } catch(e) { $('status').textContent = String(e); }
}
async function loadSettings(){
  const r = await fetch('/api/settings', {headers: headers()});
  if(!r.ok){ $('message').textContent = 'Admin password rejected'; return; }
  const j = await r.json();
  $('ssid').value = j.wifi_ssid || ''; $('server').value = j.server_url || '';
  $('interval').value = j.upload_interval_s || 60;
  $('upload').checked = !!j.upload_enabled; $('archive').checked = !!j.archive_enabled;
  $('message').textContent = 'Settings unlocked';
}
async function save(){
  const body = {
    wifi_ssid: $('ssid').value, wifi_password: $('wifipass').value,
    server_url: $('server').value, device_token: $('token').value,
    upload_interval_s: Number($('interval').value),
    upload_enabled: $('upload').checked, archive_enabled: $('archive').checked
  };
  const r = await fetch('/api/settings', {method:'POST', headers: headers(), body: JSON.stringify(body)});
  $('message').textContent = r.ok ? 'Saved. Reboot to apply Wi-Fi changes.' : 'Save failed: ' + await r.text();
}
async function reboot(){
  if(!confirm('Reboot now?')) return;
  await fetch('/api/reboot', {method:'POST', headers: headers()});
  $('message').textContent = 'Rebooting...';
}
refresh(); setInterval(refresh, 5000);
</script></body></html>)HTML";

void copyIfPresent(const JsonVariantConst& source, char* destination, size_t size,
                   bool keepIfEmpty) {
  const char* value = source.is<const char*>() ? source.as<const char*>() : nullptr;
  if (value == nullptr || (keepIfEmpty && value[0] == '\0')) {
    return;
  }
  std::strncpy(destination, value, size - 1);
  destination[size - 1] = '\0';
}

}  // namespace

WebConfigServer::WebConfigServer(DeviceSettings& settings, SettingsStore& store,
                                 WifiService& wifi, TimeService& time)
    : settings_(settings), store_(store), wifi_(wifi), time_(time), server_(80) {}

WebConfigServer::~WebConfigServer() { server_.stop(); }

Status WebConfigServer::begin() {
  server_.on("/", HTTP_GET, [this]() { handleRoot(); });
  server_.on("/api/status", HTTP_GET, [this]() { handleStatus(); });
  server_.on("/api/settings", HTTP_GET, [this]() { handleGetSettings(); });
  server_.on("/api/settings", HTTP_POST, [this]() { handleSaveSettings(); });
  server_.on("/api/reboot", HTTP_POST, [this]() { handleReboot(); });
  server_.onNotFound([this]() { handleNotFound(); });
  server_.begin();
  AK_LOG_INFO("web", "local admin at http://%s/", WiFi.softAPIP().toString().c_str());
  return Status::Ok;
}

void WebConfigServer::loop() { server_.handleClient(); }

void WebConfigServer::setSubsystemStatus(bool radioReady, bool archiveReady) {
  radioReady_ = radioReady;
  archiveReady_ = archiveReady;
}

void WebConfigServer::setCounters(uint32_t queuedUploads, uint32_t recordsToday) {
  queuedUploads_ = queuedUploads;
  recordsToday_ = recordsToday;
}

bool WebConfigServer::authorized() {
  if (settings_.adminPassword[0] == '\0') {
    return false;
  }
  const String supplied = server_.header("X-Admin-Token");
  return supplied.length() > 0 && supplied == settings_.adminPassword;
}

bool WebConfigServer::rejectUnauthorized() {
  if (authorized()) {
    return false;
  }
  sendJson(401, "{\"error\":\"admin token required\"}");
  return true;
}

void WebConfigServer::sendJson(int code, const String& body) {
  server_.sendHeader("Cache-Control", "no-store");
  server_.send(code, "application/json", body);
}

void WebConfigServer::handleRoot() {
  server_.send_P(200, "text/html", kIndexHtml);
}

void WebConfigServer::handleNotFound() { sendJson(404, "{\"error\":\"not found\"}"); }

void WebConfigServer::handleStatus() {
  const WifiStatus& wifiStatus = wifi_.status();
  JsonDocument doc;
  doc["device_id"] = settings_.deviceId;
  doc["firmware"] = AKASHVANI_FW_VERSION;
  doc["uptime_s"] = millis() / 1000UL;
  doc["provisioned"] = settings_.provisioned();
  JsonObject wifi = doc["wifi"].to<JsonObject>();
  wifi["sta_connected"] = wifiStatus.staConnected;
  wifi["ap_active"] = wifiStatus.apActive;
  wifi["sta_ip"] = wifiStatus.staIp;
  wifi["ap_ip"] = wifiStatus.apIp;
  wifi["rssi_dbm"] = wifiStatus.rssiDbm;
  wifi["stations"] = wifiStatus.stationCount;
  JsonObject time = doc["time"].to<JsonObject>();
  time["valid"] = time_.isValid();
  time["source"] = time_.source();
  time["epoch"] = time_.nowEpoch();
  doc["radio_ready"] = radioReady_;
  doc["archive_ready"] = archiveReady_;
  doc["queued_uploads"] = queuedUploads_;
  doc["records_today"] = recordsToday_;

  String body;
  serializeJson(doc, body);
  sendJson(200, body);
}

void WebConfigServer::handleGetSettings() {
  if (rejectUnauthorized()) {
    return;
  }
  JsonDocument doc;
  doc["device_id"] = settings_.deviceId;
  doc["wifi_ssid"] = settings_.wifiSsid;
  doc["server_url"] = settings_.serverBaseUrl;
  doc["device_token_set"] = settings_.deviceToken[0] != '\0';
  doc["upload_interval_s"] = settings_.uploadIntervalSeconds;
  doc["upload_enabled"] = settings_.uploadEnabled;
  doc["archive_enabled"] = settings_.archiveEnabled;
  doc["ap_ssid"] = settings_.apSsid;
  String body;
  serializeJson(doc, body);
  sendJson(200, body);
}

void WebConfigServer::handleSaveSettings() {
  if (rejectUnauthorized()) {
    return;
  }
  if (!server_.hasArg("plain")) {
    sendJson(400, "{\"error\":\"json body required\"}");
    return;
  }
  JsonDocument doc;
  const DeserializationError error = deserializeJson(doc, server_.arg("plain"));
  if (error) {
    sendJson(400, "{\"error\":\"invalid json\"}");
    return;
  }

  copyIfPresent(doc["wifi_ssid"], settings_.wifiSsid, sizeof(settings_.wifiSsid), false);
  copyIfPresent(doc["wifi_password"], settings_.wifiPassword,
                sizeof(settings_.wifiPassword), true);
  copyIfPresent(doc["server_url"], settings_.serverBaseUrl,
                sizeof(settings_.serverBaseUrl), false);
  copyIfPresent(doc["device_token"], settings_.deviceToken, sizeof(settings_.deviceToken),
                true);
  copyIfPresent(doc["ap_password"], settings_.apPassword, sizeof(settings_.apPassword),
                true);
  copyIfPresent(doc["admin_password"], settings_.adminPassword,
                sizeof(settings_.adminPassword), true);

  if (doc["upload_interval_s"].is<int>()) {
    const uint16_t interval = doc["upload_interval_s"] | 60;
    settings_.uploadIntervalSeconds = interval < 10 ? 10 : interval;
  }
  if (doc["upload_enabled"].is<bool>()) {
    settings_.uploadEnabled = doc["upload_enabled"] | true;
  }
  if (doc["archive_enabled"].is<bool>()) {
    settings_.archiveEnabled = doc["archive_enabled"] | true;
  }

  if (settings_.serverBaseUrl[0] != '\0' &&
      std::strncmp(settings_.serverBaseUrl, "http://", 7) != 0 &&
      std::strncmp(settings_.serverBaseUrl, "https://", 8) != 0) {
    sendJson(400, "{\"error\":\"server_url must start with http:// or https://\"}");
    return;
  }
  if (settings_.apPassword[0] != '\0' && std::strlen(settings_.apPassword) < 8) {
    sendJson(400, "{\"error\":\"AP password must be at least 8 characters\"}");
    return;
  }

  const Status status = store_.save(settings_);
  if (!isOk(status)) {
    sendJson(500, "{\"error\":\"could not save settings\"}");
    return;
  }
  sendJson(200, "{\"saved\":true,\"reboot_required\":true}");
}

void WebConfigServer::handleReboot() {
  if (rejectUnauthorized()) {
    return;
  }
  sendJson(200, "{\"rebooting\":true}");
  delay(500);
  ESP.restart();
}

}  // namespace akashvani
