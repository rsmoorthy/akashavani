#pragma once

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

#include "ApiClient.h"
#include "DeviceSettings.h"
#include "Status.h"

namespace akashvani {

struct FirmwareRuntimeInfo {
  char version[33] = {};
  char partition[17] = {};
  char buildDate[24] = {};
  char buildTime[24] = {};
  uint32_t freeSketchBytes = 0;
};

struct StagedFirmwareInfo {
  char version[33] = {};
  char partition[17] = {};
  char sha256[65] = {};
  uint32_t bytes = 0;
};

class OtaService {
 public:
  explicit OtaService(const DeviceSettings& settings);

  // LAN OTA: ArduinoOTA on the site network.
  Status begin();

  // Report the running image.
  Status currentFirmware(FirmwareRuntimeInfo& info);

  // Download into the inactive OTA slot without changing the boot partition.
  // The expected SHA-256 is mandatory in this phase; a mismatch aborts the
  // staged image so it can never be selected at boot.
  Status stageImage(const char* url, const char* expectedSha256, size_t expectedBytes,
                    StagedFirmwareInfo& info);

  // Verify the staged slot and make it the next boot partition. The caller
  // must restart the device after acknowledging the command.
  Status bootStagedImage(const char* expectedVersion, StagedFirmwareInfo& info);

  // Legacy convenience used by the local admin path: check the server, then
  // stage and boot in one step.
  Status checkAndApply(const char* channel = "stable");
  Status applyImage(const char* url, const char* expectedSha256 = nullptr);

  void loop();
  bool inProgress() const { return inProgress_; }

 private:
  void logProgress(unsigned int progress, unsigned int total);

  const DeviceSettings& settings_;
  bool started_ = false;
  bool inProgress_ = false;
  unsigned lastProgressPercent_ = 0;
};

}  // namespace akashvani
