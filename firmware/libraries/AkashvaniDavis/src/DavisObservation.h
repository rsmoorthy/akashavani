#pragma once

#include <cmath>
#include <cstdint>

namespace akashvani {

enum class DavisField : uint32_t {
  Temperature = 1u << 0,
  Humidity = 1u << 1,
  Pressure = 1u << 2,
  WindSpeed = 1u << 3,
  WindDirection = 1u << 4,
  WindGust = 1u << 5,
  RainRate = 1u << 6,
  RainDay = 1u << 7,
  UvIndex = 1u << 8,
  SolarRadiation = 1u << 9,
  BatteryVoltage = 1u << 10,
};

constexpr uint32_t kAllDavisFields =
    static_cast<uint32_t>(DavisField::Temperature) |
    static_cast<uint32_t>(DavisField::Humidity) |
    static_cast<uint32_t>(DavisField::Pressure) |
    static_cast<uint32_t>(DavisField::WindSpeed) |
    static_cast<uint32_t>(DavisField::WindDirection) |
    static_cast<uint32_t>(DavisField::WindGust) |
    static_cast<uint32_t>(DavisField::RainRate) |
    static_cast<uint32_t>(DavisField::RainDay) |
    static_cast<uint32_t>(DavisField::UvIndex) |
    static_cast<uint32_t>(DavisField::SolarRadiation) |
    static_cast<uint32_t>(DavisField::BatteryVoltage);

// Values that are not present stay NaN and their bit is cleared in
// validFields. Units are part of the field name and the protocol schema.
struct DavisObservation {
  uint32_t validFields = 0;

  // Receiver-assigned metadata. The transmitter simulator leaves the timestamp
  // at zero; the receiver stamps frames when they arrive.
  int64_t observedAtEpoch = 0;
  uint8_t transmitterId = 0;
  uint8_t channel = 0;
  uint32_t sequence = 0;

  float temperatureC = NAN;
  float humidityPct = NAN;
  float pressureHpa = NAN;
  float windSpeedMs = NAN;
  float windDirectionDeg = NAN;
  float windGustMs = NAN;
  float rainRateMmPerHour = NAN;
  float rainDayMm = NAN;
  float uvIndex = NAN;
  float solarRadiationWm2 = NAN;
  float batteryVoltageV = NAN;

  // Raw rain fields. The receiver sends tip counts and the raw 0x5 click
  // interval; the server converts both using the configured bucket size.
  uint8_t rainCounter = 0;
  bool rainCounterValid = false;
  uint16_t rainRateRaw = 0;
  bool rainRateHigh = false;
  bool rainRateValid = false;

  bool has(DavisField field) const {
    return (validFields & static_cast<uint32_t>(field)) != 0;
  }

  void set(DavisField field, float value) {
    validFields |= static_cast<uint32_t>(field);
    switch (field) {
      case DavisField::Temperature:
        temperatureC = value;
        break;
      case DavisField::Humidity:
        humidityPct = value;
        break;
      case DavisField::Pressure:
        pressureHpa = value;
        break;
      case DavisField::WindSpeed:
        windSpeedMs = value;
        break;
      case DavisField::WindDirection:
        windDirectionDeg = value;
        break;
      case DavisField::WindGust:
        windGustMs = value;
        break;
      case DavisField::RainRate:
        rainRateMmPerHour = value;
        break;
      case DavisField::RainDay:
        rainDayMm = value;
        break;
      case DavisField::UvIndex:
        uvIndex = value;
        break;
      case DavisField::SolarRadiation:
        solarRadiationWm2 = value;
        break;
      case DavisField::BatteryVoltage:
        batteryVoltageV = value;
        break;
    }
  }

  void clear() { *this = DavisObservation(); }
};

}  // namespace akashvani
