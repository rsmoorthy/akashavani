#include "ArchiveCollector.h"

#include <cmath>

namespace akashavani {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr uint8_t kRainCounterWrap = 128;
constexpr uint8_t kMaxPlausibleRainDelta = 64;

bool finite(float value) { return std::isfinite(value); }

}  // namespace

ArchiveCollector::ArchiveCollector(uint32_t bucketSeconds, uint32_t slotMs)
    : bucketSeconds_(bucketSeconds == 0 ? 300 : bucketSeconds),
      slotMs_(slotMs == 0 ? 3000 : slotMs) {}

void ArchiveCollector::reset() {
  started_ = false;
  ready_ = false;
  bucketEndEpoch_ = 0;
  current_ = ArchiveRecord();
  completed_ = ArchiveRecord();
  haveRainCounter_ = false;
  lastRainCounter_ = 0;
}

int64_t ArchiveCollector::bucketEndFor(int64_t epochSeconds) const {
  const int64_t bucket = static_cast<int64_t>(bucketSeconds_);
  return ((epochSeconds / bucket) + 1) * bucket;
}

void ArchiveCollector::clearCurrent() {
  current_ = ArchiveRecord();
  current_.bucketEndEpoch = bucketEndEpoch_;
  current_.intervalMinutes = bucketSeconds_ / 60;
  current_.framesExpected = (bucketSeconds_ * 1000UL) / slotMs_;
}

bool ArchiveCollector::ensureBucket(int64_t epochSeconds) {
  if (epochSeconds <= 0) {
    return false;
  }
  if (!started_) {
    started_ = true;
    bucketEndEpoch_ = bucketEndFor(epochSeconds);
    clearCurrent();
    return true;
  }
  while (epochSeconds >= bucketEndEpoch_) {
    // Emit every bucket, even with zero valid frames: the record doubles as the
    // 5-minute radio-health report (frames_valid = 0, environmental means null).
    finalize();
    ready_ = true;
    bucketEndEpoch_ += static_cast<int64_t>(bucketSeconds_);
    clearCurrent();
  }
  return true;
}

bool ArchiveCollector::add(const DavisObservation& observation, int16_t rssiDbm,
                           bool batteryLow, int64_t epochSeconds) {
  if (!ensureBucket(epochSeconds)) {
    return false;
  }
  current_.transmitterId = observation.transmitterId;
  ++current_.framesValid;
  current_.rssiSum += rssiDbm;
  if (current_.rssiCount == 0 || rssiDbm < current_.rssiMin) {
    current_.rssiMin = rssiDbm;
  }
  if (current_.rssiCount == 0 || rssiDbm > current_.rssiMax) {
    current_.rssiMax = rssiDbm;
  }
  ++current_.rssiCount;
  current_.batteryLow = current_.batteryLow || batteryLow;

  if (observation.has(DavisField::WindSpeed) && finite(observation.windSpeedMs)) {
    current_.windSpeedMsSum += observation.windSpeedMs;
    ++current_.windSpeedCount;
  }

  if (observation.has(DavisField::WindDirection) && finite(observation.windDirectionDeg)) {
    const double radians = static_cast<double>(observation.windDirectionDeg) * kPi / 180.0;
    const double speed =
        observation.has(DavisField::WindSpeed) && finite(observation.windSpeedMs)
            ? static_cast<double>(observation.windSpeedMs)
            : 1.0;
    current_.windDirSinSum += std::sin(radians) * speed;
    current_.windDirCosSum += std::cos(radians) * speed;
    current_.windDirSpeedSum += speed;
    ++current_.windDirCount;
  }

  if (observation.has(DavisField::Temperature) && finite(observation.temperatureC)) {
    current_.outTempCSum += observation.temperatureC;
    ++current_.outTempCount;
  }
  if (observation.has(DavisField::Humidity) && finite(observation.humidityPct)) {
    current_.outHumidityPctSum += observation.humidityPct;
    ++current_.outHumidityCount;
  }
  if (observation.has(DavisField::WindGust) && finite(observation.windGustMs)) {
    if (current_.windGustCount == 0 || observation.windGustMs > current_.windGustMaxMs) {
      current_.windGustMaxMs = observation.windGustMs;
      if (observation.has(DavisField::WindDirection) &&
          finite(observation.windDirectionDeg)) {
        current_.windGustDirDeg = observation.windDirectionDeg;
      }
    }
    ++current_.windGustCount;
  }
  if (observation.has(DavisField::UvIndex) && finite(observation.uvIndex)) {
    current_.uvIndexSum += observation.uvIndex;
    ++current_.uvCount;
  }
  if (observation.has(DavisField::SolarRadiation) &&
      finite(observation.solarRadiationWm2)) {
    current_.solarWm2Sum += observation.solarRadiationWm2;
    ++current_.solarCount;
  }
  if (observation.has(DavisField::BatteryVoltage) &&
      finite(observation.batteryVoltageV)) {
    const double value = observation.batteryVoltageV;
    if (current_.supercapCount == 0 || value < current_.supercapMinV) {
      current_.supercapMinV = value;
    }
    current_.supercapV = value;
    ++current_.supercapCount;
  }

  if (observation.rainCounterValid) {
    ++current_.rainCount;
    if (!haveRainCounter_) {
      haveRainCounter_ = true;
      lastRainCounter_ = observation.rainCounter;
    } else {
      const uint8_t delta = static_cast<uint8_t>(
          (observation.rainCounter + kRainCounterWrap - lastRainCounter_) %
          kRainCounterWrap);
      lastRainCounter_ = observation.rainCounter;
      if (delta > 0 && delta <= kMaxPlausibleRainDelta) {
        current_.rainTips += delta;
      }
    }
  }
  if (observation.rainRateValid) {
    current_.rainRateValid = true;
    current_.rainRateRaw = observation.rainRateRaw;
    current_.rainRateHigh = observation.rainRateHigh;
  }
  return true;
}

bool ArchiveCollector::addBme280(float temperatureC, float humidityPct,
                                 float pressureHpa, int64_t epochSeconds) {
  if (!ensureBucket(epochSeconds)) {
    return false;
  }
  if (finite(temperatureC)) {
    current_.inTempCSum += temperatureC;
    ++current_.inTempCount;
  }
  if (finite(humidityPct)) {
    current_.inHumidityPctSum += humidityPct;
    ++current_.inHumidityCount;
  }
  if (finite(pressureHpa)) {
    current_.pressureHpaSum += pressureHpa;
    ++current_.pressureCount;
  }
  return true;
}

bool ArchiveCollector::poll(int64_t epochSeconds, ArchiveRecord& output) {
  if (ensureBucket(epochSeconds) && ready_) {
    output = completed_;
    ready_ = false;
    return true;
  }
  return false;
}

void ArchiveCollector::finalize() {
  completed_ = current_;
  if (completed_.rssiCount > 0) {
    // Keep the sum; the server turns it into an average.
    completed_.rssiMin = current_.rssiMin;
    completed_.rssiMax = current_.rssiMax;
  }
}

void ArchiveCollector::noteBadFrame() { ++current_.framesCrcBad; }

void ArchiveCollector::noteBadFrames(uint32_t count, int16_t rssiMin, int32_t rssiSum,
                                     int16_t rssiMax) {
  if (count == 0) {
    return;
  }
  if (current_.framesCrcBad == 0) {
    current_.crcBadRssiMin = rssiMin;
    current_.crcBadRssiMax = rssiMax;
  } else {
    if (rssiMin < current_.crcBadRssiMin) {
      current_.crcBadRssiMin = rssiMin;
    }
    if (rssiMax > current_.crcBadRssiMax) {
      current_.crcBadRssiMax = rssiMax;
    }
  }
  current_.framesCrcBad += count;
  current_.crcBadRssiSum += rssiSum;
}

void ArchiveCollector::noteResync() { ++current_.resyncCount; }

void ArchiveCollector::noteSearch() { ++current_.searchCount; }

void ArchiveCollector::noteGap(uint16_t slots) {
  current_.gapSlots += slots;
  if (slots > current_.maxGapSlots) {
    current_.maxGapSlots = slots;
  }
}

void ArchiveCollector::resetRadioCounters() {
  current_.resyncCount = 0;
  current_.searchCount = 0;
  current_.gapSlots = 0;
  current_.maxGapSlots = 0;
}

}  // namespace akashavani
