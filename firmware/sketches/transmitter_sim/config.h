#pragma once

#include <cstdint>

#if __has_include("secrets.h")
#include "secrets.h"
#else
#include "secrets.example.h"
#endif

// Transmitter simulator pin map and scenario tuning.
//
// The bench transmitter carries an RFM69. The scaffold assumes an ESP32-S3;
// the current bench boards are ESP32 classic. SD, RTC, display and the
// cloud upload path are intentionally absent, matching the "test transmitter"
// role. Verify pins against the assembled board.
namespace config {

constexpr unsigned long kSerialBaud = 115200;
// Match the receiver: give a laptop time to attach a console after power-up.
constexpr uint32_t kSerialStartupDelayMs = 3000;

constexpr uint8_t kSpiSckPin = 18;
constexpr uint8_t kSpiMisoPin = 19;
constexpr uint8_t kSpiMosiPin = 23;

constexpr uint8_t kRadioCsPin = 5;
constexpr uint8_t kRadioResetPin = 17;  // not connected on USB1; harmless
constexpr uint8_t kRadioDio0Pin = 13;
constexpr float kRadioFrequencyMhz = 915.0f;
constexpr float kRadioBitrateKbps = 19.2f;
constexpr float kRadioDeviationKhz = 4.8f;
constexpr float kRadioBandwidthKhz = 25.0f;
constexpr int8_t kRadioPowerDbm = 0;

constexpr uint8_t kStatusLedPin = 2;

// Status OLED and site Wi-Fi. The simulator has no sensors, archive or cloud
// path; it only joins the site network so the panel can show the live SSID.
constexpr uint8_t kI2cSdaPin = 21;
constexpr uint8_t kI2cSclPin = 22;
constexpr uint8_t kDisplayAddress = 0x3C;
constexpr bool kEnableDisplay = true;
constexpr bool kEnableWifi = true;
constexpr const char* kWifiSsid = AK_DEV_WIFI_SSID;
constexpr const char* kWifiPassword = AK_DEV_WIFI_PASSWORD;
constexpr uint32_t kWifiRetryMs = 15000;
constexpr uint32_t kDisplayIntervalMs = 2000;

constexpr uint8_t kTransmitterId = 1;
constexpr uint32_t kSimulatorSeed = 20261025;

// The transmitter sends one eight-byte Davis frame every 3 s, matching the ISS.
constexpr uint32_t kTransmitIntervalMs = 3000;
constexpr uint8_t kSkipProbabilityPercent = 12;
constexpr uint32_t kRestartOneMs = 3UL * 60UL * 1000UL;
constexpr uint32_t kRestartTwoMs = 8UL * 60UL * 1000UL;
constexpr bool kDryRun = false;

}  // namespace config
