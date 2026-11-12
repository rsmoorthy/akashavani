#include "WebConfigServer.h"

#include <ArduinoJson.h>

#include <cstdio>
#include <cstring>

#include "Log.h"
#include "TimeUtils.h"
#include "Version.h"

namespace akashvani {
namespace {

const char kIndexHtml[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Akashvani</title>
<style>
:root{--bg:#eef2f7;--card:#fff;--ink:#17202a;--muted:#5b6675;--line:#d3dae4;--accent:#1f6feb;--ok:#1a7f37;--bad:#b42318}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--ink);font-family:system-ui,-apple-system,Segoe UI,Roboto,sans-serif;line-height:1.45}
main{max-width:40rem;margin:0 auto;padding:1rem}
header{padding:.4rem 0 1rem}
h1{font-size:1.45rem;margin:0}
h2{font-size:1.05rem;margin:0 0 .75rem}
h3{font-size:.95rem;margin:1rem 0 .4rem}
.sub{color:var(--muted);font-size:.9rem}
.card{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:1rem;margin-bottom:1rem;box-shadow:0 1px 2px rgba(16,24,40,.04)}
.row{display:flex;justify-content:space-between;gap:1rem;padding:.28rem 0;border-bottom:1px dashed #e8edf3;font-size:.92rem}
.row:last-child{border-bottom:0}
.row span{color:var(--muted)}
.row b{text-align:right;font-weight:600;word-break:break-word}
.dot{display:inline-block;width:.55rem;height:.55rem;border-radius:50%;background:#98a2b3;margin-left:.35rem;vertical-align:middle}
.dot.ok{background:var(--ok)}.dot.bad{background:var(--bad)}
label{display:block;font-weight:600;font-size:.9rem;margin:.7rem 0 .25rem}
input{width:100%;padding:.62rem .7rem;border:1px solid var(--line);border-radius:8px;font:inherit;background:#fff}
input:focus{outline:2px solid #b7d1ff;border-color:var(--accent)}
.wifi-row{display:grid;grid-template-columns:1.6rem 1fr;gap:.45rem;align-items:center;margin:.55rem 0}
.wifi-row .num{color:var(--muted);font-size:.82rem;text-align:center}
.wifi-row .fields{display:grid;gap:.4rem}
button{margin-top:.9rem;padding:.68rem .95rem;border:0;border-radius:8px;background:var(--accent);color:#fff;font:inherit;font-weight:600}
button.secondary{background:#57606a;margin-left:.4rem}
button:disabled{opacity:.6}
.msg{min-height:1.2rem;font-size:.9rem;margin:.7rem 0 0;word-break:break-word}
.msg.err{color:var(--bad)}.msg.ok{color:var(--ok)}
.muted{color:var(--muted);font-size:.88rem}
</style></head><body><main>
<header><h1>Akashvani</h1>
<div class="sub">Device <b id="device">-</b> &middot; MAC <span id="mac">-</span> &middot; firmware <b id="fw">-</b></div></header>

<section class="card">
<h2>Status <span id="live" class="dot" title="status"></span></h2>
<div class="row"><span>Date &amp; time</span><b id="time">-</b></div>
<div class="row"><span>Station ID</span><b id="station">-</b></div>
<div class="row"><span>Uptime</span><b id="uptime">-</b></div>
<div class="row"><span>Site Wi-Fi</span><b id="wifi">-</b></div>
<div class="row"><span>Site SSID</span><b id="stassid">-</b></div>
<div class="row"><span>Site IP</span><b id="staip">-</b></div>
<div class="row"><span>AP SSID</span><b id="apssid">-</b></div>
<div class="row"><span>AP IP</span><b id="apip">-</b></div>
<div class="row"><span>AP clients</span><b id="apclients">-</b></div>
<div class="row"><span>Radio</span><b id="radio">-</b></div>
<div class="row"><span>SD card</span><b id="sd">-</b></div>
<div class="row"><span>Queued uploads</span><b id="queue">-</b></div>
<h3>Weather station</h3>
<div id="weather" class="muted">No weather frame decoded yet.</div>
</section>

<section class="card">
<h2>Configuration</h2>
<form id="cfg" novalidate>
<label for="stationInput">Station ID</label>
<input id="stationInput" type="number" inputmode="numeric" min="1" max="65535" required>
<label for="serverInput">Server URL</label>
<input id="serverInput" type="url" placeholder="http://192.168.1.100:8000" required>
<label for="apiTokenInput">Server API token</label>
<input id="apiTokenInput" type="password" maxlength="64" placeholder="secret" required>
<label for="deviceTokenInput">Device token</label>
<input id="deviceTokenInput" type="password" maxlength="64" placeholder="token assigned to this device" required>
<p class="muted">Bearer token used for archive and admin API calls. Default: secret.</p>
<h3>Site Wi-Fi</h3>
<p class="muted">Up to six networks, tried in order. Leave a row blank to skip it.</p>
<div id="wifiRows"></div>
<button type="button" id="togglePass" class="secondary">Show passwords</button>
<button type="submit" id="saveBtn">Save all</button>
<p id="msg" class="msg" role="status"></p>
</form>
</section>
</main>
<script>
const $ = id => document.getElementById(id);
const esc = value => String(value ?? '');
function show(text, kind){
  const el = $('msg');
  el.textContent = text;
  el.className = 'msg' + (kind ? ' ' + kind : '');
}
function formatUptime(seconds){
  const s = Number(seconds) || 0;
  const d = Math.floor(s / 86400), h = Math.floor((s % 86400) / 3600), m = Math.floor((s % 3600) / 60);
  return (d ? d + 'd ' : '') + h + 'h ' + m + 'm ' + (s % 60) + 's';
}
function formatTime(epoch){
  if(!epoch) return '-';
  try { return new Date(epoch * 1000).toLocaleString(); } catch(_) { return String(epoch); }
}
async function refresh(){
  try {
    const r = await fetch('/api/status', {cache:'no-store'});
    if(!r.ok) throw new Error('HTTP ' + r.status);
    const j = await r.json();
    $('device').textContent = j.device_id || '-';
    $('mac').textContent = j.mac || '-';
    $('fw').textContent = j.firmware || '-';
    $('station').textContent = j.station_id ?? '-';
    $('uptime').textContent = formatUptime(j.uptime_s);
    const t = j.time || {};
    $('time').textContent = t.valid ? formatTime(t.epoch) : 'invalid - waiting for SNTP';
    const w = j.wifi || {};
    $('wifi').textContent = w.sta_connected
      ? 'connected' + (w.rssi_dbm ? ' (' + w.rssi_dbm + ' dBm)' : '')
      : 'not connected';
    $('stassid').textContent = w.sta_connected ? (w.sta_ssid || '-') : '-';
    $('staip').textContent = w.sta_ip || '-';
    $('apssid').textContent = w.ap_ssid || '-';
    $('apip').textContent = w.ap_ip || '-';
    $('apclients').textContent = w.stations ?? '-';
    $('radio').textContent = j.radio_ready ? 'ready' : 'not ready';
    $('sd').textContent = j.archive_ready ? 'mounted' : 'not mounted';
    $('queue').textContent = j.queued_uploads ?? '-';
    $('live').className = 'dot ' + (w.sta_connected ? 'ok' : 'bad');
    const weather = j.weather || {};
    $('weather').textContent = weather.available
      ? 'T ' + Number(weather.temperature_c).toFixed(1) + ' C, RH ' + Number(weather.humidity_pct).toFixed(0) +
        ' %, P ' + Number(weather.pressure_hpa).toFixed(1) + ' hPa, in ' + Number(weather.in_temperature_c).toFixed(1) +
        ' C / ' + Number(weather.in_humidity_pct).toFixed(0) + ' %, wind ' + Number(weather.wind_speed_ms).toFixed(1) +
        ' m/s, rain ' + Number(weather.rain_day_mm).toFixed(1) + ' mm, frame ' + formatTime(weather.observed_at_epoch)
      : 'No weather frame decoded yet.';
  } catch(e) {
    $('live').className = 'dot bad';
    $('wifi').textContent = 'status unavailable';
  }
}
function buildRows(profiles){
  const box = $('wifiRows');
  box.innerHTML = '';
  for(let i = 0; i < 6; i++){
    const p = profiles[i] || {};
    const row = document.createElement('div');
    row.className = 'wifi-row';
    const num = document.createElement('div');
    num.className = 'num';
    num.textContent = String(i + 1);
    const fields = document.createElement('div');
    fields.className = 'fields';
    const ssid = document.createElement('input');
    ssid.className = 'ssid';
    ssid.type = 'text';
    ssid.maxLength = 32;
    ssid.placeholder = 'Wi-Fi network name';
    ssid.value = esc(p.ssid);
    const pass = document.createElement('input');
    pass.className = 'pass';
    pass.type = 'password';
    pass.maxLength = 63;
    pass.placeholder = 'Password (8-63 characters)';
    pass.value = esc(p.password);
    fields.appendChild(ssid);
    fields.appendChild(pass);
    row.appendChild(num);
    row.appendChild(fields);
    box.appendChild(row);
  }
}
async function loadSettings(){
  try {
    const r = await fetch('/api/settings', {cache:'no-store'});
    if(!r.ok) throw new Error('HTTP ' + r.status);
    const j = await r.json();
    $('stationInput').value = j.station_id ?? '';
    $('serverInput').value = j.server_url || '';
    $('apiTokenInput').value = j.api_token || '';
    $('deviceTokenInput').value = j.device_token || '';
    buildRows(j.wifi || []);
  } catch(e) {
    show('Could not load settings: ' + e.message, 'err');
  }
}
function collectProfiles(){
  const profiles = [];
  const seen = new Set();
  const rows = document.querySelectorAll('#wifiRows .wifi-row');
  for(let i = 0; i < rows.length; i++){
    const ssid = rows[i].querySelector('.ssid').value.trim();
    const password = rows[i].querySelector('.pass').value;
    if(!ssid && password){ return {error: 'Row ' + (i + 1) + ': enter an SSID or clear the password.'}; }
    if(!ssid) continue;
    if(ssid.length > 32){ return {error: 'Row ' + (i + 1) + ': SSID must be 32 characters or fewer.'}; }
    if(password.length < 8 || password.length > 63){ return {error: 'Row ' + (i + 1) + ': password must be 8-63 characters.'}; }
    if(seen.has(ssid.toLowerCase())){ return {error: 'Row ' + (i + 1) + ': duplicate SSID.'}; }
    seen.add(ssid.toLowerCase());
    profiles.push({ssid, password});
  }
  return {profiles};
}
$('togglePass').addEventListener('click', () => {
  const button = $('togglePass');
  const showPasswords = button.textContent === 'Show passwords';
  document.querySelectorAll('#wifiRows .pass').forEach(input => { input.type = showPasswords ? 'text' : 'password'; });
  button.textContent = showPasswords ? 'Hide passwords' : 'Show passwords';
});
$('cfg').addEventListener('submit', async ev => {
  ev.preventDefault();
  const station = Number($('stationInput').value);
  const server = $('serverInput').value.trim();
  const apiToken = $('apiTokenInput').value.trim();
  const deviceToken = $('deviceTokenInput').value.trim();
  if(!Number.isInteger(station) || station < 1 || station > 65535){
    show('Station ID must be a whole number from 1 to 65535.', 'err');
    return;
  }
  if(!/^https?:\/\/.+/i.test(server) || server.length > 128){
    show('Server URL must start with http:// or https:// and be at most 128 characters.', 'err');
    return;
  }
  if(apiToken.length < 3 || apiToken.length > 64){
    show('API token must be 3-64 characters.', 'err');
    return;
  }
  if (deviceToken.length < 3 || deviceToken.length > 64) {
    show('Device token must be 3-64 characters.', 'err');
    return;
  }
  const collected = collectProfiles();
  if(collected.error){ show(collected.error, 'err'); return; }
  if(collected.profiles.length === 0){ show('Enter at least one site Wi-Fi network.', 'err'); return; }
  $('saveBtn').disabled = true;
  show('Saving...');
  try {
    const r = await fetch('/api/settings', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify({station_id: station, server_url: server, api_token: apiToken, device_token: deviceToken, wifi: collected.profiles})
    });
    const text = await r.text();
    let result = {};
    try { result = JSON.parse(text); } catch(_) {}
    if(!r.ok){ show(result.error || ('Save failed: ' + text), 'err'); return; }
    if(result.reboot_required){
      show('Saved. The device is rebooting to apply the Wi-Fi changes; reconnect to the AP in a few seconds.', 'ok');
      setTimeout(refresh, 7000);
    } else {
      show('Saved.', 'ok');
      refresh();
    }
  } catch(e) {
    show('Save failed: ' + e.message, 'err');
  } finally {
    $('saveBtn').disabled = false;
  }
});
refresh();
loadSettings();
setInterval(refresh, 5000);
</script></body></html>)HTML";

void copyString(const char* value, char* destination, size_t size) {
  if (destination == nullptr || size == 0) {
    return;
  }
  if (value == nullptr) {
    destination[0] = '\0';
    return;
  }
  std::strncpy(destination, value, size - 1);
  destination[size - 1] = '\0';
}

const char* jsonString(const JsonVariantConst& value) {
  return value.is<const char*>() ? value.as<const char*>() : nullptr;
}

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

void WebConfigServer::loop() {
  server_.handleClient();
  if (restartScheduled_ && static_cast<int32_t>(millis() - restartAtMs_) >= 0) {
    restartScheduled_ = false;
    AK_LOG_INFO("web", "restarting to apply settings");
    delay(100);
    ESP.restart();
  }
}

void WebConfigServer::setSubsystemStatus(bool radioReady, bool archiveReady) {
  radioReady_ = radioReady;
  archiveReady_ = archiveReady;
}

void WebConfigServer::setCounters(uint32_t queuedUploads, uint32_t recordsToday) {
  queuedUploads_ = queuedUploads;
  recordsToday_ = recordsToday;
}

void WebConfigServer::setWeather(const WeatherSnapshot& weather) { weather_ = weather; }

void WebConfigServer::sendJson(int code, const String& body) {
  server_.sendHeader("Cache-Control", "no-store");
  server_.send(code, "application/json", body);
}

void WebConfigServer::handleRoot() {
  server_.sendHeader("Cache-Control", "no-store");
  server_.send_P(200, "text/html", kIndexHtml);
}

void WebConfigServer::handleNotFound() { sendJson(404, "{\"error\":\"not found\"}"); }

void WebConfigServer::handleStatus() {
  const WifiStatus& wifiStatus = wifi_.status();
  const bool timeValid = time_.isValid();
  const int64_t epoch = timeValid ? time_.nowEpoch() : 0;

  JsonDocument doc;
  doc["device_id"] = settings_.deviceId;
  doc["mac"] = settings_.deviceMac;
  doc["station_id"] = settings_.stationId;
  doc["firmware"] = AKASHVANI_FW_VERSION;
  doc["uptime_s"] = millis() / 1000UL;
  doc["provisioned"] = settings_.provisioned();
  doc["server_url"] = settings_.serverBaseUrl;

  JsonObject wifi = doc["wifi"].to<JsonObject>();
  wifi["sta_connected"] = wifiStatus.staConnected;
  wifi["sta_ssid"] = wifiStatus.staSsid;
  wifi["sta_ip"] = wifiStatus.staIp;
  wifi["ap_active"] = wifiStatus.apActive;
  wifi["ap_ssid"] = wifiStatus.apSsid;
  wifi["ap_ip"] = wifiStatus.apIp;
  wifi["rssi_dbm"] = wifiStatus.rssiDbm;
  wifi["stations"] = wifiStatus.stationCount;
  wifi["profile_index"] = wifiStatus.profileIndex;

  JsonObject time = doc["time"].to<JsonObject>();
  char utc[24] = {};
  if (timeValid && formatIso8601Utc(epoch, utc, sizeof(utc))) {
    time["utc"] = utc;
  } else {
    time["utc"] = "";
  }
  time["valid"] = timeValid;
  time["source"] = time_.source();
  time["epoch"] = epoch;

  JsonObject weather = doc["weather"].to<JsonObject>();
  weather["available"] = weather_.available;
  if (weather_.available) {
    weather["observed_at_epoch"] = weather_.observedAtEpoch;
    weather["temperature_c"] = weather_.temperatureC;
    weather["humidity_pct"] = weather_.humidityPct;
    weather["pressure_hpa"] = weather_.pressureHpa;
    weather["in_temperature_c"] = weather_.inTemperatureC;
    weather["in_humidity_pct"] = weather_.inHumidityPct;
    weather["wind_speed_ms"] = weather_.windSpeedMs;
    weather["wind_direction_deg"] = weather_.windDirectionDeg;
    weather["wind_gust_ms"] = weather_.windGustMs;
    weather["rain_day_mm"] = weather_.rainDayMm;
  }

  doc["radio_ready"] = radioReady_;
  doc["archive_ready"] = archiveReady_;
  doc["queued_uploads"] = queuedUploads_;
  doc["records_today"] = recordsToday_;

  String body;
  serializeJson(doc, body);
  sendJson(200, body);
}

void WebConfigServer::handleGetSettings() {
  JsonDocument doc;
  doc["device_id"] = settings_.deviceId;
  doc["mac"] = settings_.deviceMac;
  doc["station_id"] = settings_.stationId;
  doc["server_url"] = settings_.serverBaseUrl;
  doc["api_token"] = settings_.apiToken;
  doc["device_token"] = settings_.deviceToken;
  JsonArray wifi = doc["wifi"].to<JsonArray>();
  for (uint8_t i = 0; i < kMaxWifiProfiles; ++i) {
    JsonObject profile = wifi.add<JsonObject>();
    if (i < settings_.wifiProfileCount) {
      profile["ssid"] = settings_.wifiProfiles[i].ssid;
      profile["password"] = settings_.wifiProfiles[i].password;
    } else {
      profile["ssid"] = "";
      profile["password"] = "";
    }
  }
  String body;
  serializeJson(doc, body);
  sendJson(200, body);
}

void WebConfigServer::handleSaveSettings() {
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

  const int stationId = doc["station_id"] | 0;
  if (stationId < 1 || stationId > 65535) {
    sendJson(400, "{\"error\":\"station_id must be between 1 and 65535\"}");
    return;
  }
  const char* serverUrl = jsonString(doc["server_url"]);
  if (serverUrl == nullptr ||
      (std::strncmp(serverUrl, "http://", 7) != 0 &&
       std::strncmp(serverUrl, "https://", 8) != 0) ||
      std::strlen(serverUrl) >= sizeof(settings_.serverBaseUrl)) {
    sendJson(400, "{\"error\":\"server_url must start with http:// or https://\"}");
    return;
  }
  const char* apiToken = jsonString(doc["api_token"]);
  if (apiToken == nullptr || std::strlen(apiToken) < 3 ||
      std::strlen(apiToken) >= sizeof(settings_.apiToken)) {
    sendJson(400, "{\"error\":\"api_token must be 3-64 characters\"}");
    return;
  }
  const char* deviceToken = jsonString(doc["device_token"]);
  if (deviceToken == nullptr || std::strlen(deviceToken) < 3 ||
      std::strlen(deviceToken) >= sizeof(settings_.deviceToken)) {
    sendJson(400, "{\"error\":\"device_token must be 3-64 characters\"}");
    return;
  }
  JsonArrayConst wifi = doc["wifi"].as<JsonArrayConst>();
  if (wifi.isNull()) {
    sendJson(400, "{\"error\":\"wifi array required\"}");
    return;
  }
  if (wifi.size() > kMaxWifiProfiles) {
    sendJson(400, "{\"error\":\"at most 6 wifi profiles\"}");
    return;
  }

  DeviceSettings updated = settings_;
  updated.clearWifiProfiles();
  for (JsonVariantConst entry : wifi) {
    const char* ssid = jsonString(entry["ssid"]);
    if (ssid == nullptr || ssid[0] == '\0') {
      continue;
    }
    const char* password = jsonString(entry["password"]);
    if (password == nullptr) {
      password = "";
    }
    if (std::strlen(ssid) > 32) {
      sendJson(400, "{\"error\":\"SSID must be 32 characters or fewer\"}");
      return;
    }
    if (std::strlen(password) < 8 || std::strlen(password) > 63) {
      sendJson(400, "{\"error\":\"Wi-Fi password must be 8-63 characters\"}");
      return;
    }
    if (!updated.addWifiProfile(ssid, password)) {
      sendJson(400, "{\"error\":\"could not store wifi profile\"}");
      return;
    }
  }

  updated.stationId = static_cast<uint16_t>(stationId);
  copyString(serverUrl, updated.serverBaseUrl, sizeof(updated.serverBaseUrl));
  copyString(apiToken, updated.apiToken, sizeof(updated.apiToken));
  copyString(deviceToken, updated.deviceToken, sizeof(updated.deviceToken));
  // The device ID is the MAC-derived `ak-xxxxxx` and is not editable here, so
  // changing the station number cannot break the server-side identity.

  const bool networkChanged = !sameProfiles(updated, settings_);
  const Status status = store_.save(updated);
  if (!isOk(status)) {
    sendJson(500, "{\"error\":\"could not save settings\"}");
    return;
  }
  settings_ = updated;
  AK_LOG_INFO("web", "settings saved: station=%u profiles=%u network_changed=%s",
              static_cast<unsigned>(settings_.stationId),
              static_cast<unsigned>(settings_.wifiProfileCount),
              networkChanged ? "yes" : "no");

  if (networkChanged) {
    scheduleRestart(2000);
    sendJson(200, "{\"saved\":true,\"reboot_required\":true}");
    return;
  }
  sendJson(200, "{\"saved\":true,\"reboot_required\":false}");
}

void WebConfigServer::handleReboot() {
  sendJson(200, "{\"rebooting\":true}");
  scheduleRestart(500);
}

void WebConfigServer::scheduleRestart(uint32_t delayMs) {
  restartScheduled_ = true;
  restartAtMs_ = millis() + delayMs;
}

}  // namespace akashvani
