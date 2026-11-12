#include "DavisEncoder.h"

#include <cmath>

#include "DavisConstants.h"
#include "DavisCrc.h"
#include "DavisDecoder.h"

namespace akashvani {
namespace {

constexpr float kMphToMetresPerSecond = 0.44704f;

uint8_t clampByte(long value) {
  if (value < 0) {
    return 0;
  }
  if (value > 255) {
    return 255;
  }
  return static_cast<uint8_t>(value);
}

}  // namespace

const char* toString(DavisEncodeStatus status) {
  switch (status) {
    case DavisEncodeStatus::Ok:
      return "ok";
    case DavisEncodeStatus::BufferTooSmall:
      return "buffer-too-small";
    case DavisEncodeStatus::UnsupportedProtocol:
      return "unsupported-protocol";
    case DavisEncodeStatus::InvalidObservation:
      return "invalid-observation";
  }
  return "unknown";
}

DavisEncodeStatus encodeDavisFrame(const DavisObservation& observation,
                                   DavisMessageType messageType, uint8_t* output,
                                   size_t outputSize, size_t& written,
                                   uint8_t testSequence) {
  written = 0;
  if (output == nullptr || outputSize < kDavisFrameBytes) {
    return DavisEncodeStatus::BufferTooSmall;
  }
  if (observation.transmitterId < kDavisMinTransmitterId ||
      observation.transmitterId > kDavisMaxTransmitterId) {
    return DavisEncodeStatus::InvalidObservation;
  }

  if (!std::isfinite(observation.windSpeedMs) ||
      !std::isfinite(observation.windDirectionDeg)) {
    return DavisEncodeStatus::InvalidObservation;
  }

  uint8_t frame[kDavisFrameBytes] = {};
  const uint8_t messageId = static_cast<uint8_t>(messageType);
  frame[0] = static_cast<uint8_t>((messageId << 4) | (observation.transmitterId & 0x07u));
  frame[1] = clampByte(std::lround(observation.windSpeedMs / kMphToMetresPerSecond));

  float direction = observation.windDirectionDeg;
  if (std::isfinite(direction)) {
    while (direction < 0.0f) {
      direction += 360.0f;
    }
    while (direction >= 360.0f) {
      direction -= 360.0f;
    }
    // Undo the receiver's 180-degree rotation.
    direction = direction >= 180.0f ? direction - 180.0f : direction + 180.0f;
    frame[2] = clampByte(std::lround(direction * 255.0f / 360.0f));
  }

  switch (messageType) {
    case DavisMessageType::GoldcapVoltage: {
      if (!std::isfinite(observation.batteryVoltageV)) {
        return DavisEncodeStatus::InvalidObservation;
      }
      const long raw = std::lround(observation.batteryVoltageV * 100.0f);
      if (raw < 0 || raw > 1023) {
        return DavisEncodeStatus::InvalidObservation;
      }
      frame[3] = static_cast<uint8_t>((raw >> 2) & 0xFF);
      frame[4] = static_cast<uint8_t>((raw & 0x03) << 6);
      break;
    }
    case DavisMessageType::RainRate: {
      if (!std::isfinite(observation.rainRateMmPerHour) ||
          observation.rainRateMmPerHour < 0.0f) {
        return DavisEncodeStatus::InvalidObservation;
      }
      if (observation.rainRateMmPerHour <= 0.0f) {
        frame[3] = 0xFF;
        frame[4] = 0;
        break;
      }
      // Low-rate mode: raw = 3600 / clicksPerHour = 720 / mmPerHour.
      // High-rate mode: raw = 57600 / clicksPerHour = 11520 / mmPerHour.
      // The high four bits are carried in byte 4 bits 4-5, so raw is 10 bits.
      long raw = std::lround(720.0f / observation.rainRateMmPerHour);
      uint8_t flags = 0x40;  // low rain rate
      if (raw > 1023) {
        raw = std::lround(11520.0f / observation.rainRateMmPerHour);
        flags = 0x00;
      }
      if (raw < 1 || raw > 1023) {
        return DavisEncodeStatus::InvalidObservation;
      }
      frame[3] = static_cast<uint8_t>(raw & 0xFF);
      frame[4] = static_cast<uint8_t>((((raw >> 8) & 0x03) << 4) | flags);
      break;
    }
    case DavisMessageType::UvIndex: {
      if (!std::isfinite(observation.uvIndex) || observation.uvIndex < 0.0f) {
        return DavisEncodeStatus::InvalidObservation;
      }
      const long raw = std::lround(observation.uvIndex * 50.0f);
      if (raw > 0x3FF) {
        return DavisEncodeStatus::InvalidObservation;
      }
      frame[3] = static_cast<uint8_t>(raw >> 2);
      frame[4] = static_cast<uint8_t>((raw & 0x03) << 6);
      break;
    }
    case DavisMessageType::SolarRadiationWm2: {
      if (!std::isfinite(observation.solarRadiationWm2) ||
          observation.solarRadiationWm2 < 0.0f) {
        return DavisEncodeStatus::InvalidObservation;
      }
      const long raw = std::lround(observation.solarRadiationWm2 / 1.757936f);
      if (raw > 0x3FF) {
        return DavisEncodeStatus::InvalidObservation;
      }
      frame[3] = static_cast<uint8_t>(raw >> 2);
      frame[4] = static_cast<uint8_t>((raw & 0x03) << 6);
      break;
    }
    case DavisMessageType::SolarRadiation: {
      if (!std::isfinite(observation.solarRadiationWm2) ||
          observation.solarRadiationWm2 < 0.0f) {
        return DavisEncodeStatus::InvalidObservation;
      }
      const long raw = std::lround(observation.solarRadiationWm2);
      if (raw > 1023) {
        return DavisEncodeStatus::InvalidObservation;
      }
      frame[3] = static_cast<uint8_t>(raw >> 2);
      frame[4] = static_cast<uint8_t>((raw & 0x03) << 6);
      break;
    }
    case DavisMessageType::Temperature: {
      if (!std::isfinite(observation.temperatureC)) {
        return DavisEncodeStatus::InvalidObservation;
      }
      const float fahrenheit = observation.temperatureC * 9.0f / 5.0f + 32.0f;
      const long raw = std::lround(fahrenheit * 160.0f);
      if (raw < 0 || raw > 65535) {
        return DavisEncodeStatus::InvalidObservation;
      }
      frame[3] = static_cast<uint8_t>((raw >> 8) & 0xFF);
      frame[4] = static_cast<uint8_t>(raw & 0xFF);
      break;
    }
    case DavisMessageType::GustSpeed: {
      if (!std::isfinite(observation.windGustMs) || observation.windGustMs < 0.0f) {
        return DavisEncodeStatus::InvalidObservation;
      }
      frame[3] = clampByte(std::lround(observation.windGustMs / kMphToMetresPerSecond));
      break;
    }
    case DavisMessageType::Humidity: {
      if (!std::isfinite(observation.humidityPct) || observation.humidityPct < 0.0f ||
          observation.humidityPct > 100.0f) {
        return DavisEncodeStatus::InvalidObservation;
      }
      const long raw = std::lround(observation.humidityPct * 10.0f);
      frame[3] = static_cast<uint8_t>(raw & 0xFF);
      frame[4] = static_cast<uint8_t>(((raw >> 8) & 0x0F) << 4);
      break;
    }
    case DavisMessageType::RainCounter:
      frame[3] = 0;
      frame[4] = 0;
      break;
    case DavisMessageType::Unknown3:
      frame[3] = 0;
      frame[4] = 0;
      break;
    default:
      return DavisEncodeStatus::UnsupportedProtocol;
  }

  frame[5] = testSequence;
  const uint16_t crc = davisCrc16(frame, 6);
  frame[6] = static_cast<uint8_t>(crc >> 8);
  frame[7] = static_cast<uint8_t>(crc & 0xFF);

  for (size_t i = 0; i < kDavisFrameBytes; ++i) {
    output[i] = frame[i];
  }
  written = kDavisFrameBytes;
  return DavisEncodeStatus::Ok;
}

DavisEncodeStatus encodeDavisFrame(const DavisObservation& observation, uint8_t* output,
                                   size_t outputSize, size_t& written) {
  if (observation.has(DavisField::Temperature)) {
    return encodeDavisFrame(observation, DavisMessageType::Temperature, output, outputSize,
                            written);
  }
  if (observation.has(DavisField::Humidity)) {
    return encodeDavisFrame(observation, DavisMessageType::Humidity, output, outputSize,
                            written);
  }
  if (observation.has(DavisField::RainRate)) {
    return encodeDavisFrame(observation, DavisMessageType::RainRate, output, outputSize,
                            written);
  }
  if (observation.has(DavisField::SolarRadiation)) {
    return encodeDavisFrame(observation, DavisMessageType::SolarRadiation, output,
                            outputSize, written);
  }
  if (observation.has(DavisField::WindGust)) {
    return encodeDavisFrame(observation, DavisMessageType::GustSpeed, output, outputSize,
                            written);
  }
  if (observation.has(DavisField::BatteryVoltage)) {
    return encodeDavisFrame(observation, DavisMessageType::GoldcapVoltage, output,
                            outputSize, written);
  }
  return DavisEncodeStatus::InvalidObservation;
}

}  // namespace akashvani
