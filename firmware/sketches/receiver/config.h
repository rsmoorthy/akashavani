#pragma once

#include <cstdint>

#if __has_include("secrets.h")
#include "secrets.h"
#else
#include "secrets.example.h"
#endif

// Receiver pin map and runtime tuning.
//
// The pin values below match the observed ESP32-D0WD-V3 bench wiring from
// 2026-10-26 (see docs/hardware/pinmap.md). Change the header and the pin map
// together before using a different carrier board.
namespace config {

constexpr unsigned long kSerialBaud = 115200;
// Field service window: wait this long after power-up before the first log line
// so a laptop running minicom/`make fw-monitor` can attach to the port in time.
constexpr uint32_t kSerialStartupDelayMs = 3000;

// Shared SPI bus (VSPI on the bench boards).
constexpr uint8_t kSpiSckPin = 18;
constexpr uint8_t kSpiMisoPin = 19;
constexpr uint8_t kSpiMosiPin = 23;

// RFM69. On USB1 the RESET line is not wired, but the pin is harmless.
constexpr uint8_t kRadioCsPin = 5;
constexpr uint8_t kRadioResetPin = 17;
constexpr uint8_t kRadioDio0Pin = 13;
constexpr float kRadioFrequencyMhz = 915.0f;
constexpr float kRadioBitrateKbps = 19.2f;
constexpr float kRadioDeviationKhz = 4.8f;
constexpr float kRadioBandwidthKhz = 25.0f;
constexpr int8_t kRadioPowerDbm = 20;

// RF/Davis bench mode. It skips Wi-Fi, SD, OTA and the cloud path so the
// 3-second channel-hopping test is not disturbed. Set to false to run the full
// receiver again; the radio driver and decoder are shared by both paths.
constexpr bool kRfTestMode = false;
constexpr uint8_t kRadioInitialChannel = 0;
constexpr uint32_t kRadioSlotMs = 3000;
constexpr uint32_t kRadioHopMs = 3200;
constexpr uint32_t kRadioSearchHopMs = 3500;
constexpr uint8_t kRadioSearchAfterMisses = 7;

// Micro-SD.
constexpr uint8_t kSdCsPin = 16;
constexpr uint32_t kSdFrequencyHz = 4000000;

// DS3231 / BME280 / SSD1306 I2C bus.
constexpr uint8_t kI2cSdaPin = 21;
constexpr uint8_t kI2cSclPin = 22;

// ST7735-class 1" panel (not enabled; the bench boards carry an I2C SSD1306).
constexpr uint8_t kDisplayCsPin = 8;
constexpr uint8_t kDisplayDcPin = 7;
constexpr uint8_t kDisplayResetPin = 6;

// SSD1306-class 128x64 I2C status panel. It shares the I2C bus with the DS3231
// and BME280 at 0x3C on SDA 21 / SCL 22 (both bench boards).
constexpr uint8_t kDisplayAddress = 0x3C;

constexpr uint8_t kStatusLedPin = 2;

// Provisioning access point. The SSID is the prefix plus the last MAC byte,
// e.g. "akashvani-e0". Change the password, IP address and DHCP pool here.
constexpr const char* kApSsidPrefix = "akashvani-";
constexpr const char* kApPassword = "doordarshan";
constexpr uint8_t kApChannel = 6;
constexpr uint8_t kApMaxClients = 4;
constexpr const char* kApIp = "10.90.0.1";
constexpr const char* kApGateway = "10.90.0.1";
constexpr const char* kApSubnet = "255.255.255.0";
constexpr const char* kApDhcpFirst = "10.90.0.11";
constexpr const char* kApDhcpLast = "10.90.0.20";

constexpr bool kEnableSdArchive = true;
constexpr bool kEnableRtc = true;
constexpr bool kEnableDisplay = true;
constexpr bool kEnableLanOta = true;

// Development identity/network. These values are temporary and will be replaced
// by provisioning. The Wi-Fi credentials and device token live in secrets.h.
constexpr uint16_t kStationId = 1;
constexpr const char* kDeviceToken = AK_DEV_DEVICE_TOKEN;
constexpr const char* kApiToken = "secret";
constexpr const char* kWifiSsid = AK_DEV_WIFI_SSID;
constexpr const char* kWifiPassword = AK_DEV_WIFI_PASSWORD;
constexpr const char* kServerBaseUrl = "http://192.168.29.31:8000";
constexpr uint32_t kCommandPollIntervalSeconds = 30;

constexpr uint32_t kRadioReceiveTimeoutMs = 100;
constexpr uint32_t kLoopDelayMs = 5;
constexpr uint32_t kBmeSampleIntervalMs = 10000;
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
