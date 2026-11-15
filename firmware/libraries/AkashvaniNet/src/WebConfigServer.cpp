#include "WebConfigServer.h"

#include <ArduinoJson.h>

#include <cmath>
#include <cstdio>
#include <cstring>

#include "ConfigJson.h"
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
input,select{width:100%;padding:.62rem .7rem;border:1px solid var(--line);border-radius:8px;font:inherit;background:#fff}
input:focus,select:focus{outline:2px solid #b7d1ff;border-color:var(--accent)}
.wifi-row{display:grid;grid-template-columns:1.6rem 1fr;gap:.45rem;align-items:center;margin:.55rem 0}
.wifi-row .num{color:var(--muted);font-size:.82rem;text-align:center}
.wifi-row .fields{display:grid;gap:.4rem}
button{margin-top:.9rem;padding:.68rem .95rem;border:0;border-radius:8px;background:var(--accent);color:#fff;font:inherit;font-weight:600}
button.secondary{background:#57606a;margin-left:.4rem}
button:disabled{opacity:.6}
.msg{min-height:1.2rem;font-size:.9rem;margin:.7rem 0 0;word-break:break-word}
.msg.err{color:var(--bad)}.msg.ok{color:var(--ok)}
.muted{color:var(--muted);font-size:.88rem}
.age{color:var(--muted);font-weight:400}
.tabs{display:flex;gap:.25rem;background:#dfe6ef;border:1px solid var(--line);border-radius:12px;padding:.25rem;margin:0 0 1rem}
.tabs button{flex:1;margin:0;padding:.6rem .75rem;border:0;border-radius:9px;background:transparent;color:var(--muted);font:inherit;font-weight:600}
.tabs button.active{background:var(--card);color:var(--ink);box-shadow:0 1px 2px rgba(16,24,40,.08)}
.tabs button:focus-visible{outline:2px solid var(--accent);outline-offset:2px}
.tab-panel[hidden]{display:none}
</style></head><body><main>
<header><h1>Akashvani</h1>
<div class="sub">Device <b id="device">-</b> &middot; MAC <span id="mac">-</span> &middot; firmware <b id="fw">-</b></div></header>

<nav class="tabs" role="tablist" aria-label="Sections">
<button type="button" id="tab-status" class="active" role="tab" aria-selected="true" aria-controls="panel-status">Status</button>
<button type="button" id="tab-config" role="tab" aria-selected="false" aria-controls="panel-config">Configuration</button>
</nav>

<section id="panel-status" class="card tab-panel" role="tabpanel" aria-labelledby="tab-status">
<h2>Status <span id="live" class="dot" title="status"></span></h2>
<div class="row"><span>Date &amp; time</span><b id="time">-</b></div>
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
<div class="row"><span>Last sent</span><b id="lastsent">-</b></div>
<h3>Weather station</h3>
<div class="row"><span>Temp</span><b id="w-temp">-</b></div>
<div class="row"><span>Int Temp</span><b id="w-int-temp">-</b></div>
<div class="row"><span>Wind</span><b id="w-wind">-</b></div>
<div class="row"><span>Rain</span><b id="w-rain">-</b></div>
<div class="row"><span>Valid frames</span><b id="w-valid">-</b></div>
<div class="row"><span>Invalid frames</span><b id="w-invalid">-</b></div>
<div class="row"><span>Last frame</span><b id="w-last">No weather frame decoded yet.</b></div>
</section>

<section id="panel-config" class="card tab-panel" role="tabpanel" aria-labelledby="tab-config" hidden>
<h2>Configuration</h2>
<form id="cfg" novalidate>
<label for="serverInput">Server URL</label>
<input id="serverInput" type="url" placeholder="http://192.168.1.100:8000" required>
<label for="apiTokenInput">Server API token</label>
<input id="apiTokenInput" type="password" maxlength="64" placeholder="secret" required>
<label for="deviceTokenInput">Device token</label>
<input id="deviceTokenInput" type="password" maxlength="64" placeholder="token assigned to this device" required>
<p class="muted">Bearer token used for archive and admin API calls. Default: secret.</p>
<label for="tzInput">Time zone</label>
<select id="tzInput"></select>
<p class="muted">Display only. Archives, raw logs and all uploads stay in UTC.</p>
<h3>Site coordinates</h3>
<p class="muted">Leave all three blank until the site position is known. The server uses altitude for derived pressure values.</p>
<label for="latInput">Latitude</label>
<input id="latInput" type="number" step="any" min="-90" max="90" placeholder="13.4867">
<label for="lonInput">Longitude</label>
<input id="lonInput" type="number" step="any" min="-180" max="180" placeholder="77.7064">
<label for="altInput">Altitude (m)</label>
<input id="altInput" type="number" step="any" min="-500" max="9000" placeholder="940">
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
let deviceTimezone = 'UTC';
const timezoneNames = (() => {
  try {
    const all = Intl.supportedValuesOf('timeZone');
    if (all && all.length) return all;
  } catch (_) {}
  return ['UTC', 'Asia/Kolkata', 'Europe/London', 'America/New_York', 'Australia/Sydney'];
})();
function buildTimezones(selected){
  const zone = selected || 'UTC';
  const options = timezoneNames.includes(zone) ? timezoneNames.slice() : [zone, ...timezoneNames];
  const select = $('tzInput');
  select.innerHTML = '';
  for(const name of options){
    const option = document.createElement('option');
    option.value = name;
    option.textContent = name;
    select.appendChild(option);
  }
  select.value = zone;
}
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
function formatTime(epoch, zone){
  if(!epoch) return '-';
  try {
    return new Date(epoch * 1000).toLocaleString(undefined, {timeZone: zone || 'UTC', timeZoneName: 'short'});
  } catch(_) { return new Date(epoch * 1000).toUTCString(); }
}
function hasNumber(value){ return typeof value === 'number' && Number.isFinite(value); }
function ago(seconds){
  const s = Math.max(0, Math.round(Number(seconds) || 0));
  if(s < 60) return s + 's ago';
  const m = Math.floor(s / 60);
  if(m < 60) return m + 'm ago';
  const h = Math.floor(m / 60);
  if(h < 24) return h + 'h ago';
  return Math.floor(h / 24) + 'd ago';
}
function setReading(id, text, ageSeconds){
  const el = $(id);
  el.textContent = text;
  if(hasNumber(ageSeconds)){
    const age = document.createElement('span');
    age.className = 'age';
    age.textContent = ' · ' + ago(ageSeconds);
    el.appendChild(age);
  }
}
function lastFrameText(w){
  const parts = [];
  if(hasNumber(w.temperature_c)) parts.push('T ' + w.temperature_c.toFixed(1) + ' C');
  if(hasNumber(w.humidity_pct)) parts.push('RH ' + w.humidity_pct.toFixed(0) + ' %');
  if(hasNumber(w.pressure_hpa)) parts.push('P ' + w.pressure_hpa.toFixed(1) + ' hPa');
  if(hasNumber(w.in_temperature_c) || hasNumber(w.in_humidity_pct)){
    const inTemp = hasNumber(w.in_temperature_c) ? w.in_temperature_c.toFixed(1) + ' C' : '-';
    const inHumidity = hasNumber(w.in_humidity_pct) ? w.in_humidity_pct.toFixed(0) + ' %' : '-';
    parts.push('in ' + inTemp + ' / ' + inHumidity);
  }
  if(hasNumber(w.wind_speed_ms)){
    let wind = 'wind ' + w.wind_speed_ms.toFixed(1) + ' m/s';
    if(hasNumber(w.wind_direction_deg)) wind += ' at ' + w.wind_direction_deg.toFixed(0) + '°';
    parts.push(wind);
  }
  if(hasNumber(w.wind_gust_ms)) parts.push('gust ' + w.wind_gust_ms.toFixed(1) + ' m/s');
  if(hasNumber(w.rain_rate_mm_h)) parts.push('rain ' + w.rain_rate_mm_h.toFixed(1) + ' mm/h');
  return parts.join(', ');
}
async function refresh(){
  try {
    const r = await fetch('/api/status', {cache:'no-store'});
    if(!r.ok) throw new Error('HTTP ' + r.status);
    const j = await r.json();
    $('device').textContent = j.device_id || '-';
    $('mac').textContent = j.mac || '-';
    $('fw').textContent = j.firmware || '-';
    $('uptime').textContent = formatUptime(j.uptime_s);
    const t = j.time || {};
    deviceTimezone = t.timezone || 'UTC';
    $('time').textContent = t.valid
      ? formatTime(t.epoch, deviceTimezone) + ' (' + deviceTimezone + ')'
      : 'invalid - waiting for SNTP';
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
    $('lastsent').textContent = j.last_sent_epoch ? formatTime(j.last_sent_epoch, deviceTimezone) : '-';
    $('live').className = 'dot ' + (w.sta_connected ? 'ok' : 'bad');
    const weather = j.weather || {};
    const readings = weather.readings || {};
    const frames = weather.frames || {};
    const temperature = readings.temperature;
    setReading('w-temp',
      temperature && hasNumber(temperature.value_c) ? temperature.value_c.toFixed(1) + ' °C' : '-',
      temperature && hasNumber(temperature.age_s) ? temperature.age_s : null);
    const inTemperature = readings.in_temperature;
    setReading('w-int-temp',
      inTemperature && hasNumber(inTemperature.value_c) ? inTemperature.value_c.toFixed(1) + ' °C' : '-',
      inTemperature && hasNumber(inTemperature.age_s) ? inTemperature.age_s : null);
    const wind = readings.wind;
    let windText = '-';
    if(wind && hasNumber(wind.speed_ms)){
      windText = wind.speed_ms.toFixed(1) + ' m/s';
      if(hasNumber(wind.direction_deg)) windText += ' · ' + wind.direction_deg.toFixed(0) + '°';
    }
    setReading('w-wind', windText, wind && hasNumber(wind.age_s) ? wind.age_s : null);
    const rain = readings.rain;
    setReading('w-rain',
      rain && hasNumber(rain.rate_mm_h) ? rain.rate_mm_h.toFixed(1) + ' mm/h' : '-',
      rain && hasNumber(rain.age_s) ? rain.age_s : null);
    $('w-valid').textContent = hasNumber(frames.valid) && hasNumber(frames.expected)
      ? frames.valid + '/' + frames.expected : '-';
    $('w-invalid').textContent = hasNumber(frames.invalid) ? String(frames.invalid) : '-';
    const frameText = weather.available ? lastFrameText(weather) : '';
    setReading('w-last', frameText || 'No weather frame decoded yet.',
      frameText && hasNumber(weather.last_frame_age_s) ? weather.last_frame_age_s : null);
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
    $('serverInput').value = j.server_url || '';
    $('apiTokenInput').value = j.api_token || '';
    $('deviceTokenInput').value = j.device_token || '';
    buildTimezones(j.timezone || 'UTC');
    $('latInput').value = j.site_configured ? j.latitude : '';
    $('lonInput').value = j.site_configured ? j.longitude : '';
    $('altInput').value = j.site_configured ? j.altitude_m : '';
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
  const server = $('serverInput').value.trim();
  const apiToken = $('apiTokenInput').value.trim();
  const deviceToken = $('deviceTokenInput').value.trim();
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
  const latRaw = $('latInput').value.trim();
  const lonRaw = $('lonInput').value.trim();
  const altRaw = $('altInput').value.trim();
  const siteFields = [latRaw, lonRaw, altRaw];
  const filled = siteFields.filter(value => value !== '').length;
  let site = {site_configured: false, latitude: 0, longitude: 0, altitude_m: 0};
  if(filled > 0){
    if(filled !== 3){ show('Enter latitude, longitude and altitude together, or leave all three blank.', 'err'); return; }
    const latitude = Number(latRaw), longitude = Number(lonRaw), altitude = Number(altRaw);
    if(!Number.isFinite(latitude) || latitude < -90 || latitude > 90){ show('Latitude must be between -90 and 90.', 'err'); return; }
    if(!Number.isFinite(longitude) || longitude < -180 || longitude > 180){ show('Longitude must be between -180 and 180.', 'err'); return; }
    if(!Number.isFinite(altitude) || altitude < -500 || altitude > 9000){ show('Altitude must be between -500 and 9000 m.', 'err'); return; }
    site = {site_configured: true, latitude, longitude, altitude_m: altitude};
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
      body: JSON.stringify({server_url: server, api_token: apiToken, device_token: deviceToken, timezone: $('tzInput').value, wifi: collected.profiles, ...site})
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
function selectTab(name){
  const status = name !== 'config';
  $('tab-status').classList.toggle('active', status);
  $('tab-config').classList.toggle('active', !status);
  $('tab-status').setAttribute('aria-selected', status ? 'true' : 'false');
  $('tab-config').setAttribute('aria-selected', status ? 'false' : 'true');
  $('panel-status').hidden = !status;
  $('panel-config').hidden = status;
}
$('tab-status').addEventListener('click', () => selectTab('status'));
$('tab-config').addEventListener('click', () => selectTab('config'));
selectTab(location.hash === '#config' ? 'config' : 'status');
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
  doc["firmware"] = AKASHVANI_FW_VERSION;
  const uint32_t nowMs = millis();
  doc["uptime_s"] = nowMs / 1000UL;
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
  time["timezone"] = settings_.timezone;
  time["epoch"] = epoch;

  JsonObject weather = doc["weather"].to<JsonObject>();
  weather["available"] = weather_.available;
  JsonObject frames = weather["frames"].to<JsonObject>();
  frames["valid"] = weather_.framesValid;
  frames["expected"] = weather_.framesExpected;
  frames["invalid"] = weather_.framesInvalid;
  if (weather_.available) {
    weather["observed_at_epoch"] = weather_.observedAtEpoch;
    weather["last_frame_age_s"] = (nowMs - weather_.observedAtMs) / 1000UL;
    if (std::isfinite(weather_.temperatureC)) {
      weather["temperature_c"] = weather_.temperatureC;
    }
    if (std::isfinite(weather_.humidityPct)) {
      weather["humidity_pct"] = weather_.humidityPct;
    }
    if (std::isfinite(weather_.pressureHpa)) {
      weather["pressure_hpa"] = weather_.pressureHpa;
    }
    if (std::isfinite(weather_.inTemperatureC)) {
      weather["in_temperature_c"] = weather_.inTemperatureC;
    }
    if (std::isfinite(weather_.inHumidityPct)) {
      weather["in_humidity_pct"] = weather_.inHumidityPct;
    }
    if (std::isfinite(weather_.windSpeedMs)) {
      weather["wind_speed_ms"] = weather_.windSpeedMs;
    }
    if (std::isfinite(weather_.windDirectionDeg)) {
      weather["wind_direction_deg"] = weather_.windDirectionDeg;
    }
    if (std::isfinite(weather_.windGustMs)) {
      weather["wind_gust_ms"] = weather_.windGustMs;
    }
    if (std::isfinite(weather_.rainRateMmH)) {
      weather["rain_rate_mm_h"] = weather_.rainRateMmH;
    }
  }

  JsonObject readings = weather["readings"].to<JsonObject>();
  if (weather_.temperature.valid) {
    JsonObject reading = readings["temperature"].to<JsonObject>();
    reading["value_c"] = weather_.temperature.value;
    reading["age_s"] = (nowMs - weather_.temperature.updatedAtMs) / 1000UL;
  }
  if (weather_.inTemperature.valid) {
    JsonObject reading = readings["in_temperature"].to<JsonObject>();
    reading["value_c"] = weather_.inTemperature.value;
    reading["age_s"] = (nowMs - weather_.inTemperature.updatedAtMs) / 1000UL;
  }
  if (weather_.windSpeed.valid) {
    JsonObject reading = readings["wind"].to<JsonObject>();
    reading["speed_ms"] = weather_.windSpeed.value;
    if (std::isfinite(weather_.windDirectionDeg)) {
      reading["direction_deg"] = weather_.windDirectionDeg;
    }
    reading["age_s"] = (nowMs - weather_.windSpeed.updatedAtMs) / 1000UL;
  }
  if (weather_.rainRate.valid) {
    JsonObject reading = readings["rain"].to<JsonObject>();
    reading["rate_mm_h"] = weather_.rainRate.value;
    reading["age_s"] = (nowMs - weather_.rainRate.updatedAtMs) / 1000UL;
  }

  doc["radio_ready"] = radioReady_;
  doc["archive_ready"] = archiveReady_;
  doc["queued_uploads"] = queuedUploads_;
  doc["records_today"] = recordsToday_;
  doc["last_sent_epoch"] = lastSentEpoch_;
  doc["raw_frames"] = rawFrames_;

  String body;
  serializeJson(doc, body);
  sendJson(200, body);
}

void WebConfigServer::handleGetSettings() {
  JsonDocument doc;
  doc["device_id"] = settings_.deviceId;
  doc["mac"] = settings_.deviceMac;
  doc["server_url"] = settings_.serverBaseUrl;
  doc["api_token"] = settings_.apiToken;
  doc["device_token"] = settings_.deviceToken;
  doc["timezone"] = settings_.timezone;
  doc["site_configured"] = settings_.siteConfigured;
  doc["latitude"] = settings_.latitude;
  doc["longitude"] = settings_.longitude;
  doc["altitude_m"] = settings_.altitudeM;
  doc["config_version"] = settings_.configVersion;
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
  const char* timezone = jsonString(doc["timezone"]);
  if (!isValidTimezoneName(timezone, sizeof(settings_.timezone))) {
    sendJson(400, "{\"error\":\"timezone must be an IANA name of at most 32 characters\"}");
    return;
  }
  const bool siteConfigured = doc["site_configured"] | settings_.siteConfigured;
  float latitude = 0.0f;
  float longitude = 0.0f;
  float altitude = 0.0f;
  if (siteConfigured) {
    latitude = doc["latitude"] | 0.0f;
    longitude = doc["longitude"] | 0.0f;
    altitude = doc["altitude_m"] | 0.0f;
    if (!std::isfinite(latitude) || latitude < -90.0f || latitude > 90.0f) {
      sendJson(400, "{\"error\":\"latitude must be between -90 and 90\"}");
      return;
    }
    if (!std::isfinite(longitude) || longitude < -180.0f || longitude > 180.0f) {
      sendJson(400, "{\"error\":\"longitude must be between -180 and 180\"}");
      return;
    }
    if (!std::isfinite(altitude) || altitude < -500.0f || altitude > 9000.0f) {
      sendJson(400, "{\"error\":\"altitude must be between -500 and 9000\"}");
      return;
    }
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

  copyString(serverUrl, updated.serverBaseUrl, sizeof(updated.serverBaseUrl));
  copyString(apiToken, updated.apiToken, sizeof(updated.apiToken));
  copyString(deviceToken, updated.deviceToken, sizeof(updated.deviceToken));
  copyString(timezone, updated.timezone, sizeof(updated.timezone));
  updated.siteConfigured = siteConfigured;
  updated.latitude = latitude;
  updated.longitude = longitude;
  updated.altitudeM = altitude;
  updated.configVersion = settings_.configVersion + 1;
  // The device ID is the MAC-derived `ak-xxxxxx` and is not editable here.

  const bool networkChanged = !sameProfiles(updated, settings_);
  const Status status = store_.save(updated);
  if (!isOk(status)) {
    sendJson(500, "{\"error\":\"could not save settings\"}");
    return;
  }
  settings_ = updated;
  AK_LOG_INFO("web", "settings saved: profiles=%u network_changed=%s",
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
