#pragma once

#include <cstdint>

#include "DavisObservation.h"

namespace akashvani {

// Deterministic pseudo-random weather generator.
//
// This is deliberately simple and dependency-free: the simulator board produces
// plausible, bounded, repeatable observations so the receiver, archiver and
// cloud path can be exercised before a real station is available. It does not
// emulate Davis RF modulation; that is `DavisEncoder`'s job once ported.
class DavisSimulator {
 public:
  explicit DavisSimulator(uint32_t seed = 1, uint8_t transmitterId = 1);

  DavisObservation next();

  uint8_t transmitterId() const { return transmitterId_; }
  void setTransmitterId(uint8_t id);
  void setChannel(uint8_t channel) { channel_ = channel; }
  void reset(uint32_t seed);

 private:
  uint32_t nextRandom();
  float nextUnit();
  float boundedWalk(float value, float delta, float minimum, float maximum);

  uint32_t state_ = 1;
  uint8_t transmitterId_ = 1;
  uint8_t channel_ = 0;
  uint32_t sequence_ = 0;

  float temperatureC_ = 21.5f;
  float humidityPct_ = 55.0f;
  float pressureHpa_ = 1013.2f;
  float windSpeedMs_ = 2.5f;
  float windDirectionDeg_ = 180.0f;
  float rainDayMm_ = 0.0f;
  float uvIndex_ = 3.0f;
  float solarWm2_ = 350.0f;
  float batteryV_ = 3.15f;
};

}  // namespace akashvani
