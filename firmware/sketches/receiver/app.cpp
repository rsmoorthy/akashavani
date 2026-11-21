#include "app.h"

#include <Adafruit_BME280.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <SPI.h>
#include <WiFi.h>
#include <Wire.h>
#include <esp_random.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>

#include "AkashavaniArchive.h"
#include "AkashavaniCore.h"
#include "AkashavaniDavis.h"
#include "AkashavaniDisplay.h"
#include "AkashavaniNet.h"
#include "AkashavaniProtocol.h"
#include "AkashavaniRadio.h"
#include "AkashavaniRtc.h"
#include "AkashavaniStorage.h"
#include "ArchiveJson.h"
#include "DavisChannels.h"
#include "Rfm69Radio.h"
#include "config.h"

namespace akashavani {
namespace receiver_app {
namespace {

SettingsStore settingsStore;
DeviceSettings settings;
WifiService wifi;
TimeService timeService;
Ds3231Clock rtc(Wire, config::kI2cSdaPin, config::kI2cSclPin);
NullDisplay nullDisplay;

Ssd1306Config ssd1306Config() {
  Ssd1306Config value;
  value.address = config::kDisplayAddress;
  value.sdaPin = config::kI2cSdaPin;
  value.sclPin = config::kI2cSclPin;
  return value;
}

Ssd1306Display panel(Wire, ssd1306Config());
IDisplay* display = &nullDisplay;
Rfm69Radio radio(SPI, RadioPins{config::kRadioCsPin, config::kRadioResetPin,
                                config::kRadioDio0Pin, config::kSpiSckPin,
                                config::kSpiMisoPin, config::kSpiMosiPin});
Adafruit_BME280 bme;
bool bmeReady = false;
float latestPressureHpa = NAN;
float latestInTempC = NAN;
float latestInHumidityPct = NAN;
uint32_t lastBmeSampleMs = 0;
ArchiveCollector archiveCollector(300, 3000);

std::unique_ptr<SdArchive> archive;
RawLog rawLog(RawLogConfig{});
bool rawLogReady = false;
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
uint32_t lastConfigReportMs = 0;
uint32_t lastConfigAttemptMs = 0;
uint32_t reportedConfigVersion = 0;
bool configReportPending = true;
uint32_t receivedSequence = 0;
uint32_t lastRadioMissedSlots = 0;
uint32_t lastRadioResyncCount = 0;
uint32_t lastRadioSearchCount = 0;
uint32_t lastRadioBadFrames = 0;
uint32_t lastBadLogMs = 0;

DavisObservation lastObservation;
bool haveObservation = false;
uint32_t lastObservationAtMs = 0;
// Davis frames carry one message-specific field at a time, so the status page
// keeps the most recent value of each parameter separately.
WeatherReading lastTemperature;
WeatherReading lastInTemperature;
WeatherReading lastWindSpeed;
WeatherReading lastRainRate;
// Epoch of the last CRC-valid Davis frame, reported as X-Last-Radio-At on every
// API request. Zero means "never seen".
int64_t lastRadioEpoch = 0;
// Last CRC-valid frame's RSSI, used when the current bucket has no samples yet.
int16_t lastValidRssiDbm = 0;
bool haveValidRssi = false;
const char* bootResetReason = "unknown";
// Random 64-bit identity for this boot, generated in setup(). Archive rows
// carry it so the server can group buckets by boot and count restarts.
uint64_t bootId = 0;

// Newest archive bucket the server acknowledged; persisted on SD as last_sent.
int64_t lastSentEpoch = 0;
bool replayActive = false;
uint32_t lastReplayAttemptMs = 0;
int64_t replayBatchLastEpoch = 0;

constexpr size_t kCatchupBatchRecords = 20;
constexpr size_t kCatchupLineBytes = 1536;
constexpr size_t kCatchupBodyBytes = kCatchupBatchRecords * kCatchupLineBytes + 32;
char catchupBody[kCatchupBodyBytes];

size_t buildReplayBatch(int64_t& lastEpoch, size_t& byteLength);
bool runRangeReplay(int64_t startEpoch, int64_t endEpoch, uint32_t& sent, char* error,
                    size_t errorSize);

// ---------------------------------------------------------------------------
// RF/Davis bench test loop
// ---------------------------------------------------------------------------
// Enabled by config::kRfTestMode. It deliberately keeps Wi-Fi, SD, OTA and the
// cloud path out of the timing-sensitive RF loop while the RF link and channel
// sync are being characterised. Every valid frame is logged with its channel,
// simulator sequence, RSSI and decoded fields. See docs/specs/rf-davis-receive.md.
uint8_t rfChannel = config::kRadioInitialChannel;
uint32_t lastRfHopMs = 0;
uint32_t lastRfRxMs = 0;
bool haveRfRx = false;
uint8_t rfMissStreak = 0;
bool rfSearchMode = false;
uint32_t rfValidFrames = 0;
uint32_t rfBadCrcFrames = 0;
uint32_t rfResyncs = 0;
uint32_t rfGapSlots = 0;
uint16_t rfMaxGap = 0;
int32_t rfRssiSum = 0;
int16_t rfRssiMin = 0;
int16_t rfRssiMax = 0;
int16_t lastRfSequence = -1;
uint32_t lastRfStatsMs = 0;
char rfCommandBuffer[24];
size_t rfCommandLength = 0;

const char* rfMessageName(DavisMessageType type) { return toString(type); }

void formatHex(const uint8_t* data, size_t length, char* output, size_t outputSize) {
  if (outputSize == 0) {
    return;
  }
  output[0] = '\0';
  size_t used = 0;
  for (size_t i = 0; i < length && used + 4 <= outputSize; ++i) {
    const int written = std::snprintf(output + used, outputSize - used, "%02X%s", data[i],
                                      i + 1 == length ? "" : " ");
    if (written <= 0) {
      break;
    }
    used += static_cast<size_t>(written);
  }
}

void logRfStats(uint32_t now) {
  const int16_t average =
      rfValidFrames == 0 ? 0 : static_cast<int16_t>(rfRssiSum / static_cast<int32_t>(rfValidFrames));
  AK_LOG_INFO("stats",
              "STATS t=%lu ch=%u valid=%lu bad_crc=%lu gap_slots=%lu max_gap=%u resyncs=%lu "
              "rssi_avg=%d rssi_min=%d rssi_max=%d search=%u",
              static_cast<unsigned long>(now), static_cast<unsigned>(rfChannel),
              static_cast<unsigned long>(rfValidFrames),
              static_cast<unsigned long>(rfBadCrcFrames),
              static_cast<unsigned long>(rfGapSlots), static_cast<unsigned>(rfMaxGap),
              static_cast<unsigned long>(rfResyncs), average, rfRssiMin, rfRssiMax,
              rfSearchMode ? 1u : 0u);
  lastRfStatsMs = now;
}

void rfResetState(const char* reason) {
  rfChannel = config::kRadioInitialChannel;
  lastRfHopMs = millis();
  lastRfRxMs = 0;
  haveRfRx = false;
  rfMissStreak = 0;
  rfSearchMode = false;
  rfValidFrames = 0;
  rfBadCrcFrames = 0;
  rfResyncs = 0;
  rfGapSlots = 0;
  rfMaxGap = 0;
  rfRssiSum = 0;
  rfRssiMin = 0;
  rfRssiMax = 0;
  lastRfSequence = -1;
  lastRfStatsMs = lastRfHopMs;
  const Status status = radio.setChannel(rfChannel);
  AK_LOG_INFO("bench", "RESET reason=%s ch=%u status=%s", reason,
              static_cast<unsigned>(rfChannel), statusToString(status));
}

void pollRfSerial() {
  while (Serial.available() > 0) {
    const char c = static_cast<char>(Serial.read());
    if (c == '\r' || c == '\n') {
      rfCommandBuffer[rfCommandLength] = '\0';
      if (std::strcmp(rfCommandBuffer, "R") == 0) {
        rfResetState("serial");
      } else if (std::strcmp(rfCommandBuffer, "S") == 0 ||
                 std::strcmp(rfCommandBuffer, "?") == 0) {
        logRfStats(millis());
      } else if (rfCommandLength > 0) {
        AK_LOG_WARN("bench", "unknown command '%s' (use R, S, ?)", rfCommandBuffer);
      }
      rfCommandLength = 0;
    } else if (rfCommandLength + 1 < sizeof(rfCommandBuffer)) {
      rfCommandBuffer[rfCommandLength++] = c;
    }
  }
}

void rfHop(bool backwards, const char* reason) {
  const uint8_t from = rfChannel;
  rfChannel = backwards ? previousDavisChannel(rfChannel) : nextDavisChannel(rfChannel);
  const Status status = radio.setChannel(rfChannel);
  lastRfHopMs = millis();
  if (!isOk(status)) {
    AK_LOG_ERROR("hop", "setChannel(%u) failed: %s", static_cast<unsigned>(rfChannel),
                 statusToString(status));
    return;
  }
  AK_LOG_INFO("hop", "HOP t=%lu from=%u to=%u freq=%.3fMHz reason=%s miss=%u search=%u",
              static_cast<unsigned long>(lastRfHopMs), static_cast<unsigned>(from),
              static_cast<unsigned>(rfChannel), davisChannelFrequencyMhz(rfChannel), reason,
              static_cast<unsigned>(rfMissStreak), rfSearchMode ? 1u : 0u);
}

void rfHandleFrame(const RadioFrame& frame, uint32_t now) {
  const DavisDecodeResult decoded = decodeDavisFrame(frame.data, frame.length, now);
  char raw[3 * kDavisFrameBytes + 1];
  formatHex(frame.data, frame.length, raw, sizeof(raw));

  const int16_t sequence = decoded.status == DavisDecodeStatus::Ok
                               ? static_cast<int16_t>(decoded.benchSequence)
                               : static_cast<int16_t>(frame.data[5]);

  if (decoded.status != DavisDecodeStatus::Ok) {
    ++rfBadCrcFrames;
    AK_LOG_WARN("rx",
                "RX_BAD t=%lu ch=%u seq=%u rssi=%d status=%s raw=%s",
                static_cast<unsigned long>(now), static_cast<unsigned>(rfChannel),
                static_cast<unsigned>(sequence), frame.rssiDbm, toString(decoded.status), raw);
    // Match the prototype: a failed CRC is not treated as a received packet and
    // does not advance the channel. Noise can pass the two-byte sync word with
    // tolerance 2, so hopping on bad frames would break a good lock.
    return;
  }

  if (haveRfRx && now - lastRfRxMs > 6000) {
    ++rfResyncs;
    AK_LOG_WARN("sync",
                "RESYNC t=%lu silence_ms=%lu ch=%u seq=%d rssi=%d",
                static_cast<unsigned long>(now),
                static_cast<unsigned long>(now - lastRfRxMs),
                static_cast<unsigned>(rfChannel), sequence, frame.rssiDbm);
  }

  {
    uint16_t gap = 0;
    if (lastRfSequence >= 0) {
      const int expected = (lastRfSequence + 1) & 0xFF;
      gap = static_cast<uint16_t>((sequence - expected + 256) & 0xFF);
    }
    lastRfSequence = sequence;
    rfGapSlots += gap;
    if (gap > rfMaxGap) {
      rfMaxGap = gap;
    }
    ++rfValidFrames;
    rfRssiSum += frame.rssiDbm;
    if (rfValidFrames == 1 || frame.rssiDbm < rfRssiMin) {
      rfRssiMin = frame.rssiDbm;
    }
    if (rfValidFrames == 1 || frame.rssiDbm > rfRssiMax) {
      rfRssiMax = frame.rssiDbm;
    }

    char detail[128];
    detail[0] = '\0';
    const DavisObservation& observation = decoded.observation;
    switch (decoded.messageType) {
      case DavisMessageType::Temperature:
        std::snprintf(detail, sizeof(detail), "temp=%.2fC",
                      static_cast<double>(observation.temperatureC));
        break;
      case DavisMessageType::Humidity:
        std::snprintf(detail, sizeof(detail), "hum=%.1f%%",
                      static_cast<double>(observation.humidityPct));
        break;
      case DavisMessageType::RainRate:
        std::snprintf(detail, sizeof(detail), "rain=%.2fmm/h",
                      static_cast<double>(observation.rainRateMmPerHour));
        break;
      case DavisMessageType::UvIndex:
        if (observation.has(DavisField::UvIndex)) {
          std::snprintf(detail, sizeof(detail), "uv=%.1f",
                        static_cast<double>(observation.uvIndex));
        } else {
          std::snprintf(detail, sizeof(detail), "uv=missing");
        }
        break;
      case DavisMessageType::SolarRadiationWm2:
        if (observation.has(DavisField::SolarRadiation)) {
          std::snprintf(detail, sizeof(detail), "solar=%.0fW/m2",
                        static_cast<double>(observation.solarRadiationWm2));
        } else {
          std::snprintf(detail, sizeof(detail), "solar=missing");
        }
        break;
      case DavisMessageType::SolarRadiation:
        std::snprintf(detail, sizeof(detail), "solar=%.0fW/m2",
                      static_cast<double>(observation.solarRadiationWm2));
        break;
      case DavisMessageType::GustSpeed:
        std::snprintf(detail, sizeof(detail), "gust=%.2fm/s",
                      static_cast<double>(observation.windGustMs));
        break;
      case DavisMessageType::GoldcapVoltage:
        std::snprintf(detail, sizeof(detail), "goldcap=%.2fV",
                      static_cast<double>(observation.batteryVoltageV));
        break;
      case DavisMessageType::RainCounter:
        std::snprintf(detail, sizeof(detail), "rainclicks=%u",
                      static_cast<unsigned>(frame.data[3] & 0x7Fu));
        break;
      case DavisMessageType::Unknown3:
      default:
        std::snprintf(detail, sizeof(detail), "unknown3");
        break;
    }

    AK_LOG_INFO("rx",
                "RX t=%lu ch=%u seq=%u gap=%u rssi=%d msg=%s tx=%u wind=%.2fm/s dir=%.0f "
                "battery_low=%u raw=%s %s",
                static_cast<unsigned long>(now), static_cast<unsigned>(rfChannel),
                static_cast<unsigned>(sequence), static_cast<unsigned>(gap), frame.rssiDbm,
                rfMessageName(decoded.messageType),
                static_cast<unsigned>(observation.transmitterId),
                static_cast<double>(observation.windSpeedMs),
                static_cast<double>(observation.windDirectionDeg), decoded.batteryLow ? 1u : 0u,
                raw, detail);
  }

  lastRfRxMs = now;
  haveRfRx = true;
  rfMissStreak = 0;
  rfSearchMode = false;
  rfHop(false, "rx");
}

void rfTestLoop() {
  pollRfSerial();
  RadioFrame frame;
  const Status status = radio.receive(frame, 50);
  const uint32_t now = millis();

  if (isOk(status)) {
    rfHandleFrame(frame, now);
  } else if (status == Status::Timeout) {
    const uint32_t interval = rfSearchMode ? config::kRadioSearchHopMs : config::kRadioHopMs;
    if (now - lastRfHopMs >= interval) {
      if (!rfSearchMode) {
        ++rfMissStreak;
        if (rfMissStreak >= config::kRadioSearchAfterMisses) {
          rfSearchMode = true;
          AK_LOG_WARN("sync",
                      "SEARCH t=%lu after %u missed slots; reverse scan from ch=%u",
                      static_cast<unsigned long>(now),
                      static_cast<unsigned>(rfMissStreak),
                      static_cast<unsigned>(rfChannel));
        }
      }
      rfHop(rfSearchMode, rfSearchMode ? "search" : "timeout");
    }
  }

  if (now - lastRfStatsMs >= 60000) {
    logRfStats(now);
  }
  delay(2);
}

void setupRfTest() {
  pinMode(config::kStatusLedPin, OUTPUT);
  digitalWrite(config::kStatusLedPin, LOW);

  RadioConfig radioConfig;
  radioConfig.chip = RadioChip::Rfm69;
  radioConfig.frequencyMhz = config::kRadioFrequencyMhz;
  radioConfig.modulation = RadioModulation::Fsk;
  radioConfig.bitrateKbps = config::kRadioBitrateKbps;
  radioConfig.deviationKhz = config::kRadioDeviationKhz;
  radioConfig.bandwidthKhz = config::kRadioBandwidthKhz;
  radioConfig.powerDbm = config::kRadioPowerDbm;
  radioConfig.highPower = true;
  radioConfig.channel = config::kRadioInitialChannel;
  radioConfig.fixedPayloadLength = kDavisFrameBytes;
  radioConfig.backgroundRx = false;
  const Status status = radio.begin(radioConfig);
  if (!isOk(status)) {
    AK_LOG_ERROR("boot", "RF bench mode: radio failed: %s", statusToString(status));
    return;
  }
  rfChannel = config::kRadioInitialChannel;
  lastRfHopMs = millis();
  lastRfStatsMs = lastRfHopMs;
  AK_LOG_INFO("boot",
              "RF bench mode: ch=%u freq=%.3fMHz slot=%lums hop=%lums search_after=%u "
              "search_hop=%lums",
              static_cast<unsigned>(rfChannel), davisChannelFrequencyMhz(rfChannel),
              static_cast<unsigned long>(config::kRadioSlotMs),
              static_cast<unsigned long>(config::kRadioHopMs),
              static_cast<unsigned>(config::kRadioSearchAfterMisses),
              static_cast<unsigned long>(config::kRadioSearchHopMs));
}

struct PendingArchive {
  char messageId[65] = {};
  char payload[2048] = {};
  int64_t bucketEndEpoch = 0;
  uint8_t attempts = 0;
  uint32_t nextAttemptMs = 0;
};

PendingArchive pending[config::kMaxPendingUploads];
size_t pendingCount = 0;

void buildArchiveMessageId(char* output, size_t size, int64_t bucketEndEpoch) {
  std::snprintf(output, size, "%s-%lld-archive", settings.deviceId,
                static_cast<long long>(bucketEndEpoch));
}

// Development-only identity and network defaults. On a blank device these seed
// the first boot so the bench board reaches the local server after a reflash.
// Once anything is saved through the AP page, NVS wins on later boots. The
// credentials themselves live in the git-ignored secrets.h.
void applyDevelopmentSettings(DeviceSettings& target) {
  // Keep the MAC-derived device ID that SettingsStore::applyDefaults() filled
  // in (`ak-xxxxxx`). It must stay unique per board; the station number is a
  // separate field and changing it must not change the device identity.
  std::snprintf(target.deviceToken, sizeof(target.deviceToken), "%s",
                config::kDeviceToken);
  std::snprintf(target.apiToken, sizeof(target.apiToken), "%s", config::kApiToken);
  std::snprintf(target.serverBaseUrl, sizeof(target.serverBaseUrl), "%s",
                config::kServerBaseUrl);
  target.commandPollIntervalSeconds = config::kCommandPollIntervalSeconds;
  target.clearWifiProfiles();
  if (config::kWifiSsid[0] != '\0') {
    target.addWifiProfile(config::kWifiSsid, config::kWifiPassword);
  }
}

// The ESP-IDF app descriptor does not carry firmware/VERSION on this core, so
// the sketch records which release it downloaded. BootWithNewFW only boots a
// slot whose recorded version matches the requested release.
bool rememberStagedVersion(const char* version) {
  if (version == nullptr || version[0] == '\0') {
    return false;
  }
  Preferences preferences;
  if (!preferences.begin("ota_meta", false)) {
    return false;
  }
  const size_t written = preferences.putString("staged_version", version);
  preferences.end();
  return written > 0;
}

size_t loadStagedVersion(char* output, size_t size) {
  if (output == nullptr || size == 0) {
    return 0;
  }
  output[0] = '\0';
  Preferences preferences;
  if (!preferences.begin("ota_meta", true)) {
    return 0;
  }
  const String value = preferences.getString("staged_version", "");
  preferences.end();
  std::strncpy(output, value.c_str(), size - 1);
  output[size - 1] = '\0';
  return std::strlen(output);
}

void clearStagedVersion() {
  Preferences preferences;
  if (preferences.begin("ota_meta", false)) {
    preferences.remove("staged_version");
    preferences.end();
  }
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

void enqueueArchive(const char* payload, const char* messageId, int64_t bucketEndEpoch) {
  if (payload == nullptr || payload[0] == '\0') {
    return;
  }
  if (std::strlen(payload) >= sizeof(pending[0].payload)) {
    AK_LOG_ERROR("archive", "serialized record too large (%u bytes)",
                 static_cast<unsigned>(std::strlen(payload)));
    return;
  }
  if (pendingCount == config::kMaxPendingUploads) {
    AK_LOG_WARN("upload", "queue full; dropping oldest archive record");
    shiftPending();
  }
  PendingArchive& slot = pending[pendingCount++];
  std::strncpy(slot.messageId, messageId, sizeof(slot.messageId) - 1);
  slot.messageId[sizeof(slot.messageId) - 1] = '\0';
  std::strncpy(slot.payload, payload, sizeof(slot.payload) - 1);
  slot.payload[sizeof(slot.payload) - 1] = '\0';
  slot.bucketEndEpoch = bucketEndEpoch;
  slot.attempts = 0;
  slot.nextAttemptMs = millis();
}

const char* resetReasonName() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON:
      return "poweron";
    case ESP_RST_EXT:
      return "external";
    case ESP_RST_SW:
      return "software";
    case ESP_RST_PANIC:
      return "panic";
    case ESP_RST_INT_WDT:
      return "int_wdt";
    case ESP_RST_TASK_WDT:
      return "task_wdt";
    case ESP_RST_WDT:
      return "wdt";
    case ESP_RST_DEEPSLEEP:
      return "deepsleep";
    case ESP_RST_BROWNOUT:
      return "brownout";
    case ESP_RST_SDIO:
      return "sdio";
    default:
      return "unknown";
  }
}

// Current radio RSSI: average of the valid frames in the bucket being filled,
// or the last valid frame when that bucket has no samples yet. Zero means no
// valid frame has been decoded since boot.
int16_t currentRadioRssiDbm() {
  const ArchiveRecord& bucket = archiveCollector.current();
  if (bucket.rssiCount > 0) {
    const double average = bucket.rssiSum / static_cast<double>(bucket.rssiCount);
    return static_cast<int16_t>(std::lround(average));
  }
  return haveValidRssi ? lastValidRssiDbm : 0;
}

DeviceHealth currentHealth() {
  DeviceHealth health;
  health.uptimeSeconds = millis() / 1000UL;
  health.queueDepth = static_cast<uint16_t>(pendingCount);
  health.freeHeap = ESP.getFreeHeap();
  health.wifiRssi = wifi.staConnected() ? static_cast<int16_t>(WiFi.RSSI()) : 0;
  health.radioFramesValid = archiveCollector.current().framesValid;
  health.radioRssiDbm = currentRadioRssiDbm();
  health.resetReason = bootResetReason;
  return health;
}

void markSent(int64_t bucketEndEpoch) {
  if (bucketEndEpoch <= lastSentEpoch) {
    return;
  }
  lastSentEpoch = bucketEndEpoch;
  if (archive) {
    archive->setLastSentEpoch(bucketEndEpoch);
  }
}

// Replays SD archive records newer than last_sent in batches, oldest first.
// Live uploads are held while a replay is active so last_sent cannot jump over
// older unsent records.
void serviceCatchUp() {
  if (!archiveReady || !archive || !settings.uploadEnabled ||
      !settings.cloudConfigured() || !wifi.staConnected() || !timeService.isValid()) {
    return;
  }
  if (pendingCount > 0 && !replayActive) {
    return;
  }
  const int64_t nowEpoch = timeService.nowEpoch();
  if (!replayActive && lastSentEpoch > 0 && nowEpoch - lastSentEpoch < 300) {
    return;  // newest bucket is already acknowledged
  }
  const uint32_t now = millis();
  if (replayActive && now - lastReplayAttemptMs < 1000) {
    return;
  }
  if (!replayActive) {
    if (now - lastReplayAttemptMs < 5000) {
      return;
    }
    lastReplayAttemptMs = now;
    const Status started = archive->beginReplay(lastSentEpoch, nowEpoch);
    if (!isOk(started)) {
      return;
    }
    AK_LOG_INFO("catchup", "replay start after=%lld now=%lld",
                static_cast<long long>(lastSentEpoch),
                static_cast<long long>(timeService.nowEpoch()));
    replayActive = true;
  }
  lastReplayAttemptMs = now;

  size_t catchupBytes = 0;
  const size_t count = buildReplayBatch(replayBatchLastEpoch, catchupBytes);
  if (count == 0) {
    AK_LOG_INFO("catchup", "replay end last_sent=%lld",
                static_cast<long long>(lastSentEpoch));
    archive->endReplay();
    replayActive = false;
    return;
  }

  ApiClient api(settings, lastRadioEpoch, currentHealth());
  const ArchiveBatchResult result =
      api.postArchiveBatch(reinterpret_cast<const uint8_t*>(catchupBody), catchupBytes);
  if (isOk(result.status) && result.rejected == 0) {
    markSent(replayBatchLastEpoch);
    AK_LOG_INFO("catchup", "batch=%u accepted=%d duplicate=%d last_sent=%lld",
                static_cast<unsigned>(count), result.accepted, result.duplicates,
                static_cast<long long>(lastSentEpoch));
    return;
  }
  AK_LOG_WARN("catchup", "batch failed status=%s rejected=%d err=%s; backing off",
              statusToString(result.status), result.rejected, result.error);
  archive->endReplay();
  replayActive = false;
  lastReplayAttemptMs = now;
}

// Builds one MessagePack batch (up to kCatchupBatchRecords) from the active
// replay. ArduinoJson provides a native MessagePack encoder, so no custom
// encoder is needed on the device.
size_t buildReplayBatch(int64_t& lastEpoch, size_t& byteLength) {
  lastEpoch = 0;
  byteLength = 0;
  JsonDocument batch;
  JsonArray array = batch.to<JsonArray>();
  size_t count = 0;
  while (count < kCatchupBatchRecords) {
    char line[kCatchupLineBytes];
    int64_t epoch = 0;
    bool done = false;
    if (!archive->nextReplayLine(line, sizeof(line), epoch, done)) {
      break;
    }
    JsonDocument record;
    if (deserializeJson(record, line) != DeserializationError::Ok) {
      continue;
    }
    array.add(record.as<JsonObjectConst>());
    ++count;
    lastEpoch = epoch;
  }
  if (count == 0) {
    return 0;
  }
  const size_t needed = measureMsgPack(batch);
  if (needed > sizeof(catchupBody)) {
    AK_LOG_ERROR("catchup", "batch too large: %u bytes", static_cast<unsigned>(needed));
    return 0;
  }
  byteLength = serializeMsgPack(batch, catchupBody, sizeof(catchupBody));
  return count;
}

// Synchronous bounded replay used by the get_data_again troubleshooting
// command: sends SD records in [start, end] and returns when done or failed.
bool runRangeReplay(int64_t startEpoch, int64_t endEpoch, uint32_t& sent,
                    char* error, size_t errorSize) {
  sent = 0;
  if (endEpoch <= startEpoch || endEpoch - startEpoch > 7 * 86400) {
    std::snprintf(error, errorSize, "range must be >0 and at most 7 days");
    return false;
  }
  if (!isOk(archive->beginReplayRange(startEpoch, endEpoch))) {
    std::snprintf(error, errorSize, "replay unavailable");
    return false;
  }
  const uint32_t started = millis();
  while (millis() - started < 240000UL) {
    int64_t lastEpoch = 0;
    size_t byteLength = 0;
    const size_t count = buildReplayBatch(lastEpoch, byteLength);
    if (count == 0) {
      archive->endReplay();
      return true;
    }
    ApiClient api(settings, lastRadioEpoch, currentHealth());
    const ArchiveBatchResult result =
        api.postArchiveBatch(reinterpret_cast<const uint8_t*>(catchupBody), byteLength);
    if (!isOk(result.status) || result.rejected > 0) {
      std::snprintf(error, errorSize, "batch failed: %s rejected=%d",
                    result.error, result.rejected);
      archive->endReplay();
      return false;
    }
    sent += static_cast<uint32_t>(count);
  }
  std::snprintf(error, errorSize, "replay timed out");
  archive->endReplay();
  return false;
}

void drainUploads() {
  if (replayActive) {
    return;
  }
  if (pendingCount == 0 || !settings.uploadEnabled || !settings.cloudConfigured() ||
      !wifi.staConnected() || !timeService.isValid()) {
    return;
  }
  if (static_cast<int32_t>(millis() - pending[0].nextAttemptMs) < 0) {
    return;
  }

  ApiClient api(settings, lastRadioEpoch, currentHealth());
  const ArchiveUpload result =
      api.postArchive(pending[0].payload, pending[0].messageId);
  if (isOk(result.status)) {
    AK_LOG_INFO("upload", "sent bucket=%lld duplicate=%u",
                static_cast<long long>(pending[0].bucketEndEpoch),
                result.duplicate ? 1u : 0u);
    markSent(pending[0].bucketEndEpoch);
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
  AK_LOG_ERROR("upload", "dropping archive record after failure: %s", result.error);
  shiftPending();
}

void finishArchiveRecord(const ArchiveRecord& record) {
  ArchiveRecord completed = record;
  completed.bootId = bootId;
  completed.uptimeMs = static_cast<uint32_t>(millis());

  char messageId[65];
  buildArchiveMessageId(messageId, sizeof(messageId), completed.bucketEndEpoch);

  String payload;
  if (!serializeArchiveJson(settings.deviceId, completed, messageId, payload)) {
    AK_LOG_ERROR("archive", "could not serialize bucket %lld",
                 static_cast<long long>(completed.bucketEndEpoch));
    return;
  }

  if (settings.archiveEnabled && archiveReady && archive) {
    const Status status = archive->append(payload.c_str(), payload.length());
    if (!isOk(status)) {
      AK_LOG_ERROR("archive", "append failed: %s", statusToString(status));
    }
  }

  AK_LOG_INFO("archive",
              "bucket=%lld frames=%lu/%lu bad=%lu rain_tips=%lu temp_n=%lu rssi=%d",
              static_cast<long long>(record.bucketEndEpoch),
              static_cast<unsigned long>(record.framesValid),
              static_cast<unsigned long>(record.framesExpected),
              static_cast<unsigned long>(record.framesCrcBad),
              static_cast<unsigned long>(record.rainTips),
              static_cast<unsigned long>(record.outTempCount),
              record.rssiCount > 0
                  ? static_cast<int>(record.rssiSum / record.rssiCount)
                  : 0);

  if (settings.uploadEnabled && settings.cloudConfigured()) {
    enqueueArchive(payload.c_str(), messageId, completed.bucketEndEpoch);
  }
}

void sampleBme280() {
  if (!bmeReady || millis() - lastBmeSampleMs < config::kBmeSampleIntervalMs) {
    return;
  }
  lastBmeSampleMs = millis();
  // In forced mode the BME280 sleeps after one conversion. readTemperature()
  // only reads the result registers, so without this the firmware re-reads the
  // single measurement taken at boot forever.
  if (!bme.takeForcedMeasurement()) {
    return;
  }
  const float temperature = bme.readTemperature();
  const float humidity = bme.readHumidity();
  const float pressure = bme.readPressure() / 100.0f;
  if (!std::isfinite(temperature) || !std::isfinite(humidity) ||
      !std::isfinite(pressure)) {
    return;
  }
  latestInTempC = temperature;
  latestInHumidityPct = humidity;
  latestPressureHpa = pressure;
  lastInTemperature.valid = true;
  lastInTemperature.value = temperature;
  lastInTemperature.updatedAtMs = lastBmeSampleMs;
  if (timeService.isValid() && radio.pendingFrames() == 0) {
    archiveCollector.addBme280(temperature, humidity, pressure, timeService.nowEpoch());
  }
}

bool isDavisFrameValid(const uint8_t* data, size_t length) {
  if (data == nullptr || length != kDavisFrameBytes) {
    return false;
  }
  const uint16_t expected =
      static_cast<uint16_t>((static_cast<uint16_t>(data[6]) << 8) | data[7]);
  return expected != 0 && davisCrc16(data, 6) == expected;
}

bool handleFrame(const RadioFrame& frame) {
  const DavisDecodeResult decoded = decodeDavisFrame(frame.data, frame.length,
                                                     frame.receivedAtMs);
  if (decoded.status != DavisDecodeStatus::Ok) {
    if (timeService.isValid()) {
      archiveCollector.noteBadFrames(1, frame.rssiDbm, frame.rssiDbm, frame.rssiDbm);
    }
    AK_LOG_WARN("rx", "RX_BAD len=%u status=%s rssi=%d raw=%02X %02X %02X %02X %02X %02X %02X %02X",
                static_cast<unsigned>(frame.length), toString(decoded.status), frame.rssiDbm,
                frame.data[0], frame.data[1], frame.data[2], frame.data[3], frame.data[4],
                frame.data[5], frame.data[6], frame.data[7]);
    return false;
  }

  const int64_t epoch = frame.receivedAtEpoch;
  lastValidRssiDbm = frame.rssiDbm;
  haveValidRssi = true;
  if (epoch < 946684800LL) {
    AK_LOG_WARN("rx", "decoded frame but receive time invalid; dropping");
    return true;
  }

  DavisObservation observation = decoded.observation;
  observation.observedAtEpoch = epoch;
  if (observation.sequence == 0) {
    observation.sequence = ++receivedSequence;
  }

  AK_LOG_INFO("rx", "seq=%lu temp=%.1fC hum=%.0f%% rssi=%ddBm ch=%u",
              static_cast<unsigned long>(observation.sequence), observation.temperatureC,
              observation.humidityPct, frame.rssiDbm,
              static_cast<unsigned>(frame.channel));

  lastObservation = observation;
  haveObservation = true;
  lastObservationAtMs = frame.receivedAtMs;
  if (observation.has(DavisField::Temperature) &&
      std::isfinite(observation.temperatureC)) {
    lastTemperature.valid = true;
    lastTemperature.value = observation.temperatureC;
    lastTemperature.updatedAtMs = frame.receivedAtMs;
  }
  if (observation.has(DavisField::WindSpeed) &&
      std::isfinite(observation.windSpeedMs)) {
    lastWindSpeed.valid = true;
    lastWindSpeed.value = observation.windSpeedMs;
    lastWindSpeed.updatedAtMs = frame.receivedAtMs;
  }
  if (observation.has(DavisField::RainRate) &&
      std::isfinite(observation.rainRateMmPerHour)) {
    lastRainRate.valid = true;
    lastRainRate.value = observation.rainRateMmPerHour;
    lastRainRate.updatedAtMs = frame.receivedAtMs;
  }
  lastRadioEpoch = epoch;
  if (rawLogReady) {
    rawLog.append(epoch, frame.data, frame.length, frame.rssiDbm, frame.channel);
  }
  archiveCollector.add(observation, frame.rssiDbm, decoded.batteryLow, epoch);
  return true;
}

void updateRadioStats() {
  const Rfm69Radio::RadioStats stats = radio.stats();
  if (stats.missedSlots > lastRadioMissedSlots) {
    uint32_t delta = stats.missedSlots - lastRadioMissedSlots;
    lastRadioMissedSlots = stats.missedSlots;
    while (delta-- > 0) {
      archiveCollector.noteGap(1);
    }
  }
  if (stats.resyncCount > lastRadioResyncCount) {
    uint32_t delta = stats.resyncCount - lastRadioResyncCount;
    lastRadioResyncCount = stats.resyncCount;
    while (delta-- > 0) {
      archiveCollector.noteResync();
    }
  }
  if (stats.searchCount > lastRadioSearchCount) {
    uint32_t delta = stats.searchCount - lastRadioSearchCount;
    lastRadioSearchCount = stats.searchCount;
    while (delta-- > 0) {
      archiveCollector.noteSearch();
    }
  }
  if (stats.badFrames > lastRadioBadFrames) {
    lastRadioBadFrames = stats.badFrames;
    if (lastBadLogMs == 0 || millis() - lastBadLogMs >= 60000UL) {
      lastBadLogMs = millis();
      AK_LOG_WARN("rx",
                  "RX_BAD total=%lu rssi_min=%d rssi_avg=%d rssi_max=%d",
                  static_cast<unsigned long>(stats.badFrames),
                  static_cast<int>(stats.badRssiMin),
                  static_cast<int>(stats.badRssiAvg),
                  static_cast<int>(stats.badRssiMax));
    }
  }
  const Rfm69Radio::BadStats bad = radio.takeBadStats();
  if (bad.count > 0) {
    archiveCollector.noteBadFrames(bad.count, bad.rssiMin, bad.rssiSum, bad.rssiMax);
  }
}

void serviceRadio() {
  RadioFrame frame;
  const Status status = radio.receive(frame, config::kRadioReceiveTimeoutMs);
  if (isOk(status)) {
    handleFrame(frame);
  }
}

// Uploads raw frames in [start, end] to the server for CSV export. Bounded to
// 20k frames per command so a week of 3 s frames cannot stall the device.
bool sendRawRange(int64_t startEpoch, int64_t endEpoch, uint32_t& total, bool& truncated,
                  char* error, size_t errorSize) {
  total = 0;
  truncated = false;
  if (endEpoch <= startEpoch || endEpoch - startEpoch > 7 * 86400) {
    std::snprintf(error, errorSize, "range must be >0 and at most 7 days");
    return false;
  }
  if (!rawLogReady || !isOk(rawLog.beginReplayRange(startEpoch, endEpoch))) {
    std::snprintf(error, errorSize, "raw log unavailable");
    return false;
  }
  JsonDocument doc;
  doc["start_epoch"] = startEpoch;
  doc["end_epoch"] = endEpoch;
  JsonArray records = doc["records"].to<JsonArray>();
  size_t inBatch = 0;
  RawLog::Record record;
  while (rawLog.nextReplayRecord(record)) {
    if (total >= 20000) {
      truncated = true;
      break;
    }
    JsonObject item = records.add<JsonObject>();
    item["t"] = record.epoch;
    char hex[3 * 8 + 1];
    formatHex(record.frame, record.length, hex, sizeof(hex));
    item["hex"] = hex;
    item["rssi"] = record.rssi;
    item["ch"] = record.channel;
    ++inBatch;
    ++total;
    if (inBatch >= 100) {
      String payload;
      serializeJson(doc, payload);
      ApiClient api(settings, lastRadioEpoch, currentHealth());
      if (!isOk(api.postRawBatch(payload.c_str()))) {
        std::snprintf(error, errorSize, "raw upload failed");
        rawLog.endReplay();
        return false;
      }
      doc.clear();
      doc["start_epoch"] = startEpoch;
      doc["end_epoch"] = endEpoch;
      records = doc["records"].to<JsonArray>();
      inBatch = 0;
    }
  }
  if (inBatch > 0) {
    String payload;
    serializeJson(doc, payload);
    ApiClient api(settings, lastRadioEpoch, currentHealth());
    if (!isOk(api.postRawBatch(payload.c_str()))) {
      std::snprintf(error, errorSize, "raw upload failed");
      rawLog.endReplay();
      return false;
    }
  }
  rawLog.endReplay();
  return true;
}

void reportConfigWhenOnline() {
  if (!settings.cloudConfigured() || !wifi.staConnected()) {
    return;
  }
  const bool changed = configReportPending || settings.configVersion != reportedConfigVersion;
  if (!changed && lastConfigReportMs != 0 &&
      millis() - lastConfigReportMs < config::kConfigReportIntervalMs) {
    return;
  }
  if (lastConfigAttemptMs != 0 && millis() - lastConfigAttemptMs < 60000UL) {
    return;
  }
  lastConfigAttemptMs = millis();
  ApiClient api(settings, lastRadioEpoch, currentHealth());
  if (isOk(api.putDeviceConfig())) {
    lastConfigReportMs = millis();
    reportedConfigVersion = settings.configVersion;
    configReportPending = false;
    AK_LOG_INFO("config", "reported v%u", static_cast<unsigned>(settings.configVersion));
  }
}

void pollCommands() {
  const uint32_t intervalMs =
      static_cast<uint32_t>(settings.commandPollIntervalSeconds) * 1000UL;
  if (!settings.cloudConfigured() || !wifi.staConnected()) {
    return;
  }
  if (millis() - lastCommandPollMs < intervalMs) {
    return;
  }
  lastCommandPollMs = millis();

  ApiClient api(settings, lastRadioEpoch, currentHealth());
  static CommandMessage commands[1];
  size_t count = 0;
  const Status status = api.leaseCommands(commands, 1, count);
  if (!isOk(status)) {
    AK_LOG_DEBUG("cmd", "lease failed: %s", statusToString(status));
    return;
  }

  for (size_t i = 0; i < count; ++i) {
    const protocol::CommandType type = protocol::commandTypeFromString(commands[i].type);
    bool ok = true;
    const char* error = nullptr;
    char errorBuffer[96] = {};
    bool reboot = false;

    JsonDocument payload;
    const DeserializationError payloadError = deserializeJson(payload, commands[i].payloadJson);
    if (payloadError != DeserializationError::Ok) {
      ok = false;
      error = "invalid command payload";
    }

    JsonDocument result;
    if (ok) {
      switch (type) {
        case protocol::CommandType::Reboot:
          reboot = true;
          result["rebooting"] = true;
          break;
        case protocol::CommandType::SyncTime:
          ok = isOk(timeService.syncFromNtp());
          if (!ok) {
            error = "sntp failed";
          } else {
            result["synced"] = true;
          }
          break;
        case protocol::CommandType::GetConfig:
          ok = isOk(api.putDeviceConfig());
          if (!ok) {
            error = "config report failed";
          } else {
            reportedConfigVersion = settings.configVersion;
            lastConfigReportMs = millis();
            configReportPending = false;
            result["reported"] = true;
          }
          break;
        case protocol::CommandType::DownloadNewFw:
          if (ota) {
            const char* version = payload["version"] | "";
            const char* url = payload["url"] | "";
            const char* sha256 = payload["sha256"] | "";
            const size_t sizeBytes = payload["size_bytes"] | static_cast<size_t>(0);
            String absoluteUrl(url);
            if (url[0] == '/') {
              absoluteUrl = String(settings.serverBaseUrl);
              while (absoluteUrl.endsWith("/")) {
                absoluteUrl.remove(absoluteUrl.length() - 1);
              }
              absoluteUrl += url;
            }
            StagedFirmwareInfo staged;
            ok = isOk(ota->stageImage(absoluteUrl.c_str(), sha256, sizeBytes, staged));
            if (ok && !rememberStagedVersion(version)) {
              ok = false;
              error = "could not persist staged version";
            }
            if (ok) {
              result["staged"] = true;
              result["version"] = String(version);
              result["partition"] = String(staged.partition);
              result["bytes"] = staged.bytes;
              result["sha256"] = String(staged.sha256);
            } else if (error == nullptr) {
              error = "download/stage failed";
            }
          } else {
            ok = false;
            error = "ota service unavailable";
          }
          break;
        case protocol::CommandType::BootWithNewFw:
          if (ota) {
            const char* version = payload["version"] | "";
            char stagedVersion[33] = {};
            loadStagedVersion(stagedVersion, sizeof(stagedVersion));
            if (stagedVersion[0] == '\0') {
              ok = false;
              error = "no staged image recorded";
            } else if (version[0] != '\0' && std::strcmp(version, stagedVersion) != 0) {
              ok = false;
              std::snprintf(errorBuffer, sizeof(errorBuffer), "staged %s, requested %s",
                            stagedVersion, version);
              error = errorBuffer;
            } else {
              StagedFirmwareInfo staged;
              ok = isOk(ota->bootStagedImage(version, staged));
              if (ok) {
                result["version"] = String(stagedVersion);
                result["partition"] = String(staged.partition);
                result["boot_next"] = true;
                clearStagedVersion();
                reboot = true;
              } else {
                error = "no valid staged image";
              }
            }
          } else {
            ok = false;
            error = "ota service unavailable";
          }
          break;
        case protocol::CommandType::CheckForUpdate:
          otaCheckRequested = true;
          result["check_scheduled"] = true;
          break;
        case protocol::CommandType::SelfTest:
          result["uptime_ms"] = millis();
          result["wifi_connected"] = wifi.staConnected();
          result["radio_ready"] = radioReady;
          result["archive_ready"] = archiveReady;
          result["time_valid"] = timeService.isValid();
          break;
        case protocol::CommandType::GetDataAgain: {
          const int64_t startEpoch = payload["start_epoch"] | static_cast<int64_t>(0);
          const int64_t endEpoch = payload["end_epoch"] | static_cast<int64_t>(0);
          uint32_t sent = 0;
          char replayError[96] = {};
          if (!runRangeReplay(startEpoch, endEpoch, sent, replayError,
                              sizeof(replayError))) {
            ok = false;
            error = replayError;
          } else {
            result["records"] = sent;
          }
          break;
        }
        case protocol::CommandType::GetPendingCommands:
          // Commands are executed synchronously, so nothing is left in flight.
          // A rebooted device legitimately reports an empty list and the server
          // requeues the timed-out command.
          result["pending"].to<JsonArray>();
          break;
        case protocol::CommandType::DownloadRawData: {
          const int64_t startEpoch = payload["start_epoch"] | static_cast<int64_t>(0);
          const int64_t endEpoch = payload["end_epoch"] | static_cast<int64_t>(0);
          uint32_t total = 0;
          bool truncated = false;
          char rawError[96] = {};
          if (!sendRawRange(startEpoch, endEpoch, total, truncated, rawError,
                            sizeof(rawError))) {
            ok = false;
            error = rawError;
          } else {
            result["records"] = total;
            result["truncated"] = truncated;
          }
          break;
        }
        case protocol::CommandType::PushConfig: {
          DeviceSettings updated = settings;
          bool networkChanged = false;
          if (!applyDeviceConfigJson(payload.as<JsonVariantConst>(), updated, networkChanged,
                                     errorBuffer, sizeof(errorBuffer))) {
            ok = false;
            error = errorBuffer;
            break;
          }
          updated.configVersion = settings.configVersion + 1;
          if (!isOk(settingsStore.save(updated))) {
            ok = false;
            error = "could not save config";
            break;
          }
          settings = updated;
          reportedConfigVersion = settings.configVersion;
          lastConfigReportMs = millis();
          configReportPending = false;
          result["applied"] = true;
          result["reboot_required"] = networkChanged;
          reboot = networkChanged;
          break;
        }
        case protocol::CommandType::SetConfig:
        case protocol::CommandType::UploadLogs:
        case protocol::CommandType::Unknown:
          ok = false;
          error = "not implemented";
          break;
      }
    }

    char resultJson[512] = {};
    if (ok) {
      serializeJson(result, resultJson, sizeof(resultJson));
    }
    api.acknowledgeCommand(commands[i].id, ok, error, ok ? resultJson : nullptr);
    AK_LOG_INFO("cmd", "%s -> %s", commands[i].type,
                ok ? (resultJson[0] != '\0' ? resultJson : "ok") : error);
    if (reboot) {
      delay(500);
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
  std::snprintf(state.line1, sizeof(state.line1), "%s %s",
                wifi.staConnected() ? "IP" : "AP",
                wifi.staConnected() ? wifi.status().staIp : wifi.status().apIp);
  std::snprintf(state.wifiSsid, sizeof(state.wifiSsid), "%s", wifi.status().staSsid);
  std::snprintf(state.apSsid, sizeof(state.apSsid), "%s", wifi.status().apSsid);
  std::snprintf(state.wifiIp, sizeof(state.wifiIp), "%s",
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
    webServer->setLastSentEpoch(lastSentEpoch);
    webServer->setRawFrames(rawLog.written());
    WeatherSnapshot weather;
    weather.available = haveObservation;
    weather.observedAtEpoch = lastObservation.observedAtEpoch;
    weather.observedAtMs = lastObservationAtMs;
    if (haveObservation) {
      weather.temperatureC = lastObservation.temperatureC;
      weather.humidityPct = lastObservation.humidityPct;
      weather.pressureHpa = std::isfinite(latestPressureHpa) ? latestPressureHpa
                                                             : lastObservation.pressureHpa;
      weather.inTemperatureC = latestInTempC;
      weather.inHumidityPct = latestInHumidityPct;
      weather.windSpeedMs = lastObservation.windSpeedMs;
      weather.windDirectionDeg = lastObservation.windDirectionDeg;
      weather.windGustMs = lastObservation.windGustMs;
      weather.rainRateMmH = lastObservation.rainRateMmPerHour;
    }
    weather.temperature = lastTemperature;
    weather.inTemperature = lastInTemperature;
    weather.windSpeed = lastWindSpeed;
    weather.rainRate = lastRainRate;
    const ArchiveRecord& bucket = archiveCollector.current();
    weather.framesExpected = archiveCollector.expectedFrames();
    weather.framesValid = bucket.framesValid;
    weather.framesInvalid = bucket.framesCrcBad;
    weather.rssiValid = bucket.rssiCount > 0 || haveValidRssi;
    weather.rssiDbm = currentRadioRssiDbm();
    weather.rssiFromBucket = bucket.rssiCount > 0;
    webServer->setWeather(weather);
  }
  digitalWrite(config::kStatusLedPin, wifi.staConnected() ? HIGH : LOW);
}

void logBanner() {
  AK_LOG_INFO("boot", "%s %s | protocol v%d", AKASHAVANI_PRODUCT_NAME, AKASHAVANI_FW_VERSION,
              AKASHAVANI_PROTOCOL_VERSION);
  AK_LOG_INFO("boot", "boot_id=%016llx reset=%s",
              static_cast<unsigned long long>(bootId), bootResetReason);
}

}  // namespace

void setup() {
  Log::begin(config::kSerialBaud);
  bootResetReason = resetReasonName();
  bootId = (static_cast<uint64_t>(esp_random()) << 32) | esp_random();
  // Hold the first line so an operator who powers the board in the field can
  // start minicom/`make fw-monitor` and still catch the boot banner. The ROM
  // bootloader text before this is not delayable.
  delay(config::kSerialStartupDelayMs);
  Log::setLevel(LogLevel::Info);
  delay(100);
  logBanner();

  if (config::kRfTestMode) {
    setupRfTest();
    return;
  }

  pinMode(config::kStatusLedPin, OUTPUT);
  digitalWrite(config::kStatusLedPin, LOW);

  if (!isOk(settingsStore.begin())) {
    AK_LOG_ERROR("boot", "settings store unavailable");
  }
  const Status settingsStatus = settingsStore.load(settings);
  if (!isOk(settingsStatus)) {
    AK_LOG_ERROR("boot", "settings load failed: %s", statusToString(settingsStatus));
  }
  if (!settingsStore.configured()) {
    applyDevelopmentSettings(settings);
  }
  AK_LOG_INFO("boot", "device=%s server=%s", settings.deviceId, settings.serverBaseUrl);

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

  bmeReady = bme.begin(0x76, &Wire);
  if (bmeReady) {
    bme.setSampling(Adafruit_BME280::MODE_FORCED,
                    Adafruit_BME280::SAMPLING_X1,
                    Adafruit_BME280::SAMPLING_X1,
                    Adafruit_BME280::SAMPLING_X1,
                    Adafruit_BME280::FILTER_OFF);
    AK_LOG_INFO("bme280", "BME280 ready at 0x76");
  } else {
    AK_LOG_WARN("bme280", "BME280 not found at 0x76");
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
    if (archiveReady) {
      lastSentEpoch = archive->lastSentEpoch();
      AK_LOG_INFO("archive", "last_sent=%lld", static_cast<long long>(lastSentEpoch));
      if (timeService.isValid()) {
        const int64_t cutoff =
            timeService.nowEpoch() - static_cast<int64_t>(config::kArchiveRetentionDays) * 86400;
        archive->pruneBefore(cutoff);
        rawLogReady = isOk(rawLog.begin(timeService.nowEpoch()));
        if (rawLogReady) {
          AK_LOG_INFO("raw", "rolling weekday log ready");
        }
      }
    }
  }

  RadioConfig radioConfig;
  radioConfig.chip = RadioChip::Rfm69;
  radioConfig.frequencyMhz = config::kRadioFrequencyMhz;
  radioConfig.modulation = RadioModulation::Fsk;
  radioConfig.bitrateKbps = config::kRadioBitrateKbps;
  radioConfig.deviationKhz = config::kRadioDeviationKhz;
  radioConfig.bandwidthKhz = config::kRadioBandwidthKhz;
  radioConfig.powerDbm = config::kRadioPowerDbm;
  radioConfig.highPower = true;
  radioConfig.channel = config::kRadioInitialChannel;
  radioConfig.fixedPayloadLength = kDavisFrameBytes;
  radioConfig.hopIntervalMs = config::kRadioHopMs;
  radioConfig.searchHopIntervalMs = config::kRadioSearchHopMs;
  radioConfig.searchAfterMisses = config::kRadioSearchAfterMisses;
  radioConfig.frameValidator = isDavisFrameValid;
  const Status radioStatus = radio.begin(radioConfig);
  radioReady = isOk(radioStatus);
  if (!radioReady) {
    AK_LOG_WARN("boot", "radio driver not active (%s); port prototype driver",
                statusToString(radioStatus));
  } else {
    archiveCollector.reset();
    AK_LOG_INFO("boot", "production RF: ch=%u freq=%.3fMHz",
                static_cast<unsigned>(radio.channel()),
                davisChannelFrequencyMhz(radio.channel()));
  }

  WifiApConfig apConfig;
  apConfig.ssidPrefix = config::kApSsidPrefix;
  apConfig.password = config::kApPassword;
  apConfig.channel = config::kApChannel;
  apConfig.maxClients = config::kApMaxClients;
  apConfig.ip = config::kApIp;
  apConfig.gateway = config::kApGateway;
  apConfig.subnet = config::kApSubnet;
  apConfig.dhcpFirst = config::kApDhcpFirst;
  apConfig.dhcpLast = config::kApDhcpLast;
  wifi.begin(settings, apConfig);
  uint8_t mac[6] = {};
  WiFi.macAddress(mac);
  std::snprintf(settings.deviceMac, sizeof(settings.deviceMac),
                "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3],
                mac[4], mac[5]);
  AK_LOG_INFO("boot", "wifi mac=%s", settings.deviceMac);

  webServer = std::make_unique<WebConfigServer>(settings, settingsStore, wifi, timeService);
  webServer->setSubsystemStatus(radioReady, archiveReady);
  webServer->begin();

  if (config::kEnableLanOta) {
    ota = std::make_unique<OtaService>(settings);
    ota->begin();
  }

  AK_LOG_INFO("boot", "ready: AP=%s device=%s", wifi.status().apSsid, settings.deviceId);
}

void loop() {
  if (config::kRfTestMode) {
    rfTestLoop();
    return;
  }

  wifi.loop();
  if (webServer) {
    webServer->loop();
  }
  if (ota) {
    ota->loop();
  }
  display->loop();

  if (radioReady) {
    serviceRadio();
    updateRadioStats();
  }
  sampleBme280();

  timeReady = timeService.isValid();
  syncTimeWhenOnline();
  reportConfigWhenOnline();

  if (timeService.isValid() && radio.pendingFrames() == 0) {
    ArchiveRecord completed;
    while (archiveCollector.poll(timeService.nowEpoch(), completed)) {
      finishArchiveRecord(completed);
    }
  }

  if (rawLogReady) {
    rawLog.loop(millis());
  }
  serviceCatchUp();
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
}  // namespace akashavani
