#include "ApiClient.h"

#include <WiFi.h>

#include <cstdio>
#include <cstring>

#include "Log.h"
#include "ConfigJson.h"
#include "ObservationJson.h"
#include "Protocol.h"
#include "Version.h"

namespace akashavani {
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

ApiClient::ApiClient(const DeviceSettings& settings, int64_t lastRadioEpoch,
                     const DeviceHealth& health)
    : settings_(settings), lastRadioEpoch_(lastRadioEpoch), health_(health) {}

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
  if (settings_.apiToken[0] != '\0') {
    http.addHeader("Authorization", String("Bearer ") + settings_.apiToken);
  }
  http.addHeader("X-Firmware-Version", AKASHAVANI_FW_VERSION);
  const char* mac = settings_.deviceMac[0] != '\0' ? settings_.deviceMac
                                                   : WiFi.macAddress().c_str();
  http.addHeader("X-Device-Mac", mac);
  char lastRadio[24];
  if (lastRadioEpoch_ > 0) {
    std::snprintf(lastRadio, sizeof(lastRadio), "%lld",
                  static_cast<long long>(lastRadioEpoch_));
  } else {
    std::snprintf(lastRadio, sizeof(lastRadio), "0");
  }
  http.addHeader("X-Last-Radio-At", lastRadio);
  char value[24];
  std::snprintf(value, sizeof(value), "%lu",
                static_cast<unsigned long>(health_.uptimeSeconds));
  http.addHeader("X-Uptime-Seconds", value);
  std::snprintf(value, sizeof(value), "%u",
                static_cast<unsigned>(health_.queueDepth));
  http.addHeader("X-Queue-Depth", value);
  std::snprintf(value, sizeof(value), "%lu", static_cast<unsigned long>(health_.freeHeap));
  http.addHeader("X-Free-Heap", value);
  std::snprintf(value, sizeof(value), "%d", static_cast<int>(health_.wifiRssi));
  http.addHeader("X-Wifi-Rssi", value);
  std::snprintf(value, sizeof(value), "%lu",
                static_cast<unsigned long>(health_.radioFramesValid));
  http.addHeader("X-Radio-Frames-Valid", value);
  std::snprintf(value, sizeof(value), "%d", static_cast<int>(health_.radioRssiDbm));
  http.addHeader("X-Radio-Rssi", value);
  http.addHeader("X-Reset-Reason", health_.resetReason == nullptr ? "unknown"
                                                                  : health_.resetReason);
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

ArchiveUpload ApiClient::postArchive(const char* payloadJson, const char* messageId) {
  ArchiveUpload result;
  if (payloadJson == nullptr || payloadJson[0] == '\0' || messageId == nullptr ||
      messageId[0] == '\0' || settings_.apiToken[0] == '\0') {
    result.status = Status::InvalidArgument;
    std::snprintf(result.error, sizeof(result.error), "missing archive payload or token");
    return result;
  }

  HTTPClient http;
  prepareHttp(http, endpoint("/api/v1/archive"));
  http.addHeader("Content-Type", "application/json");
  const int code = http.POST(String(payloadJson));
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
    std::snprintf(result.error, sizeof(result.error), "server rejected bearer token");
  } else if (code == 413 || code == 422) {
    result.status = Status::InvalidArgument;
    std::snprintf(result.error, sizeof(result.error), "archive rejected: http %d", code);
  } else {
    result.status = Status::IoError;
    result.retryable = code < 0 || code == 429 || code >= 500;
    std::snprintf(result.error, sizeof(result.error), "http %d", code);
  }
  http.end();

  if (result.status == Status::Ok && result.duplicate) {
    AK_LOG_INFO("api", "archive already accepted (duplicate)");
  } else if (result.status != Status::Ok) {
    AK_LOG_WARN("api", "archive upload failed: %s", result.error);
  }
  return result;
}


Status ApiClient::postStorageList(const char* commandId, const char* path,
                                  const char* jsonBody) {
  if (commandId == nullptr || commandId[0] == '\0' || path == nullptr ||
      path[0] == '\0' || jsonBody == nullptr) {
    return Status::InvalidArgument;
  }
  HTTPClient http;
  prepareHttp(http, endpoint("/api/v1/devices/me/storage/list"));
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Storage-Command", commandId);
  http.addHeader("X-Storage-Path", path);
  const int code = http.POST(String(jsonBody));
  http.end();
  if (code == 200 || code == 201 || code == 202) {
    return Status::Ok;
  }
  AK_LOG_WARN("api", "storage list upload failed: http %d", code);
  return code <= 0 ? Status::Timeout : Status::IoError;
}

Status ApiClient::postStorageChunk(const char* commandId, const char* path,
                                   uint32_t offset, bool eof, const uint8_t* data,
                                   size_t length) {
  if (commandId == nullptr || commandId[0] == '\0' || path == nullptr ||
      path[0] == '\0' || data == nullptr || length == 0) {
    return Status::InvalidArgument;
  }
  HTTPClient http;
  prepareHttp(http, endpoint("/api/v1/devices/me/storage/file"));
  http.addHeader("Content-Type", "application/octet-stream");
  http.addHeader("X-Storage-Command", commandId);
  http.addHeader("X-Storage-Path", path);
  char value[24];
  std::snprintf(value, sizeof(value), "%lu", static_cast<unsigned long>(offset));
  http.addHeader("X-Storage-Offset", value);
  http.addHeader("X-Storage-EOF", eof ? "1" : "0");
  const int code = http.POST(const_cast<uint8_t*>(data), length);
  http.end();
  if (code == 200 || code == 201 || code == 202) {
    return Status::Ok;
  }
  AK_LOG_WARN("api", "storage chunk upload failed: http %d", code);
  return code <= 0 ? Status::Timeout : Status::IoError;
}

Status ApiClient::postRawBatch(const char* jsonBody) {
  if (jsonBody == nullptr) {
    return Status::InvalidArgument;
  }
  HTTPClient http;
  const String url = endpoint("/api/v1/devices/me/raw");
  prepareHttp(http, url);
  http.addHeader("Content-Type", "application/json");
  const int code = http.POST(String(jsonBody));
  http.end();
  if (code == 202 || code == 200) {
    return Status::Ok;
  }
  AK_LOG_WARN("api", "raw batch failed: http %d", code);
  return code <= 0 ? Status::Timeout : Status::IoError;
}

ArchiveBatchResult ApiClient::postArchiveBatch(const uint8_t* body, size_t length) {
  ArchiveBatchResult result;
  if (body == nullptr || length == 0) {
    result.status = Status::InvalidArgument;
    return result;
  }
  HTTPClient http;
  const String url = endpoint("/api/v1/archive/batch");
  prepareHttp(http, url);
  http.addHeader("Content-Type", "application/msgpack");
  const int code = http.POST(const_cast<uint8_t*>(body), length);
  const String response = http.getString();
  http.end();
  if (code == 202 || code == 200) {
    result.status = Status::Ok;
    JsonDocument doc;
    if (deserializeJson(doc, response) == DeserializationError::Ok) {
      result.accepted = doc["accepted"] | 0;
      result.duplicates = doc["duplicates"] | 0;
      result.rejected = doc["rejected"] | 0;
    }
    return result;
  }
  result.status = code <= 0 ? Status::Timeout : Status::IoError;
  std::snprintf(result.error, sizeof(result.error), "http %d", code);
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

Status ApiClient::putDeviceConfig() {
  if (!settings_.cloudConfigured()) {
    return Status::NotReady;
  }
  String body;
  if (!serializeDeviceConfigJson(settings_, body)) {
    return Status::InvalidArgument;
  }
  HTTPClient http;
  prepareHttp(http, endpoint("/api/v1/devices/me/config"));
  http.addHeader("Content-Type", "application/json");
  const int code = http.PUT(body);
  http.end();
  return (code == 200 || code == 201) ? Status::Ok : Status::IoError;
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
                                     const char* error, const char* resultJson) {
  if (commandId == nullptr || commandId[0] == '\0') {
    return Status::InvalidArgument;
  }
  JsonDocument doc;
  doc["status"] = success ? "ok" : "error";
  if (error != nullptr && error[0] != '\0') {
    doc["error"] = error;
  }
  if (resultJson != nullptr && resultJson[0] != '\0') {
    JsonDocument resultDoc;
    if (deserializeJson(resultDoc, resultJson) == DeserializationError::Ok) {
      doc["result"] = resultDoc.as<JsonVariant>();
    }
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

}  // namespace akashavani
