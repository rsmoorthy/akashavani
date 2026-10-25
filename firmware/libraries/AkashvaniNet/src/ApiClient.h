#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

#include <cstddef>
#include <cstdint>

#include "DavisObservation.h"
#include "DeviceSettings.h"
#include "Status.h"

namespace akashvani {

struct ObservationUpload {
  Status status = Status::NotReady;
  int httpCode = 0;
  bool duplicate = false;
  bool retryable = false;
  char error[96] = {};
};

struct FirmwareInfo {
  bool available = false;
  char version[33] = {};
  char downloadUrl[192] = {};
  char sha256[65] = {};
  size_t sizeBytes = 0;
  bool mandatory = false;
};

struct CommandMessage {
  char id[37] = {};
  char type[32] = {};
  char payloadJson[512] = "{}";
};

class ApiClient {
 public:
  explicit ApiClient(const DeviceSettings& settings);

  ObservationUpload postObservation(const DavisObservation& observation,
                                    const char* messageId, int64_t observedAtEpoch,
                                    int16_t rssiDbm);

  Status checkForUpdate(const char* currentVersion, const char* channel,
                        FirmwareInfo& info);

  Status leaseCommands(CommandMessage* output, size_t capacity, size_t& count);
  Status acknowledgeCommand(const char* commandId, bool success, const char* error = nullptr);

 private:
  String endpoint(const char* path) const;
  bool isHttps() const;
  void fillClients();
  void prepareHttp(HTTPClient& http, const String& url);

  const DeviceSettings& settings_;
  WiFiClient plainClient_;
  WiFiClientSecure secureClient_;
};

}  // namespace akashvani
