#pragma once

#include <cstdint>

#include "ArchiveRecord.h"
#include "DavisObservation.h"

namespace akashvani {

// Accumulates decoded Davis observations and BME280 samples into one
// five-minute record. It does not divide or convert units; the server does
// that. The collector is host-testable and does not use Arduino headers.
class ArchiveCollector {
 public:
  ArchiveCollector(uint32_t bucketSeconds = 300, uint32_t slotMs = 3000);

  void reset();
  bool add(const DavisObservation& observation, int16_t rssiDbm, bool batteryLow,
           int64_t epochSeconds);
  bool addBme280(float temperatureC, float humidityPct, float pressureHpa,
                 int64_t epochSeconds);
  bool poll(int64_t epochSeconds, ArchiveRecord& output);

  void noteBadFrame();
  void noteBadFrames(uint32_t count, int16_t rssiMin, int32_t rssiSum, int16_t rssiMax);
  void noteResync();
  void noteSearch();
  void noteGap(uint16_t slots);
  void resetRadioCounters();

  bool ready() const { return ready_; }

  // Live view of the bucket being filled, used by the local status page.
  const ArchiveRecord& current() const { return current_; }
  uint32_t expectedFrames() const { return (bucketSeconds_ * 1000UL) / slotMs_; }

 private:
  bool ensureBucket(int64_t epochSeconds);
  void clearCurrent();
  void finalize();
  int64_t bucketEndFor(int64_t epochSeconds) const;

  uint32_t bucketSeconds_;
  uint32_t slotMs_;
  bool started_ = false;
  bool ready_ = false;
  int64_t bucketEndEpoch_ = 0;
  ArchiveRecord current_;
  ArchiveRecord completed_;

  bool haveRainCounter_ = false;
  uint8_t lastRainCounter_ = 0;
};

}  // namespace akashvani
