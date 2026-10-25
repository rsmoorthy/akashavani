#include "DavisSimulator.h"

#include <cmath>

namespace akashvani {

DavisSimulator::DavisSimulator(uint32_t seed, uint8_t transmitterId) {
  setTransmitterId(transmitterId);
  reset(seed);
}

void DavisSimulator::setTransmitterId(uint8_t id) {
  if (id < 1) {
    id = 1;
  } else if (id > 8) {
    id = 8;
  }
  transmitterId_ = id;
}

void DavisSimulator::reset(uint32_t seed) {
  state_ = seed == 0 ? 0x6D2B79F5u : seed;
  sequence_ = 0;
  temperatureC_ = 21.5f;
  humidityPct_ = 55.0f;
  pressureHpa_ = 1013.2f;
  windSpeedMs_ = 2.5f;
  windDirectionDeg_ = 180.0f;
  rainDayMm_ = 0.0f;
  uvIndex_ = 3.0f;
  solarWm2_ = 350.0f;
  batteryV_ = 3.15f;
}

uint32_t DavisSimulator::nextRandom() {
  // xorshift32: small, deterministic and good enough for a simulator.
  uint32_t x = state_;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  state_ = x;
  return x;
}

float DavisSimulator::nextUnit() {
  return static_cast<float>(nextRandom() & 0x00FFFFFFu) / 16777216.0f;
}

float DavisSimulator::boundedWalk(float value, float delta, float minimum, float maximum) {
  value += (nextUnit() * 2.0f - 1.0f) * delta;
  if (value < minimum) {
    return minimum;
  }
  if (value > maximum) {
    return maximum;
  }
  return value;
}

DavisObservation DavisSimulator::next() {
  ++sequence_;

  temperatureC_ = boundedWalk(temperatureC_, 0.25f, -25.0f, 48.0f);
  humidityPct_ = boundedWalk(humidityPct_, 1.5f, 1.0f, 100.0f);
  pressureHpa_ = boundedWalk(pressureHpa_, 0.2f, 950.0f, 1050.0f);
  windSpeedMs_ = boundedWalk(windSpeedMs_, 1.0f, 0.0f, 35.0f);
  windDirectionDeg_ = std::fmod(windDirectionDeg_ + (nextUnit() * 2.0f - 1.0f) * 12.0f + 360.0f,
                                360.0f);
  uvIndex_ = boundedWalk(uvIndex_, 0.4f, 0.0f, 12.0f);
  solarWm2_ = boundedWalk(solarWm2_, 30.0f, 0.0f, 1200.0f);
  batteryV_ = boundedWalk(batteryV_, 0.02f, 2.4f, 3.6f);

  // Occasional rain so archive/upload paths get non-zero values.
  if (nextUnit() < 0.08f) {
    rainDayMm_ += nextUnit() * 0.4f;
  }

  DavisObservation observation;
  observation.validFields = kAllDavisFields;
  observation.transmitterId = transmitterId_;
  observation.channel = channel_;
  observation.sequence = sequence_;
  observation.set(DavisField::Temperature, temperatureC_);
  observation.set(DavisField::Humidity, humidityPct_);
  observation.set(DavisField::Pressure, pressureHpa_);
  observation.set(DavisField::WindSpeed, windSpeedMs_);
  observation.set(DavisField::WindDirection, windDirectionDeg_);
  observation.set(DavisField::WindGust, windSpeedMs_ * 1.35f);
  observation.set(DavisField::RainRate,
                 rainDayMm_ > 0.0f ? (nextUnit() < 0.3f ? 1.2f : 0.0f) : 0.0f);
  observation.set(DavisField::RainDay, rainDayMm_);
  observation.set(DavisField::UvIndex, uvIndex_);
  observation.set(DavisField::SolarRadiation, solarWm2_);
  observation.set(DavisField::BatteryVoltage, batteryV_);
  return observation;
}

}  // namespace akashvani
