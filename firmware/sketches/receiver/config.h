#pragma once

#include <cstdint>

// Receiver pin map and runtime tuning.
//
// These values are placeholders for the ESP32-S3 prototype. Verify them against
// the actual carrier board before flashing. SPI is shared by the radio, SD card
// and (future) display; each device has its own CS.
namespace config {

constexpr unsigned long kSerialBaud = 115200;

// Shared SPI bus.
constexpr uint8_t kSpiSckPin = 12;
constexpr uint8_t kSpiMisoPin = 13;
constexpr uint8_t kSpiMosiPin = 11;

// RFM95.
constexpr uint8_t kRadioCsPin = 10;
constexpr uint8_t kRadioResetPin = 16;
constexpr uint8_t kRadioDio0Pin = 15;
constexpr float kRadioFrequencyMhz = 915.0f;
constexpr float kRadioBitrateKbps = 4.8f;
constexpr float kRadioDeviationKhz = 5.0f;
constexpr float kRadioBandwidthKhz = 125.0f;
constexpr int8_t kRadioPowerDbm = 17;

// Micro-SD.
constexpr uint8_t kSdCsPin = 9;
constexpr uint32_t kSdFrequencyHz = 4000000;

// DS3231.
constexpr uint8_t kI2cSdaPin = 4;
constexpr uint8_t kI2cSclPin = 5;

// ST7735-class 1" panel (not enabled yet; display library is a seam).
constexpr uint8_t kDisplayCsPin = 8;
constexpr uint8_t kDisplayDcPin = 7;
constexpr uint8_t kDisplayResetPin = 6;

constexpr uint8_t kStatusLedPin = 2;

constexpr bool kEnableSdArchive = true;
constexpr bool kEnableRtc = true;
constexpr bool kEnableDisplay = false;
constexpr bool kEnableLanOta = true;

constexpr uint32_t kRadioReceiveTimeoutMs = 100;
constexpr uint32_t kLoopDelayMs = 5;
constexpr uint32_t kStatusIntervalMs = 2000;
constexpr uint32_t kNtpRetryIntervalMs = 60000;
constexpr uint32_t kNtpSyncIntervalMs = 6UL * 60UL * 60UL * 1000UL;

// At most this many observations wait in RAM for a cloud connection. The SD
// card remains the durable copy beyond this window.
constexpr uint8_t kMaxPendingUploads = 8;
constexpr uint8_t kMaxUploadAttempts = 8;
constexpr uint32_t kUploadRetryBaseMs = 5000;
constexpr uint32_t kUploadRetryMaxMs = 300000;

}  // namespace config
