// Hardware bring-up sketch. This is not production firmware.
//
// It probes the shared SPI bus, the DS3231, the SD card and the RFM95 seam.
// The current bench hardware is RFM69 (version 0x24), not RFM95 (0x12); the
// RFM69 driver is still a stub. It then
// leaves the board in AP-less Wi-Fi scan mode. Run with `make fw-test-hw`.

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <Wire.h>
#include <WiFi.h>

#include "AkashvaniCore.h"
#include "AkashvaniRtc.h"
#include "Rfm95Radio.h"

namespace {

constexpr uint8_t kSpiSck = 12;
constexpr uint8_t kSpiMiso = 13;
constexpr uint8_t kSpiMosi = 11;
constexpr uint8_t kRadioCs = 10;
constexpr uint8_t kRadioReset = 16;
constexpr uint8_t kRadioDio0 = 15;
constexpr uint8_t kSdCs = 9;
constexpr uint8_t kI2cSda = 4;
constexpr uint8_t kI2cScl = 5;

akashvani::Ds3231Clock rtc(Wire, kI2cSda, kI2cScl);
akashvani::TimeService timeService;
akashvani::Rfm95Radio radio(
    SPI, akashvani::RadioPins{kRadioCs, kRadioReset, kRadioDio0, kSpiSck, kSpiMiso, kSpiMosi});
bool rtcReady = false;

void scanI2c() {
  uint8_t found = 0;
  for (uint8_t address = 1; address < 127; ++address) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) {
      AK_LOG_INFO("i2c", "device at 0x%02x", address);
      ++found;
    }
  }
  AK_LOG_INFO("i2c", "%u device(s)", found);
}

}  // namespace

void setup() {
  akashvani::Log::begin(115200);
  akashvani::Log::setLevel(akashvani::LogLevel::Debug);
  delay(200);
  AK_LOG_INFO("bringup", "Akashvani hardware bring-up");

  Wire.begin(kI2cSda, kI2cScl);
  scanI2c();
  rtcReady = akashvani::isOk(rtc.begin());
  if (rtcReady) {
    timeService.begin(rtc);
    AK_LOG_INFO("rtc", "epoch=%lld lostPower=%s", static_cast<long long>(rtc.nowEpoch()),
                rtc.lostPower() ? "yes" : "no");
  }

  SPI.begin(kSpiSck, kSpiMiso, kSpiMosi);
  if (SD.begin(kSdCs, SPI, 4000000)) {
    AK_LOG_INFO("sd", "mounted, cardType=%u sizeMB=%llu",
                static_cast<unsigned>(SD.cardType()),
                static_cast<unsigned long long>(SD.cardSize() / (1024ULL * 1024ULL)));
  } else {
    AK_LOG_ERROR("sd", "mount failed");
  }

  akashvani::RadioConfig config;
  config.chip = akashvani::RadioChip::Rfm95;
  config.frequencyMhz = 915.0f;
  const akashvani::Status status = radio.begin(config);
  AK_LOG_INFO("radio", "begin=%s version=0x%02x", akashvani::statusToString(status),
              radio.versionRegister());

  WiFi.mode(WIFI_STA);
  const int networks = WiFi.scanNetworks();
  AK_LOG_INFO("wifi", "scan found %d network(s)", networks);
  for (int i = 0; i < networks; ++i) {
    AK_LOG_DEBUG("wifi", "  %s (%d dBm)", WiFi.SSID(i).c_str(), WiFi.RSSI(i));
  }
  WiFi.scanDelete();
  AK_LOG_INFO("bringup", "done");
}

void loop() {
  static uint32_t lastPrint = 0;
  if (millis() - lastPrint < 5000) {
    delay(100);
    return;
  }
  lastPrint = millis();
  if (rtcReady) {
    char timestamp[21];
    const int64_t epoch = timeService.nowEpoch();
    akashvani::formatIso8601Utc(epoch, timestamp, sizeof(timestamp));
    AK_LOG_INFO("clock", "%s (%s)", timestamp, timeService.source());
  } else {
    AK_LOG_WARN("clock", "RTC unavailable");
  }
}
