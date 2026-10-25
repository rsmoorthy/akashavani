#include "app.h"

#include <Arduino.h>
#include <SPI.h>

#include <cstdio>

#include "AkashvaniCore.h"
#include "AkashvaniDavis.h"
#include "AkashvaniRadio.h"
#include "Rfm95Radio.h"
#include "config.h"

namespace akashvani {
namespace transmitter_sim_app {
namespace {

Rfm95Radio radio(SPI, RadioPins{config::kRadioCsPin, config::kRadioResetPin,
                                config::kRadioDio0Pin, config::kSpiSckPin,
                                config::kSpiMisoPin, config::kSpiMosiPin});
DavisSimulator simulator(config::kSimulatorSeed, config::kTransmitterId);
bool radioReady = false;
uint32_t lastTransmitMs = 0;
uint32_t transmitCount = 0;
uint32_t failureCount = 0;

void logObservation(const DavisObservation& observation) {
  AK_LOG_INFO("sim", "#%lu id=%u T=%.1fC H=%.0f%% P=%.1f hPa wind=%.1f m/s @ %.0f deg "
                     "rain=%.1f mm uv=%.1f solar=%.0f W/m2 batt=%.2fV",
              static_cast<unsigned long>(observation.sequence), observation.transmitterId,
              observation.temperatureC, observation.humidityPct, observation.pressureHpa,
              observation.windSpeedMs, observation.windDirectionDeg, observation.rainDayMm,
              observation.uvIndex, observation.solarRadiationWm2,
              observation.batteryVoltageV);
}

void transmitOnce() {
  DavisObservation observation = simulator.next();
  observation.observedAtEpoch = 0;  // No RTC on the simulator; receiver stamps arrival.
  logObservation(observation);

  uint8_t frame[64];
  size_t written = 0;
  const DavisEncodeStatus status =
      encodeDavisFrame(observation, frame, sizeof(frame), written);
  if (status != DavisEncodeStatus::Ok) {
    ++failureCount;
    if (config::kDryRun || failureCount == 1 || failureCount % 12 == 0) {
      AK_LOG_WARN("sim", "encoder not ported (%s); transmitted=%lu failed=%lu",
                  toString(status), static_cast<unsigned long>(transmitCount),
                  static_cast<unsigned long>(failureCount));
    }
    digitalWrite(config::kStatusLedPin, LOW);
    return;
  }

  const Status radioStatus = radio.transmit(frame, written);
  if (!isOk(radioStatus)) {
    ++failureCount;
    AK_LOG_ERROR("sim", "radio transmit failed: %s", statusToString(radioStatus));
    digitalWrite(config::kStatusLedPin, LOW);
    return;
  }
  ++transmitCount;
  digitalWrite(config::kStatusLedPin, HIGH);
  delay(50);
  digitalWrite(config::kStatusLedPin, LOW);
}

}  // namespace

void setup() {
  Log::begin(config::kSerialBaud);
  Log::setLevel(LogLevel::Info);
  delay(100);
  AK_LOG_INFO("boot", "%s simulator %s", AKASHVANI_PRODUCT_NAME, AKASHVANI_FW_VERSION);

  pinMode(config::kStatusLedPin, OUTPUT);
  digitalWrite(config::kStatusLedPin, LOW);

  RadioConfig radioConfig;
  radioConfig.chip = RadioChip::Rfm95;
  radioConfig.frequencyMhz = config::kRadioFrequencyMhz;
  radioConfig.modulation = RadioModulation::Fsk;
  radioConfig.bitrateKbps = config::kRadioBitrateKbps;
  radioConfig.deviationKhz = config::kRadioDeviationKhz;
  radioConfig.bandwidthKhz = config::kRadioBandwidthKhz;
  radioConfig.powerDbm = config::kRadioPowerDbm;
  const Status status = radio.begin(radioConfig);
  radioReady = isOk(status);
  if (!radioReady) {
    AK_LOG_WARN("boot", "radio not active (%s); simulator runs in observation-only mode",
                statusToString(status));
  }
  AK_LOG_INFO("boot", "interval=%lums id=%u", static_cast<unsigned long>(
                                               config::kTransmitIntervalMs),
              config::kTransmitterId);
}

void loop() {
  if (millis() - lastTransmitMs < config::kTransmitIntervalMs) {
    delay(20);
    return;
  }
  lastTransmitMs = millis();
  transmitOnce();
}

}  // namespace transmitter_sim_app
}  // namespace akashvani
