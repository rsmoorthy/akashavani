#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

#include <cstddef>
#include <cstdint>

#include "ArchiveRecord.h"
#include "DavisObservation.h"
#include "DeviceSettings.h"
#include "Status.h"

namespace akashvani {

struct ArchiveUpload {
  Status status = Status::NotReady;
  int httpCode = 0;
  bool duplicate = false;
  bool retryable = false;
  char error[96] = {};
};

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

struct ArchiveBatchResult {
  Status status = Status::NotReady;
  int accepted = 0;
  int duplicates = 0;
  int rejected = 0;
  char error[96] = {};
};

struct CommandMessage {
  char id[37] = {};
  char type[32] = {};
  char payloadJson[512] = "{}";
};

class ApiClient {
 public:
  explicit ApiClient(const DeviceSettings& settings, int64_t lastRadioEpoch = 0);

  ObservationUpload postObservation(const DavisObservation& observation,
                                    const char* messageId, int64_t observedAtEpoch,
                                    int16_t rssiDbm);

  // Uploads a pre-serialized archive record. The archive API uses the shared
  // bearer token from the hotspot configuration page.
  ArchiveUpload postArchive(const char* payloadJson, const char* messageId);

  // Uploads a JSON array of archive records (catch-up batches). The server
  // returns one outcome per record.
  ArchiveBatchResult postArchiveBatch(const char* jsonArray);

  Status checkForUpdate(const char* currentVersion, const char* channel,
                        FirmwareInfo& info);

  Status leaseCommands(CommandMessage* output, size_t capacity, size_t& count);
  Status acknowledgeCommand(const char* commandId, bool success, const char* error = nullptr,
                            const char* resultJson = nullptr);

 private:
  String endpoint(const char* path) const;
  bool isHttps() const;
  void fillClients();
  void prepareHttp(HTTPClient& http, const String& url);

  const DeviceSettings& settings_;
  int64_t lastRadioEpoch_ = 0;
  WiFiClient plainClient_;
  WiFiClientSecure secureClient_;
};

}  // namespace akashvani
