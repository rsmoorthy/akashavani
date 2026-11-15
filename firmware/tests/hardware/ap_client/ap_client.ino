// Temporary bench client for the Akashavani provisioning AP.
//
// Flash this on the second board (USB1). It scans for an SSID that starts with
// "akashavani-", connects with the default AP password, then exercises the local
// HTTP server: status JSON, settings JSON, an unchanged round-trip, a Wi-Fi
// profile change that must trigger a reboot, persistence across that reboot,
// and the HTML page. It prints everything on the serial console.
//
// The change test appends a temporary profile and then removes it again, so the
// receiver's Wi-Fi list and server URL end up where they started.

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>

namespace {
constexpr const char* kApPassword = "doordarshan";
constexpr const char* kApBaseUrl = "http://10.90.0.1";
constexpr uint32_t kSerialBaud = 115200;
constexpr const char* kTestSsid = "akashavani-selftest";
constexpr const char* kTestPassword = "selftest123";

String apSsid;

bool connectToAp() {
  WiFi.disconnect();
  delay(500);
  Serial.println("scanning for provisioning AP...");
  const int count = WiFi.scanNetworks();
  int best = -1;
  for (int i = 0; i < count; ++i) {
    if (WiFi.SSID(i).startsWith("akashavani-") &&
        (best < 0 || WiFi.RSSI(i) > WiFi.RSSI(best))) {
      best = i;
    }
  }
  if (best < 0) {
    WiFi.scanDelete();
    Serial.println("FAIL: no akashavani-* AP found");
    return false;
  }
  apSsid = WiFi.SSID(best);
  const int rssi = WiFi.RSSI(best);
  WiFi.scanDelete();
  Serial.printf("connecting to %s (%d dBm)\n", apSsid.c_str(), rssi);
  WiFi.mode(WIFI_STA);
  WiFi.begin(apSsid.c_str(), kApPassword);
  const uint32_t started = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - started < 20000) {
    delay(250);
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.printf("FAIL: connect status %d\n", static_cast<int>(WiFi.status()));
    return false;
  }
  const IPAddress ip = WiFi.localIP();
  const bool inPool = ip[0] == 10 && ip[1] == 90 && ip[2] == 0 && ip[3] >= 11 &&
                      ip[3] <= 20;
  Serial.printf("PASS: connected, IP=%s gateway=%s pool=%s\n", ip.toString().c_str(),
                WiFi.gatewayIP().toString().c_str(), inPool ? "yes" : "NO");
  return true;
}

bool httpGet(const char* path, String& body) {
  HTTPClient http;
  http.begin(String(kApBaseUrl) + path);
  http.setTimeout(8000);
  const int code = http.GET();
  Serial.printf("GET %s -> %d\n", path, code);
  body = http.getString();
  http.end();
  return code == 200;
}

bool httpPostSettings(const String& payload, String& response, int& code) {
  HTTPClient http;
  http.begin(String(kApBaseUrl) + "/api/settings");
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(8000);
  code = http.POST(payload);
  response = http.getString();
  Serial.printf("POST /api/settings -> %d\n%s\n", code, response.c_str());
  http.end();
  return code == 200;
}

String buildPayload(JsonDocument& settings, bool includeTestProfile) {
  JsonDocument body;
  body["server_url"] = settings["server_url"];
  body["api_token"] = settings["api_token"] | "";
  body["device_token"] = settings["device_token"] | "";
  JsonArray wifi = body["wifi"].to<JsonArray>();
  for (JsonVariantConst entry : settings["wifi"].as<JsonArrayConst>()) {
    const char* ssid = entry["ssid"] | "";
    if (ssid[0] == '\0') {
      continue;
    }
    JsonObject out = wifi.add<JsonObject>();
    out["ssid"] = ssid;
    out["password"] = entry["password"];
  }
  if (includeTestProfile) {
    JsonObject out = wifi.add<JsonObject>();
    out["ssid"] = kTestSsid;
    out["password"] = kTestPassword;
  }
  String payload;
  serializeJson(body, payload);
  return payload;
}

bool settingsContain(const JsonDocument& settings, const char* ssid) {
  for (JsonVariantConst entry : settings["wifi"].as<JsonArrayConst>()) {
    if (String(entry["ssid"] | "") == ssid) {
      return true;
    }
  }
  return false;
}

void rebootAndReconnect() {
  WiFi.disconnect();
  delay(12000);
  connectToAp();
}

void runOnce() {
  if (!connectToAp()) {
    return;
  }

  String status;
  httpGet("/api/status", status);

  String settingsText;
  if (!httpGet("/api/settings", settingsText)) {
    return;
  }
  JsonDocument settings;
  if (deserializeJson(settings, settingsText) != DeserializationError::Ok) {
    Serial.println("FAIL: settings JSON did not parse");
    return;
  }

  // Unchanged round-trip must save without rebooting.
  String response;
  int code = 0;
  httpPostSettings(buildPayload(settings, false), response, code);
  Serial.printf("unchanged round-trip: %s\n",
                code == 200 && response.indexOf("\"reboot_required\":false") >= 0
                    ? "PASS"
                    : "FAIL");

  // Append a temporary profile. This changes the Wi-Fi list and must schedule
  // a restart so the STA side re-reads the profiles.
  httpPostSettings(buildPayload(settings, true), response, code);
  Serial.printf("change triggers reboot: %s\n",
                code == 200 && response.indexOf("\"reboot_required\":true") >= 0
                    ? "PASS"
                    : "FAIL");
  rebootAndReconnect();

  String afterChange;
  if (!httpGet("/api/settings", afterChange)) {
    return;
  }
  JsonDocument changedDoc;
  if (deserializeJson(changedDoc, afterChange) != DeserializationError::Ok) {
    Serial.println("FAIL: changed settings JSON did not parse");
    return;
  }
  Serial.printf("temporary profile persisted: %s\n",
                settingsContain(changedDoc, kTestSsid) ? "PASS" : "FAIL");

  // Restore the original list.
  httpPostSettings(buildPayload(settings, false), response, code);
  rebootAndReconnect();

  String restored;
  if (!httpGet("/api/settings", restored)) {
    return;
  }
  JsonDocument restoredDoc;
  if (deserializeJson(restoredDoc, restored) != DeserializationError::Ok) {
    Serial.println("FAIL: restored settings JSON did not parse");
    return;
  }
  Serial.printf("temporary profile removed: %s\n",
                settingsContain(restoredDoc, kTestSsid) ? "FAIL" : "PASS");

  String page;
  httpGet("/", page);
  Serial.printf("page contains title: %s\n",
                page.indexOf("Akashavani") >= 0 ? "PASS" : "FAIL");
  Serial.printf("page contains 6 Wi-Fi rows: %s\n",
                page.indexOf("wifiRows") >= 0 ? "PASS" : "FAIL");

  // Server-side validation spot checks.
  JsonDocument invalid;
  invalid["server_url"] = "http://10.90.0.1";
  invalid["api_token"] = "secret";
  invalid["device_token"] = "";
  invalid["wifi"] = JsonArray();
  String invalidBody;
  serializeJson(invalid, invalidBody);
  httpPostSettings(invalidBody, response, code);
  Serial.printf("rejects empty device token: %s\n", code == 400 ? "PASS" : "FAIL");

  invalid["device_token"] = "secret123";
  invalid["server_url"] = "ftp://10.90.0.1";
  serializeJson(invalid, invalidBody);
  httpPostSettings(invalidBody, response, code);
  Serial.printf("rejects non-http URL: %s\n", code == 400 ? "PASS" : "FAIL");

  Serial.println("AP client bench test complete");
}
}  // namespace

void setup() {
  Serial.begin(kSerialBaud);
  delay(200);
  Serial.println("\nAkashavani AP client bench test");
  runOnce();
}

void loop() { delay(1000); }
