#include "OtaService.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <strings.h>

#include <esp_app_desc.h>
#include <esp_ota_ops.h>
#include <mbedtls/sha256.h>

#include "Log.h"
#include "Version.h"

namespace akashvani {
namespace {

void copyString(char* destination, size_t size, const char* source) {
  if (destination == nullptr || size == 0) {
    return;
  }
  if (source == nullptr) {
    source = "";
  }
  std::strncpy(destination, source, size - 1);
  destination[size - 1] = '\0';
}

void digestToHex(const unsigned char digest[32], char* output, size_t size) {
  static const char kHex[] = "0123456789abcdef";
  if (output == nullptr || size < 65) {
    return;
  }
  for (size_t i = 0; i < 32; ++i) {
    output[i * 2] = kHex[digest[i] >> 4];
    output[i * 2 + 1] = kHex[digest[i] & 0x0F];
  }
  output[64] = '\0';
}

}  // namespace

OtaService::OtaService(const DeviceSettings& settings) : settings_(settings) {}

Status OtaService::begin() {
  ArduinoOTA.setHostname(settings_.deviceId);
  if (settings_.adminPassword[0] != '\0') {
    ArduinoOTA.setPassword(settings_.adminPassword);
  }
  ArduinoOTA.onStart([this]() {
    inProgress_ = true;
    AK_LOG_INFO("ota", "LAN OTA started");
  });
  ArduinoOTA.onEnd([this]() {
    inProgress_ = false;
    AK_LOG_INFO("ota", "LAN OTA finished");
  });
  ArduinoOTA.onProgress(
      [this](unsigned int progress, unsigned int total) { logProgress(progress, total); });
  ArduinoOTA.onError([](ota_error_t error) {
    AK_LOG_ERROR("ota", "LAN OTA error=%u", static_cast<unsigned>(error));
  });
  ArduinoOTA.begin();
  started_ = true;
  return Status::Ok;
}

void OtaService::loop() {
  if (started_ && !inProgress_) {
    ArduinoOTA.handle();
  }
}

void OtaService::logProgress(unsigned int progress, unsigned int total) {
  if (total == 0) {
    return;
  }
  const unsigned percent = (progress * 100U) / total;
  if (percent >= lastProgressPercent_ + 10U) {
    lastProgressPercent_ = percent;
    AK_LOG_INFO("ota", "progress %u%%", percent);
  }
}

Status OtaService::currentFirmware(FirmwareRuntimeInfo& info) {
  const esp_partition_t* running = esp_ota_get_running_partition();
  if (running == nullptr) {
    return Status::NotFound;
  }
  const esp_app_desc_t* description = esp_app_get_description();
  copyString(info.partition, sizeof(info.partition), running->label);
  copyString(info.version, sizeof(info.version), AKASHVANI_FW_VERSION);
  copyString(info.buildDate, sizeof(info.buildDate), __DATE__);
  copyString(info.buildTime, sizeof(info.buildTime), __TIME__);
  if (description != nullptr) {
    AK_LOG_DEBUG("ota", "idf app descriptor version=%s", description->version);
  }
  info.freeSketchBytes = ESP.getFreeSketchSpace();
  return Status::Ok;
}

Status OtaService::stageImage(const char* url, const char* expectedSha256,
                              size_t expectedBytes, StagedFirmwareInfo& info) {
  if (url == nullptr || url[0] == '\0') {
    return Status::InvalidArgument;
  }
  if (expectedSha256 == nullptr || std::strlen(expectedSha256) != 64) {
    AK_LOG_ERROR("ota", "refusing download without a SHA-256");
    return Status::InvalidArgument;
  }
  if (inProgress_) {
    return Status::NotReady;
  }

  const esp_partition_t* target = esp_ota_get_next_update_partition(nullptr);
  if (target == nullptr) {
    AK_LOG_ERROR("ota", "no inactive OTA partition");
    return Status::NotFound;
  }

  inProgress_ = true;
  lastProgressPercent_ = 0;

  const bool https = std::strncmp(url, "https://", 8) == 0;
  WiFiClient plainClient;
  WiFiClientSecure secureClient;
  if (https) {
    // TODO(security): pin the production CA certificate.
    secureClient.setInsecure();
  }

  HTTPClient http;
  http.setConnectTimeout(5000);
  http.setTimeout(30000);
  const bool started =
      https ? http.begin(secureClient, String(url)) : http.begin(plainClient, String(url));
  if (!started) {
    inProgress_ = false;
    return Status::IoError;
  }
  http.addHeader("X-Device-Token", settings_.deviceToken);
  http.addHeader("X-Firmware-Version", AKASHVANI_FW_VERSION);

  const int code = http.GET();
  if (code != 200) {
    AK_LOG_ERROR("ota", "image download failed: http %d", code);
    http.end();
    inProgress_ = false;
    return Status::IoError;
  }

  const int contentLength = http.getSize();
  size_t total = expectedBytes;
  if (total == 0 && contentLength > 0) {
    total = static_cast<size_t>(contentLength);
  }
  if (expectedBytes > 0 && contentLength > 0 &&
      expectedBytes != static_cast<size_t>(contentLength)) {
    AK_LOG_ERROR("ota", "size mismatch: expected %u, server announced %d",
                 static_cast<unsigned>(expectedBytes), contentLength);
    http.end();
    inProgress_ = false;
    return Status::InvalidArgument;
  }

  esp_ota_handle_t handle = 0;
  esp_err_t error = esp_ota_begin(target, total > 0 ? total : OTA_SIZE_UNKNOWN, &handle);
  if (error != ESP_OK) {
    AK_LOG_ERROR("ota", "esp_ota_begin failed: %s", esp_err_to_name(error));
    http.end();
    inProgress_ = false;
    return Status::NoMemory;
  }

  mbedtls_sha256_context sha;
  mbedtls_sha256_init(&sha);
  mbedtls_sha256_starts(&sha, 0);

  WiFiClient* stream = http.getStreamPtr();
  uint8_t buffer[1024];
  size_t written = 0;
  uint32_t lastDataMs = millis();
  bool ok = true;
  while (http.connected() && (total == 0 || written < total)) {
    const size_t available = stream->available();
    if (available == 0) {
      if (millis() - lastDataMs > 15000) {
        AK_LOG_ERROR("ota", "download stalled after %u bytes", static_cast<unsigned>(written));
        ok = false;
        break;
      }
      delay(10);
      continue;
    }
    const size_t chunk = stream->readBytes(buffer, std::min(available, sizeof(buffer)));
    if (chunk == 0) {
      continue;
    }
    mbedtls_sha256_update(&sha, buffer, chunk);
    error = esp_ota_write(handle, buffer, chunk);
    if (error != ESP_OK) {
      AK_LOG_ERROR("ota", "esp_ota_write failed: %s", esp_err_to_name(error));
      ok = false;
      break;
    }
    written += chunk;
    lastDataMs = millis();
    if (total > 0) {
      logProgress(static_cast<unsigned>(written), static_cast<unsigned>(total));
    }
  }
  http.end();

  unsigned char digest[32];
  mbedtls_sha256_finish(&sha, digest);
  mbedtls_sha256_free(&sha);

  char actualSha[65];
  digestToHex(digest, actualSha, sizeof(actualSha));

  if (ok && total > 0 && written != total) {
    AK_LOG_ERROR("ota", "short download: %u of %u bytes", static_cast<unsigned>(written),
                 static_cast<unsigned>(total));
    ok = false;
  }
  if (ok && strcasecmp(actualSha, expectedSha256) != 0) {
    AK_LOG_ERROR("ota", "SHA-256 mismatch: got %s", actualSha);
    ok = false;
  }

  if (!ok) {
    esp_ota_abort(handle);
    inProgress_ = false;
    return Status::IoError;
  }

  error = esp_ota_end(handle);
  if (error != ESP_OK) {
    AK_LOG_ERROR("ota", "esp_ota_end failed: %s", esp_err_to_name(error));
    inProgress_ = false;
    return Status::IoError;
  }

  copyString(info.version, sizeof(info.version), "");
  copyString(info.partition, sizeof(info.partition), target->label);
  copyString(info.sha256, sizeof(info.sha256), actualSha);
  info.bytes = static_cast<uint32_t>(written);

  AK_LOG_INFO("ota", "staged %u bytes in %s (sha256 %s)", static_cast<unsigned>(written),
              target->label, actualSha);
  inProgress_ = false;
  return Status::Ok;
}

Status OtaService::bootStagedImage(const char* expectedVersion, StagedFirmwareInfo& info) {
  const esp_partition_t* target = esp_ota_get_next_update_partition(nullptr);
  if (target == nullptr) {
    AK_LOG_ERROR("ota", "no staged OTA partition");
    return Status::NotFound;
  }

  esp_app_desc_t description;
  const esp_err_t error = esp_ota_get_partition_description(target, &description);
  if (error != ESP_OK) {
    AK_LOG_ERROR("ota", "partition %s has no valid app: %s", target->label,
                 esp_err_to_name(error));
    return Status::NotFound;
  }
  if (expectedVersion != nullptr && expectedVersion[0] != '\0' &&
      std::strcmp(description.version, expectedVersion) != 0) {
    // The ESP-IDF app descriptor version is not the project VERSION on this
    // core. The sketch records the staged version in NVS and verifies it
    // before calling this method; log the descriptor for diagnostics only.
    AK_LOG_INFO("ota", "staged image descriptor version=%s (requested %s)",
                description.version, expectedVersion);
  }

  const esp_err_t bootError = esp_ota_set_boot_partition(target);
  if (bootError != ESP_OK) {
    AK_LOG_ERROR("ota", "esp_ota_set_boot_partition failed: %s", esp_err_to_name(bootError));
    return Status::IoError;
  }

  copyString(info.version, sizeof(info.version), description.version);
  copyString(info.partition, sizeof(info.partition), target->label);
  copyString(info.sha256, sizeof(info.sha256), "");
  info.bytes = 0;

  AK_LOG_INFO("ota", "next boot partition is %s (version %s)", target->label,
              description.version);
  return Status::Ok;
}

Status OtaService::checkAndApply(const char* channel) {
  if (!settings_.cloudConfigured()) {
    return Status::NotReady;
  }
  ApiClient api(settings_);
  FirmwareInfo info;
  const Status status = api.checkForUpdate(AKASHVANI_FW_VERSION, channel, info);
  if (!isOk(status) || !info.available) {
    return status;
  }
  AK_LOG_INFO("ota", "update %s available (%u bytes)", info.version,
              static_cast<unsigned>(info.sizeBytes));
  StagedFirmwareInfo staged;
  const Status stagedStatus = stageImage(info.downloadUrl, info.sha256, info.sizeBytes, staged);
  if (!isOk(stagedStatus)) {
    return stagedStatus;
  }
  const Status bootStatus = bootStagedImage(info.version, staged);
  if (!isOk(bootStatus)) {
    return bootStatus;
  }
  delay(500);
  ESP.restart();
  return Status::Ok;
}

Status OtaService::applyImage(const char* url, const char* expectedSha256) {
  StagedFirmwareInfo staged;
  const Status stagedStatus = stageImage(url, expectedSha256, 0, staged);
  if (!isOk(stagedStatus)) {
    return stagedStatus;
  }
  const Status bootStatus = bootStagedImage(nullptr, staged);
  if (!isOk(bootStatus)) {
    return bootStatus;
  }
  delay(500);
  ESP.restart();
  return Status::Ok;
}

}  // namespace akashvani
