#include "app.h"

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>

#include <cstdio>
#include <cstring>
#include <memory>

#include "AkashvaniCore.h"
#include "AkashvaniDavis.h"
#include "AkashvaniDisplay.h"
#include "AkashvaniNet.h"
#include "AkashvaniProtocol.h"
#include "AkashvaniRadio.h"
#include "AkashvaniRtc.h"
#include "AkashvaniStorage.h"
#include "Rfm95Radio.h"
#include "config.h"

namespace akashvani {
namespace receiver_app {
namespace {

SettingsStore settingsStore;
DeviceSettings settings;
WifiService wifi;
TimeService timeService;
Ds3231Clock rtc(Wire, config::kI2cSdaPin, config::kI2cSclPin);
NullDisplay nullDisplay;
St7735Display panel(SPI, St7735Config{config::kDisplayCsPin, config::kDisplayDcPin,
                                     config::kDisplayResetPin, config::kSpiSckPin,
                                     config::kSpiMisoPin, config::kSpiMosiPin});
IDisplay* display = &nullDisplay;
Rfm95Radio radio(SPI, RadioPins{config::kRadioCsPin, config::kRadioResetPin,
                                config::kRadioDio0Pin, config::kSpiSckPin,
                                config::kSpiMisoPin, config::kSpiMosiPin});

std::unique_ptr<SdArchive> archive;
std::unique_ptr<WebConfigServer> webServer;
std::unique_ptr<OtaService> ota;

bool radioReady = false;
bool archiveReady = false;
bool timeReady = false;
bool otaCheckRequested = false;

uint32_t lastStatusMs = 0;
uint32_t lastNtpAttemptMs = 0;
uint32_t lastNtpSyncMs = 0;
uint32_t lastCommandPollMs = 0;
uint32_t messageCounter = 0;
uint32_t receivedSequence = 0;

DavisObservation lastObservation;
bool haveObservation = false;

struct PendingUpload {
  DavisObservation observation;
  char messageId[65] = {};
  int64_t observedAtEpoch = 0;
  int16_t rssiDbm = 0;
  uint8_t attempts = 0;
  uint32_t nextAttemptMs = 0;
};

PendingUpload pending[config::kMaxPendingUploads];
size_t pendingCount = 0;

void buildMessageId(char* output, size_t size, int64_t epochSeconds) {
  ++messageCounter;
  std::snprintf(output, size, "%s-%lld-%lu", settings.deviceId,
                static_cast<long long>(epochSeconds),
                static_cast<unsigned long>(messageCounter));
}

void shiftPending() {
  if (pendingCount == 0) {
    return;
  }
  for (size_t i = 1; i < pendingCount; ++i) {
    pending[i - 1] = pending[i];
  }
  --pendingCount;
}

void enqueueUpload(const DavisObservation& observation, const char* messageId,
                   int64_t epochSeconds, int16_t rssiDbm) {
  if (pendingCount == config::kMaxPendingUploads) {
    AK_LOG_WARN("upload", "queue full; dropping oldest observation");
    shiftPending();
  }
  PendingUpload& slot = pending[pendingCount++];
  slot.observation = observation;
  std::strncpy(slot.messageId, messageId, sizeof(slot.messageId) - 1);
  slot.messageId[sizeof(slot.messageId) - 1] = '\0';
  slot.observedAtEpoch = epochSeconds;
  slot.rssiDbm = rssiDbm;
  slot.attempts = 0;
  slot.nextAttemptMs = millis();
}

void drainUploads() {
  if (pendingCount == 0 || !settings.uploadEnabled || !settings.cloudConfigured() ||
      !wifi.staConnected() || !timeService.isValid()) {
    return;
  }
  if (static_cast<int32_t>(millis() - pending[0].nextAttemptMs) < 0) {
    return;
  }

  ApiClient api(settings);
  const ObservationUpload result =
      api.postObservation(pending[0].observation, pending[0].messageId,
                          pending[0].observedAtEpoch, pending[0].rssiDbm);
  if (isOk(result.status)) {
    shiftPending();
    return;
  }
  if (result.retryable && pending[0].attempts + 1 < config::kMaxUploadAttempts) {
    ++pending[0].attempts;
    uint32_t backoff = config::kUploadRetryBaseMs << pending[0].attempts;
    if (backoff > config::kUploadRetryMaxMs) {
      backoff = config::kUploadRetryMaxMs;
    }
    pending[0].nextAttemptMs = millis() + backoff;
    return;
  }
  AK_LOG_ERROR("upload", "dropping observation after failure: %s", result.error);
  shiftPending();
}

void handleFrame(const RadioFrame& frame) {
  const DavisDecodeResult decoded = decodeDavisFrame(frame.data, frame.length,
                                                     frame.receivedAtMs);
  if (decoded.status != DavisDecodeStatus::Ok) {
    AK_LOG_DEBUG("rx", "frame len=%u not accepted: %s", static_cast<unsigned>(frame.length),
                 toString(decoded.status));
    Log::hexDump(LogLevel::Debug, "rx", frame.data, frame.length);
    return;
  }

  const int64_t epoch = timeService.nowEpoch();
  if (epoch < 946684800LL) {
    AK_LOG_WARN("rx", "decoded frame but clock is invalid; dropping");
    return;
  }

  DavisObservation observation = decoded.observation;
  observation.observedAtEpoch = epoch;
  if (observation.sequence == 0) {
    observation.sequence = ++receivedSequence;
  }

  char messageId[65];
  buildMessageId(messageId, sizeof(messageId), epoch);
  AK_LOG_INFO("rx", "seq=%lu temp=%.1fC hum=%.0f%% rssi=%ddBm",
              static_cast<unsigned long>(observation.sequence), observation.temperatureC,
              observation.humidityPct, frame.rssiDbm);

  lastObservation = observation;
  haveObservation = true;

  if (settings.archiveEnabled && archiveReady && archive) {
    String record;
    if (serializeObservationJson(settings.deviceId, observation, messageId, epoch,
                                 frame.rssiDbm, record)) {
      const Status status = archive->append(record.c_str(), record.length());
      if (!isOk(status)) {
        AK_LOG_ERROR("archive", "append failed: %s", statusToString(status));
      }
    }
  }

  if (settings.uploadEnabled && settings.cloudConfigured()) {
    enqueueUpload(observation, messageId, epoch, frame.rssiDbm);
  }
}

void pollCommands() {
  const uint32_t intervalMs =
      static_cast<uint32_t>(settings.commandPollIntervalSeconds) * 1000UL;
  if (!settings.cloudConfigured() || !wifi.staConnected() || !timeService.isValid()) {
    return;
  }
  if (millis() - lastCommandPollMs < intervalMs) {
    return;
  }
  lastCommandPollMs = millis();

  ApiClient api(settings);
  CommandMessage commands[4];
  size_t count = 0;
  const Status status = api.leaseCommands(commands, 4, count);
  if (!isOk(status)) {
    AK_LOG_DEBUG("cmd", "lease failed: %s", statusToString(status));
    return;
  }

  for (size_t i = 0; i < count; ++i) {
    const protocol::CommandType type = protocol::commandTypeFromString(commands[i].type);
    bool ok = true;
    const char* error = nullptr;
    bool reboot = false;

    switch (type) {
      case protocol::CommandType::Reboot:
        reboot = true;
        break;
      case protocol::CommandType::SyncTime:
        ok = isOk(timeService.syncFromNtp());
        if (!ok) {
          error = "sntp failed";
        }
        break;
      case protocol::CommandType::CheckForUpdate:
        otaCheckRequested = true;
        break;
      case protocol::CommandType::SelfTest:
        break;
      case protocol::CommandType::SetConfig:
      case protocol::CommandType::UploadLogs:
      case protocol::CommandType::Unknown:
        ok = false;
        error = "not implemented";
        break;
    }

    api.acknowledgeCommand(commands[i].id, ok, error);
    AK_LOG_INFO("cmd", "%s -> %s", commands[i].type, ok ? "ok" : error);
    if (reboot) {
      delay(200);
      ESP.restart();
    }
  }
}

void syncTimeWhenOnline() {
  if (!wifi.staConnected()) {
    return;
  }
  const bool valid = timeService.isValid();
  const uint32_t attemptGap = valid ? 60000UL : config::kNtpRetryIntervalMs;
  if (lastNtpAttemptMs != 0 && millis() - lastNtpAttemptMs < attemptGap) {
    return;
  }
  if (valid && lastNtpSyncMs != 0 &&
      millis() - lastNtpSyncMs < config::kNtpSyncIntervalMs) {
    return;
  }
  lastNtpAttemptMs = millis();
  if (isOk(timeService.syncFromNtp())) {
    lastNtpSyncMs = millis();
  }
}

void updateDisplay() {
  if (millis() - lastStatusMs < config::kStatusIntervalMs) {
    return;
  }
  lastStatusMs = millis();

  DisplayState state;
  std::snprintf(state.line1, sizeof(state.line1), "WiFi %s  %s",
                wifi.staConnected() ? "up" : "AP",
                wifi.staConnected() ? wifi.status().staIp : wifi.status().apIp);
  if (haveObservation) {
    std::snprintf(state.line2, sizeof(state.line2), "T %.1fC  H %.0f%%",
                  lastObservation.temperatureC, lastObservation.humidityPct);
    std::snprintf(state.line3, sizeof(state.line3), "Wind %.1f m/s  Rain %.1f mm",
                  lastObservation.windSpeedMs, lastObservation.rainDayMm);
  } else {
    std::snprintf(state.line2, sizeof(state.line2), "No RF data yet");
    std::snprintf(state.line3, sizeof(state.line3), "radio %s",
                  radioReady ? "ready" : "stub");
  }
  state.wifiConnected = wifi.staConnected();
  state.radioReady = radioReady;
  state.timeValid = timeReady || timeService.isValid();
  state.archiveReady = archiveReady;
  state.queuedUploads = static_cast<uint32_t>(pendingCount);
  state.recordsToday = archive ? static_cast<uint32_t>(archive->recordsWritten()) : 0;
  state.rssiDbm = wifi.status().rssiDbm;
  display->render(state);

  if (webServer) {
    webServer->setSubsystemStatus(radioReady, archiveReady);
    webServer->setCounters(state.queuedUploads, state.recordsToday);
  }
  digitalWrite(config::kStatusLedPin, wifi.staConnected() ? HIGH : LOW);
}

void logBanner() {
  AK_LOG_INFO("boot", "%s %s | protocol v%d", AKASHVANI_PRODUCT_NAME, AKASHVANI_FW_VERSION,
              AKASHVANI_PROTOCOL_VERSION);
}

}  // namespace

void setup() {
  Log::begin(config::kSerialBaud);
  Log::setLevel(LogLevel::Info);
  delay(100);
  logBanner();

  pinMode(config::kStatusLedPin, OUTPUT);
  digitalWrite(config::kStatusLedPin, LOW);

  if (!isOk(settingsStore.begin())) {
    AK_LOG_ERROR("boot", "settings store unavailable");
  }
  const Status settingsStatus = settingsStore.load(settings);
  if (!isOk(settingsStatus)) {
    AK_LOG_ERROR("boot", "settings load failed: %s", statusToString(settingsStatus));
  }

  if (config::kEnableDisplay) {
    display = &panel;
  }
  display->begin();
  if (config::kEnableRtc) {
    const Status rtcStatus = rtc.begin();
    timeService.begin(rtc);
    if (!isOk(rtcStatus)) {
      AK_LOG_WARN("boot", "RTC unavailable; waiting for SNTP");
    }
  }

  if (settings.archiveEnabled && config::kEnableSdArchive) {
    SdArchiveConfig sdConfig;
    sdConfig.csPin = config::kSdCsPin;
    sdConfig.sckPin = config::kSpiSckPin;
    sdConfig.misoPin = config::kSpiMisoPin;
    sdConfig.mosiPin = config::kSpiMosiPin;
    sdConfig.frequencyHz = config::kSdFrequencyHz;
    sdConfig.deviceId = settings.deviceId;
    archive = std::make_unique<SdArchive>(SPI, sdConfig);
    archiveReady = isOk(archive->begin());
  }

  RadioConfig radioConfig;
  radioConfig.chip = RadioChip::Rfm95;
  radioConfig.frequencyMhz = config::kRadioFrequencyMhz;
  radioConfig.modulation = RadioModulation::Fsk;
  radioConfig.bitrateKbps = config::kRadioBitrateKbps;
  radioConfig.deviationKhz = config::kRadioDeviationKhz;
  radioConfig.bandwidthKhz = config::kRadioBandwidthKhz;
  radioConfig.powerDbm = config::kRadioPowerDbm;
  const Status radioStatus = radio.begin(radioConfig);
  radioReady = isOk(radioStatus);  if (!radioReady) {
    AK_LOG_WARN("boot", "radio driver not active (%s); port prototype driver",
                statusToString(radioStatus));
  }

  wifi.begin(settings);

  webServer = std::make_unique<WebConfigServer>(settings, settingsStore, wifi, timeService);
  webServer->setSubsystemStatus(radioReady, archiveReady);
  webServer->begin();

  if (config::kEnableLanOta) {
    ota = std::make_unique<OtaService>(settings);
    ota->begin();
  }

  AK_LOG_INFO("boot", "ready: AP=%s device=%s", settings.apSsid, settings.deviceId);
}

void loop() {
  wifi.loop();
  if (webServer) {
    webServer->loop();
  }
  if (ota) {
    ota->loop();
  }
  display->loop();

  if (radioReady) {
    RadioFrame frame;
    if (isOk(radio.receive(frame, config::kRadioReceiveTimeoutMs))) {
      handleFrame(frame);
    }
  }

  timeReady = timeService.isValid();
  syncTimeWhenOnline();
  drainUploads();
  pollCommands();

  if (otaCheckRequested) {
    otaCheckRequested = false;
    if (ota) {
      ota->checkAndApply("stable");
    }
  }

  updateDisplay();
  delay(config::kLoopDelayMs);
}

}  // namespace receiver_app
}  // namespace akashvani
