#pragma once

#include <cstdint>

namespace akashvani {

// One completed five-minute bucket. The receiver sends sums and counts; the
// Python server performs the divisions, unit conversions and derived values.
struct ArchiveRecord {
  int64_t bucketEndEpoch = 0;
  uint32_t intervalMinutes = 5;
  uint32_t sequence = 0;
  uint8_t transmitterId = 0;

  double outTempCSum = 0.0;
  uint32_t outTempCount = 0;
  double outHumidityPctSum = 0.0;
  uint32_t outHumidityCount = 0;
  double pressureHpaSum = 0.0;
  uint32_t pressureCount = 0;
  double inTempCSum = 0.0;
  uint32_t inTempCount = 0;
  double inHumidityPctSum = 0.0;
  uint32_t inHumidityCount = 0;

  double windSpeedMsSum = 0.0;
  uint32_t windSpeedCount = 0;
  double windDirSinSum = 0.0;
  double windDirCosSum = 0.0;
  double windDirSpeedSum = 0.0;
  uint32_t windDirCount = 0;
  double windGustMaxMs = 0.0;
  double windGustDirDeg = 0.0;
  uint32_t windGustCount = 0;

  uint32_t rainTips = 0;
  uint32_t rainCount = 0;
  uint16_t rainRateRaw = 0;
  bool rainRateHigh = false;
  bool rainRateValid = false;

  double solarWm2Sum = 0.0;
  uint32_t solarCount = 0;
  double uvIndexSum = 0.0;
  uint32_t uvCount = 0;

  bool batteryLow = false;
  double supercapV = 0.0;
  double supercapMinV = 0.0;
  uint32_t supercapCount = 0;

  uint32_t framesExpected = 0;
  uint32_t framesValid = 0;
  uint32_t framesCrcBad = 0;
  int16_t crcBadRssiMin = 0;
  int32_t crcBadRssiSum = 0;
  int16_t crcBadRssiMax = 0;
  uint32_t resyncCount = 0;
  uint32_t searchCount = 0;
  uint32_t gapSlots = 0;
  uint16_t maxGapSlots = 0;
  double rssiSum = 0.0;
  int16_t rssiMin = 0;
  int16_t rssiMax = 0;
  uint32_t rssiCount = 0;
};

}  // namespace akashvani
