#pragma once

#include <cstdint>

// Transmitter simulator pin map and scenario tuning.
//
// This board carries the RFM95 and an ESP32-S3 only. SD, RTC, display and the
// cloud upload path are intentionally absent, matching the "test transmitter"
// role. Verify pins against the assembled board.
namespace config {

constexpr unsigned long kSerialBaud = 115200;

constexpr uint8_t kSpiSckPin = 12;
constexpr uint8_t kSpiMisoPin = 13;
constexpr uint8_t kSpiMosiPin = 11;

constexpr uint8_t kRadioCsPin = 10;
constexpr uint8_t kRadioResetPin = 16;
constexpr uint8_t kRadioDio0Pin = 15;
constexpr float kRadioFrequencyMhz = 915.0f;
constexpr float kRadioBitrateKbps = 4.8f;
constexpr float kRadioDeviationKhz = 5.0f;
constexpr float kRadioBandwidthKhz = 125.0f;
constexpr int8_t kRadioPowerDbm = 17;

constexpr uint8_t kStatusLedPin = 2;
constexpr uint8_t kTransmitterId = 1;
constexpr uint32_t kSimulatorSeed = 20261025;
constexpr uint32_t kTransmitIntervalMs = 5000;
constexpr bool kDryRun = false;  // log observations even when encoding is not implemented

}  // namespace config
