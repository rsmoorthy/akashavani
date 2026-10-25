#include "OtaService.h"

#include <algorithm>
#include <cstring>

#include "Log.h"
#include "Version.h"

namespace akashvani {

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
  return applyImage(info.downloadUrl, info.sha256);
}

Status OtaService::applyImage(const char* url, const char* expectedSha256) {
  if (url == nullptr || url[0] == '\0') {
    return Status::InvalidArgument;
  }
  if (inProgress_) {
    return Status::NotReady;
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
  http.setTimeout(15000);
  const bool started = https ? http.begin(secureClient, String(url))
                             : http.begin(plainClient, String(url));
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
  if (!Update.begin(contentLength > 0 ? static_cast<size_t>(contentLength)
                                      : UPDATE_SIZE_UNKNOWN)) {
    AK_LOG_ERROR("ota", "Update.begin failed: %s", Update.errorString());
    http.end();
    inProgress_ = false;
    return Status::NoMemory;
  }

  // TODO(security): stream SHA-256 while writing and refuse to boot on mismatch.
  (void)expectedSha256;
  WiFiClient* stream = http.getStreamPtr();
  uint8_t buffer[1024];
  size_t written = 0;
  uint32_t lastDataMs = millis();
  while (http.connected() && (contentLength < 0 || written < static_cast<size_t>(contentLength))) {
    const size_t available = stream->available();
    if (available == 0) {
      if (millis() - lastDataMs > 15000) {
        AK_LOG_ERROR("ota", "download stalled");
        break;
      }
      delay(10);
      continue;
    }
    const size_t chunk = stream->readBytes(buffer, std::min(available, sizeof(buffer)));
    if (Update.write(buffer, chunk) != chunk) {
      AK_LOG_ERROR("ota", "flash write failed: %s", Update.errorString());
      break;
    }
    written += chunk;
    lastDataMs = millis();
    if (contentLength > 0) {
      logProgress(static_cast<unsigned>(written), static_cast<unsigned>(contentLength));
    }
  }
  http.end();

  const bool complete =
      (contentLength > 0 && written == static_cast<size_t>(contentLength)) ||
      (contentLength < 0 && written > 0);
  if (!complete || !Update.end(true)) {
    AK_LOG_ERROR("ota", "update failed: %s", Update.errorString());
    Update.abort();
    inProgress_ = false;
    return Status::IoError;
  }

  AK_LOG_INFO("ota", "update written (%u bytes); rebooting", static_cast<unsigned>(written));
  delay(500);
  ESP.restart();
  return Status::Ok;
}

}  // namespace akashvani
