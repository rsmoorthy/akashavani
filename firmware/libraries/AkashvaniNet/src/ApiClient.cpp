#include "ApiClient.h"

#include <cstdio>
#include <cstring>

#include "Log.h"
#include "ObservationJson.h"
#include "Protocol.h"
#include "Version.h"

namespace akashvani {
namespace {

void copyJsonString(const JsonVariantConst& value, char* output, size_t size) {
  if (output == nullptr || size == 0) {
    return;
  }
  const char* text = value.is<const char*>() ? value.as<const char*>() : nullptr;
  if (text == nullptr) {
    output[0] = '\0';
    return;
  }
  std::strncpy(output, text, size - 1);
  output[size - 1] = '\0';
}

}  // namespace

ApiClient::ApiClient(const DeviceSettings& settings) : settings_(settings) {}

void ApiClient::fillClients() {
  if (isHttps()) {
    // TODO(security): load the production CA instead of disabling verification.
    secureClient_.setInsecure();
  }
}

bool ApiClient::isHttps() const {
  return std::strncmp(settings_.serverBaseUrl, "https://", 8) == 0;
}

String ApiClient::endpoint(const char* path) const {
  String base(settings_.serverBaseUrl);
  while (base.endsWith("/")) {
    base.remove(base.length() - 1);
  }
  return base + path;
}

void ApiClient::prepareHttp(HTTPClient& http, const String& url) {
  fillClients();
  if (isHttps()) {
    http.begin(secureClient_, url);
  } else {
    http.begin(plainClient_, url);
  }
  http.setConnectTimeout(5000);
  http.setTimeout(10000);
  http.addHeader("X-Device-Token", settings_.deviceToken);
  http.addHeader("X-Firmware-Version", AKASHVANI_FW_VERSION);
}

ObservationUpload ApiClient::postObservation(const DavisObservation& observation,
                                             const char* messageId,
                                             int64_t observedAtEpoch, int16_t rssiDbm) {
  ObservationUpload result;
  if (messageId == nullptr || messageId[0] == '\0' || observedAtEpoch <= 0) {
    result.status = Status::InvalidArgument;
    std::snprintf(result.error, sizeof(result.error), "missing message id or timestamp");
    return result;
  }

  String body;
  if (!serializeObservationJson(settings_.deviceId, observation, messageId, observedAtEpoch,
                                rssiDbm, body)) {
    result.status = Status::InvalidArgument;
    std::snprintf(result.error, sizeof(result.error), "could not serialize observation");
    return result;
  }

  HTTPClient http;
  prepareHttp(http, endpoint("/api/v1/observations"));
  http.addHeader("Content-Type", "application/json");
  const int code = http.POST(body);
  result.httpCode = code;

  if (code == 200 || code == 201 || code == 202) {
    result.status = Status::Ok;
    const String response = http.getString();
    if (response.length() > 0) {
      JsonDocument responseDoc;
      if (deserializeJson(responseDoc, response) == DeserializationError::Ok) {
        result.duplicate = responseDoc["duplicate"] | false;
      }
    }
  } else if (code == 401 || code == 403) {
    result.status = Status::InvalidArgument;
    std::snprintf(result.error, sizeof(result.error), "cloud rejected credentials");
  } else if (code == 413) {
    result.status = Status::InvalidArgument;
    std::snprintf(result.error, sizeof(result.error), "payload too large");
  } else {
    result.status = Status::IoError;
    result.retryable = code < 0 || code == 429 || code >= 500;
    std::snprintf(result.error, sizeof(result.error), "http %d", code);
  }
  http.end();

  if (result.status == Status::Ok && result.duplicate) {
    AK_LOG_INFO("api", "observation already accepted (duplicate)");
  } else if (result.status != Status::Ok) {
    AK_LOG_WARN("api", "upload failed: %s", result.error);
  }
  return result;
}

Status ApiClient::checkForUpdate(const char* currentVersion, const char* channel,
                                 FirmwareInfo& info) {
  info = FirmwareInfo();
  if (currentVersion == nullptr || settings_.serverBaseUrl[0] == '\0') {
    return Status::InvalidArgument;
  }
  String path = "/api/v1/firmware/check?current_version=";
  path += currentVersion;
  path += "&channel=";
  path += (channel == nullptr || channel[0] == '\0') ? "stable" : channel;

  HTTPClient http;
  prepareHttp(http, endpoint(path.c_str()));
  const int code = http.GET();
  if (code != 200) {
    http.end();
    AK_LOG_WARN("api", "firmware check failed: http %d", code);
    return code == 401 || code == 403 ? Status::InvalidArgument : Status::IoError;
  }

  const String response = http.getString();
  http.end();
  JsonDocument doc;
  if (deserializeJson(doc, response) != DeserializationError::Ok) {
    return Status::IoError;
  }
  info.available = doc["update_available"] | false;
  if (!info.available) {
    return Status::Ok;
  }
  copyJsonString(doc["version"], info.version, sizeof(info.version));
  copyJsonString(doc["download_url"], info.downloadUrl, sizeof(info.downloadUrl));
  copyJsonString(doc["sha256"], info.sha256, sizeof(info.sha256));
  info.sizeBytes = doc["size_bytes"] | static_cast<size_t>(0);
  info.mandatory = doc["mandatory"] | false;
  if (info.downloadUrl[0] == '/') {
    const String absolute = endpoint(info.downloadUrl);
    std::strncpy(info.downloadUrl, absolute.c_str(), sizeof(info.downloadUrl) - 1);
    info.downloadUrl[sizeof(info.downloadUrl) - 1] = '\0';
  }
  return Status::Ok;
}

Status ApiClient::leaseCommands(CommandMessage* output, size_t capacity, size_t& count) {
  count = 0;
  if (output == nullptr || capacity == 0 || !settings_.cloudConfigured()) {
    return Status::InvalidArgument;
  }
  String path = "/api/v1/devices/me/commands/lease?limit=";
  path += String(static_cast<unsigned>(capacity));

  HTTPClient http;
  prepareHttp(http, endpoint(path.c_str()));
  http.addHeader("Content-Type", "application/json");
  const int code = http.POST("{}");
  if (code != 200) {
    http.end();
    return code < 0 ? Status::Timeout : Status::IoError;
  }
  const String response = http.getString();
  http.end();

  JsonDocument doc;
  if (deserializeJson(doc, response) != DeserializationError::Ok || !doc.is<JsonArray>()) {
    return Status::IoError;
  }
  for (JsonObject item : doc.as<JsonArray>()) {
    if (count >= capacity) {
      break;
    }
    CommandMessage& command = output[count];
    copyJsonString(item["id"], command.id, sizeof(command.id));
    copyJsonString(item["type"], command.type, sizeof(command.type));
    String payload;
    serializeJson(item["payload"], payload);
    std::strncpy(command.payloadJson, payload.c_str(), sizeof(command.payloadJson) - 1);
    command.payloadJson[sizeof(command.payloadJson) - 1] = '\0';
    if (command.id[0] != '\0' && command.type[0] != '\0') {
      ++count;
    }
  }
  return Status::Ok;
}

Status ApiClient::acknowledgeCommand(const char* commandId, bool success,
                                     const char* error) {
  if (commandId == nullptr || commandId[0] == '\0') {
    return Status::InvalidArgument;
  }
  JsonDocument doc;
  doc["status"] = success ? "ok" : "error";
  if (error != nullptr && error[0] != '\0') {
    doc["error"] = error;
  }
  String body;
  serializeJson(doc, body);

  String path = "/api/v1/devices/me/commands/";
  path += commandId;
  path += "/ack";
  HTTPClient http;
  prepareHttp(http, endpoint(path.c_str()));
  http.addHeader("Content-Type", "application/json");
  const int code = http.POST(body);
  http.end();
  return (code == 200 || code == 204) ? Status::Ok : Status::IoError;
}

}  // namespace akashvani
