#include "app.h"

#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>
#include <Wire.h>

#include <cstdio>
#include <cstring>

#include "AkashvaniCore.h"
#include "AkashvaniDavis.h"
#include "AkashvaniDisplay.h"
#include "AkashvaniRadio.h"
#include "DavisChannels.h"
#include "config.h"

namespace akashvani {
namespace transmitter_sim_app {
namespace {

enum class Scenario : uint8_t {
  Idle,
  Continuous,
  RandomSkips,
  Restarts,
};

Rfm69Radio radio(SPI, RadioPins{config::kRadioCsPin, config::kRadioResetPin,
                                config::kRadioDio0Pin, config::kSpiSckPin,
                                config::kSpiMisoPin, config::kSpiMosiPin});
DavisSimulator simulator(config::kSimulatorSeed, config::kTransmitterId);

Ssd1306Config ssd1306Config() {
  Ssd1306Config value;
  value.address = config::kDisplayAddress;
  value.sdaPin = config::kI2cSdaPin;
  value.sclPin = config::kI2cSclPin;
  return value;
}

Ssd1306Display display(Wire, ssd1306Config());
bool displayReady = false;
bool wifiStarted = false;
bool wifiWasConnected = false;
uint32_t lastWifiAttemptMs = 0;
uint32_t lastDisplayMs = 0;
DisplayState displayState;

Scenario scenario = Scenario::Idle;
bool running = false;
bool radioReady = false;
uint32_t runStartedMs = 0;
uint32_t nextSlotMs = 0;
uint32_t slotIndex = 0;
uint8_t txChannel = 0;
uint8_t txSequence = 0;
uint8_t skipRemaining = 0;
uint8_t skipCooldown = 0;
uint32_t rngState = config::kSimulatorSeed ^ 0x9E3779B9u;
uint32_t txCount = 0;
uint32_t skipCount = 0;
uint32_t failureCount = 0;
bool restartOneDone = false;
bool restartTwoDone = false;
uint32_t lastTxStatsMs = 0;

char commandBuffer[32];
size_t commandLength = 0;

const char* scenarioName(Scenario value) {
  switch (value) {
    case Scenario::Idle:
      return "idle";
    case Scenario::Continuous:
      return "continuous";
    case Scenario::RandomSkips:
      return "random-skips";
    case Scenario::Restarts:
      return "restarts";
  }
  return "unknown";
}

uint32_t nextRandom() {
  uint32_t x = rngState;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  rngState = x;
  return x;
}

void formatHex(const uint8_t* data, size_t length, char* output, size_t outputSize) {
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

DavisMessageType messageForSlot(uint32_t index) {
  static constexpr DavisMessageType kOrder[] = {
      DavisMessageType::Temperature,     DavisMessageType::Humidity,
      DavisMessageType::RainRate,        DavisMessageType::SolarRadiationWm2,
      DavisMessageType::UvIndex,         DavisMessageType::GustSpeed,
      DavisMessageType::GoldcapVoltage,  DavisMessageType::RainCounter,
  };
  return kOrder[index % (sizeof(kOrder) / sizeof(kOrder[0]))];
}

void logStatus() {
  AK_LOG_INFO("sim",
              "STATUS scenario=%s running=%u ch=%u seq=%u slot=%lu tx=%lu skip=%lu fail=%lu",
              scenarioName(scenario), running ? 1u : 0u, static_cast<unsigned>(txChannel),
              static_cast<unsigned>(txSequence), static_cast<unsigned long>(slotIndex),
              static_cast<unsigned long>(txCount), static_cast<unsigned long>(skipCount),
              static_cast<unsigned long>(failureCount));
}

void logTxStats(uint32_t now) {
  AK_LOG_INFO("stats",
              "TX_STATS t=%lu scenario=%s running=%u slots=%lu tx=%lu skip=%lu fail=%lu "
              "ch=%u seq=%u",
              static_cast<unsigned long>(now), scenarioName(scenario), running ? 1u : 0u,
              static_cast<unsigned long>(slotIndex), static_cast<unsigned long>(txCount),
              static_cast<unsigned long>(skipCount),
              static_cast<unsigned long>(failureCount), static_cast<unsigned>(txChannel),
              static_cast<unsigned>(txSequence));
  lastTxStatsMs = now;
}

void startRun(Scenario selected) {
  scenario = selected;
  running = true;
  runStartedMs = millis();
  nextSlotMs = runStartedMs;
  slotIndex = 0;
  txChannel = 0;
  txSequence = 0;
  skipRemaining = 0;
  skipCooldown = 0;
  restartOneDone = false;
  restartTwoDone = false;
  txCount = 0;
  skipCount = 0;
  failureCount = 0;
  rngState = config::kSimulatorSeed ^ 0x9E3779B9u;
  simulator.reset(config::kSimulatorSeed);
  lastTxStatsMs = runStartedMs;
  if (radioReady) {
    const Status status = radio.setChannel(txChannel);
    if (!isOk(status)) {
      AK_LOG_ERROR("sim", "setChannel(0) failed: %s", statusToString(status));
    }
  }
  AK_LOG_INFO("sim",
              "RUN scenario=%s interval=%lums id=%u power=%ddBm",
              scenarioName(scenario), static_cast<unsigned long>(config::kTransmitIntervalMs),
              static_cast<unsigned>(config::kTransmitterId),
              static_cast<int>(config::kRadioPowerDbm));
}

void stopRun(const char* reason) {
  if (!running) {
    logStatus();
    return;
  }
  running = false;
  AK_LOG_INFO("sim", "STOP reason=%s", reason);
  logStatus();
  scenario = Scenario::Idle;
}

void restartTransmitter() {
  txChannel = 1;
  txSequence = 0;
  skipRemaining = 0;
  nextSlotMs = millis() + config::kTransmitIntervalMs;
  if (radioReady) {
    const Status status = radio.setChannel(txChannel);
    if (!isOk(status)) {
      AK_LOG_ERROR("sim", "restart setChannel(1) failed: %s", statusToString(status));
    }
  }
  AK_LOG_WARN("sim", "RESTART t=%lu next_ch=1 next_seq=0 after=%lums",
              static_cast<unsigned long>(millis()),
              static_cast<unsigned long>(millis() - runStartedMs));
}

void transmitSlot(uint8_t channel, uint8_t sequence) {
  if (!radioReady) {
    ++failureCount;
    return;
  }
  const Status channelStatus = radio.setChannel(channel);
  if (!isOk(channelStatus)) {
    ++failureCount;
    AK_LOG_ERROR("sim", "setChannel(%u) failed: %s", static_cast<unsigned>(channel),
                 statusToString(channelStatus));
    return;
  }

  DavisObservation observation = simulator.next();
  observation.transmitterId = config::kTransmitterId;
  observation.channel = channel;
  observation.sequence = sequence;
  const DavisMessageType message = messageForSlot(slotIndex);

  uint8_t frame[kDavisFrameBytes] = {};
  size_t written = 0;
  const DavisEncodeStatus encoded = encodeDavisFrame(
      observation, message, frame, sizeof(frame), written, sequence);
  if (encoded != DavisEncodeStatus::Ok) {
    ++failureCount;
    AK_LOG_ERROR("sim", "encode failed: %s", toString(encoded));
    return;
  }

  const uint32_t before = millis();
  const Status radioStatus = radio.transmit(frame, written);
  const uint32_t elapsed = millis() - before;
  if (!isOk(radioStatus)) {
    ++failureCount;
    AK_LOG_ERROR("sim", "transmit failed: %s", statusToString(radioStatus));
    return;
  }
  ++txCount;

  char raw[3 * kDavisFrameBytes + 1];
  formatHex(frame, written, raw, sizeof(raw));
  AK_LOG_INFO("sim",
              "TX t=%lu ch=%u seq=%u slot=%lu msg=%s raw=%s tx_ms=%lu",
              static_cast<unsigned long>(before), static_cast<unsigned>(channel),
              static_cast<unsigned>(sequence), static_cast<unsigned long>(slotIndex),
              toString(message), raw, static_cast<unsigned long>(elapsed));
  digitalWrite(config::kStatusLedPin, HIGH);
}

void slotTick() {
  if (!running) {
    return;
  }
  const uint32_t now = millis();
  if (static_cast<int32_t>(now - nextSlotMs) < 0) {
    return;
  }
  // If logging or a serial stall made us miss a slot, do not burst to catch up.
  if (now - nextSlotMs > config::kTransmitIntervalMs) {
    nextSlotMs = now;
  }

  if (skipCooldown > 0) {
    --skipCooldown;
  }

  const uint8_t slotChannel = txChannel;
  const uint8_t slotSequence = txSequence;
  const bool skip = scenario == Scenario::RandomSkips && skipRemaining > 0;

  if (skip) {
    --skipRemaining;
    ++skipCount;
    AK_LOG_INFO("sim", "SKIP t=%lu ch=%u seq=%u slot=%lu skip_left=%u",
                static_cast<unsigned long>(now), static_cast<unsigned>(slotChannel),
                static_cast<unsigned>(slotSequence), static_cast<unsigned long>(slotIndex),
                static_cast<unsigned>(skipRemaining));
    digitalWrite(config::kStatusLedPin, LOW);
  } else {
    transmitSlot(slotChannel, slotSequence);
  }

  txChannel = nextDavisChannel(txChannel);
  ++txSequence;
  ++slotIndex;
  nextSlotMs += config::kTransmitIntervalMs;

  if (scenario == Scenario::RandomSkips && !skip && skipRemaining == 0 &&
      skipCooldown == 0) {
    if ((nextRandom() % 100u) < config::kSkipProbabilityPercent) {
      skipRemaining = static_cast<uint8_t>(1 + (nextRandom() % 5u));
      skipCooldown = 8;
      AK_LOG_INFO("sim", "SKIP_PLAN next_skips=%u cooldown=%u",
                  static_cast<unsigned>(skipRemaining),
                  static_cast<unsigned>(skipCooldown));
    }
  }
}

void handleSerialCommand(const char* command) {
  if (command[0] == '\0') {
    return;
  }
  if (std::strcmp(command, "R1") == 0) {
    startRun(Scenario::Continuous);
  } else if (std::strcmp(command, "R2") == 0) {
    startRun(Scenario::RandomSkips);
  } else if (std::strcmp(command, "R3") == 0) {
    startRun(Scenario::Restarts);
  } else if (std::strcmp(command, "S") == 0) {
    stopRun("serial");
  } else if (std::strcmp(command, "X") == 0) {
    restartTransmitter();
  } else if (std::strcmp(command, "?") == 0) {
    logStatus();
  } else {
    AK_LOG_WARN("sim", "unknown command '%s' (use R1, R2, R3, S, X, ?)", command);
  }
}

void pollSerial() {
  while (Serial.available() > 0) {
    const char c = static_cast<char>(Serial.read());
    if (c == '\r' || c == '\n') {
      commandBuffer[commandLength] = '\0';
      handleSerialCommand(commandBuffer);
      commandLength = 0;
    } else if (commandLength + 1 < sizeof(commandBuffer)) {
      commandBuffer[commandLength++] = c;
    }
  }
}

void serviceAutoRestarts() {
  if (!running || scenario != Scenario::Restarts) {
    return;
  }
  const uint32_t elapsed = millis() - runStartedMs;
  if (!restartOneDone && elapsed >= config::kRestartOneMs) {
    restartOneDone = true;
    restartTransmitter();
  }
  if (!restartTwoDone && elapsed >= config::kRestartTwoMs) {
    restartTwoDone = true;
    restartTransmitter();
  }
}

// The simulator only joins the site network so the panel can show the live
// SSID. There is no cloud path, so connection loss is harmless.
void serviceWifi() {
  if (!config::kEnableWifi || config::kWifiSsid[0] == '\0') {
    return;
  }
  const uint32_t now = millis();
  if (WiFi.status() == WL_CONNECTED) {
    if (!wifiWasConnected) {
      wifiWasConnected = true;
      AK_LOG_INFO("wifi", "connected ssid=%s ip=%s rssi=%d dBm",
                  WiFi.SSID().c_str(), WiFi.localIP().toString().c_str(),
                  static_cast<int>(WiFi.RSSI()));
    }
    return;
  }
  if (wifiWasConnected) {
    wifiWasConnected = false;
    AK_LOG_WARN("wifi", "disconnected; retrying every %lums",
                static_cast<unsigned long>(config::kWifiRetryMs));
  }
  if (wifiStarted && now - lastWifiAttemptMs < config::kWifiRetryMs) {
    return;
  }
  lastWifiAttemptMs = now;
  if (!wifiStarted) {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    wifiStarted = true;
  } else {
    WiFi.disconnect();
  }
  WiFi.begin(config::kWifiSsid, config::kWifiPassword);
  AK_LOG_INFO("wifi", "connecting ssid=%s", config::kWifiSsid);
}

void updateDisplay() {
  if (!displayReady) {
    return;
  }
  const uint32_t now = millis();
  if (now - lastDisplayMs < config::kDisplayIntervalMs) {
    return;
  }
  lastDisplayMs = now;

  const bool connected = WiFi.status() == WL_CONNECTED;
  std::snprintf(displayState.wifiSsid, sizeof(displayState.wifiSsid), "%s",
                connected ? WiFi.SSID().c_str() : "");
  std::snprintf(displayState.wifiIp, sizeof(displayState.wifiIp), "%s",
                connected ? WiFi.localIP().toString().c_str() : "");
  displayState.wifiConnected = connected;
  displayState.rssiDbm = connected ? static_cast<int16_t>(WiFi.RSSI()) : 0;
  displayState.radioReady = radioReady;
  displayState.timeValid = false;
  displayState.archiveReady = false;

  std::snprintf(displayState.line1, sizeof(displayState.line1), "sim %s",
                scenarioName(scenario));
  std::snprintf(displayState.line2, sizeof(displayState.line2), "tx %lu skip %lu",
                static_cast<unsigned long>(txCount),
                static_cast<unsigned long>(skipCount));
  std::snprintf(displayState.line3, sizeof(displayState.line3), "ch %u seq %u",
                static_cast<unsigned>(txChannel), static_cast<unsigned>(txSequence));
  display.render(displayState);
}

}  // namespace

void setup() {
  Log::begin(config::kSerialBaud);
  Log::setLevel(LogLevel::Info);
  // Give a laptop time to attach a console after power-up (see the receiver).
  delay(config::kSerialStartupDelayMs);
  AK_LOG_INFO("boot", "%s simulator %s", AKASHVANI_PRODUCT_NAME, AKASHVANI_FW_VERSION);

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
  radioConfig.channel = 0;
  radioConfig.fixedPayloadLength = kDavisFrameBytes;
  const Status status = radio.begin(radioConfig);
  radioReady = isOk(status);
  if (!radioReady) {
    AK_LOG_WARN("boot", "radio not active (%s); simulator runs in observation-only mode",
                statusToString(status));
  }
  AK_LOG_INFO("boot",
              "READY interval=%lums id=%u power=%ddBm; commands R1/R2/R3, S, X, ?",
              static_cast<unsigned long>(config::kTransmitIntervalMs),
              static_cast<unsigned>(config::kTransmitterId),
              static_cast<int>(config::kRadioPowerDbm));

  if (config::kEnableDisplay) {
    displayReady = isOk(display.begin());
  }
  serviceWifi();
}

void loop() {
  pollSerial();
  serviceAutoRestarts();
  slotTick();
  serviceWifi();
  updateDisplay();
  const uint32_t now = millis();
  if (now - lastTxStatsMs >= 60000) {
    logTxStats(now);
  }
  delay(2);
}

}  // namespace transmitter_sim_app
}  // namespace akashvani
