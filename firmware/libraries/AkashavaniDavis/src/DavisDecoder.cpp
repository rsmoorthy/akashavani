#include "DavisDecoder.h"

#include <cmath>

#include "DavisConstants.h"
#include "DavisCrc.h"

namespace akashavani {
namespace {

constexpr float kMphToMetresPerSecond = 0.44704f;

float windDirectionDegrees(uint8_t raw) {
  float degrees = static_cast<float>(raw) * 360.0f / 255.0f;
  // Davis reports 0 = South. Rotate so 180 = South, matching the prototype.
  if (degrees >= 180.0f) {
    degrees -= 180.0f;
  } else {
    degrees += 180.0f;
  }
  return degrees;
}

bool validTemperature(float value) { return value >= -90.0f && value <= 70.0f; }

bool validHumidity(float value) { return value >= 0.0f && value <= 100.0f; }

// UV and solar share the same 10-bit field: byte 3 holds bits 9..2, byte 4
// bits 7..6 hold bits 1..0. Source: kobuki/VPTools ISSRx.ino `word(packet[3],
// packet[4]) >> 6`. Do not confuse this with the legacy 0x7 mapping below,
// which packs the same raw bits into only 8 bits.
uint16_t tenBitField(uint8_t b3, uint8_t b4) {
  return static_cast<uint16_t>((static_cast<uint16_t>(b3) << 2) | (b4 >> 6));
}

}  // namespace

const char* toString(DavisDecodeStatus status) {
  switch (status) {
    case DavisDecodeStatus::Ok:
      return "ok";
    case DavisDecodeStatus::TooShort:
      return "too-short";
    case DavisDecodeStatus::TooLong:
      return "too-long";
    case DavisDecodeStatus::CrcMismatch:
      return "crc-mismatch";
    case DavisDecodeStatus::UnsupportedProtocol:
      return "unsupported-protocol";
    case DavisDecodeStatus::InvalidValue:
      return "invalid-value";
  }
  return "unknown";
}

const char* toString(DavisMessageType type) {
  switch (type) {
    case DavisMessageType::GoldcapVoltage:
      return "goldcap";
    case DavisMessageType::Unknown3:
      return "unknown3";
    case DavisMessageType::UvIndex:
      return "uv";
    case DavisMessageType::RainRate:
      return "rain-rate";
    case DavisMessageType::SolarRadiationWm2:
      return "solar";
    case DavisMessageType::SolarRadiation:
      return "solar";
    case DavisMessageType::Temperature:
      return "temperature";
    case DavisMessageType::GustSpeed:
      return "gust";
    case DavisMessageType::Humidity:
      return "humidity";
    case DavisMessageType::RainCounter:
      return "rain-counter";
  }
  return "unknown";
}

DavisDecodeResult decodeDavisFrame(const uint8_t* frame, size_t length,
                                   uint32_t receivedAtMs) {
  DavisDecodeResult result;
  (void)receivedAtMs;

  if (frame == nullptr || length < kDavisMinFrameBytes) {
    result.status = DavisDecodeStatus::TooShort;
    return result;
  }
  if (length > kDavisMaxFrameBytes) {
    result.status = DavisDecodeStatus::TooLong;
    return result;
  }
  if (length != kDavisFrameBytes) {
    result.status = DavisDecodeStatus::UnsupportedProtocol;
    return result;
  }

  const uint16_t expected =
      static_cast<uint16_t>((static_cast<uint16_t>(frame[6]) << 8) | frame[7]);
  const uint16_t actual = davisCrc16(frame, 6);
  if (actual == 0 || actual != expected) {
    result.status = DavisDecodeStatus::CrcMismatch;
    return result;
  }

  const uint8_t first = frame[0];
  const uint8_t messageId = static_cast<uint8_t>(first >> 4);

  result.messageId = messageId;
  result.batteryLow = (first & 0x08u) != 0;
  result.benchSequence = frame[5];

  switch (messageId) {
    case 0x2:
      result.messageType = DavisMessageType::GoldcapVoltage;
      break;
    case 0x3:
      result.messageType = DavisMessageType::Unknown3;
      break;
    case 0x4:
      result.messageType = DavisMessageType::UvIndex;
      break;
    case 0x5:
      result.messageType = DavisMessageType::RainRate;
      break;
    case 0x6:
      result.messageType = DavisMessageType::SolarRadiationWm2;
      break;
    case 0x7:
      result.messageType = DavisMessageType::SolarRadiation;
      break;
    case 0x8:
      result.messageType = DavisMessageType::Temperature;
      break;
    case 0x9:
      result.messageType = DavisMessageType::GustSpeed;
      break;
    case 0xA:
      result.messageType = DavisMessageType::Humidity;
      break;
    case 0xE:
      result.messageType = DavisMessageType::RainCounter;
      break;
    default:
      result.status = DavisDecodeStatus::UnsupportedProtocol;
      return result;
  }

  DavisObservation& observation = result.observation;
  observation.transmitterId = static_cast<uint8_t>(first & 0x07u);

  // Wind speed and direction are present in every Davis ISS message.
  observation.set(DavisField::WindSpeed,
                  static_cast<float>(frame[1]) * kMphToMetresPerSecond);
  const float direction = windDirectionDegrees(frame[2]);
  if (direction < 0.0f || direction >= 360.0f) {
    result.status = DavisDecodeStatus::InvalidValue;
    return result;
  }
  observation.set(DavisField::WindDirection, direction);

  const uint8_t b3 = frame[3];
  const uint8_t b4 = frame[4];

  switch (result.messageType) {
    case DavisMessageType::GoldcapVoltage: {
      const float volts =
          static_cast<float>((static_cast<uint16_t>(b3) << 2) | ((b4 & 0xC0u) >> 6)) /
          100.0f;
      observation.set(DavisField::BatteryVoltage, volts);
      break;
    }
    case DavisMessageType::RainRate: {
      float millimetresPerHour = 0.0f;
      observation.rainRateValid = true;
      observation.rainRateHigh = (b4 & 0x40u) == 0;
      if (b3 != 0xFF) {
        const uint16_t raw = static_cast<uint16_t>(b3 + ((b4 & 0x30u) * 16));
        observation.rainRateRaw = raw;
        if (raw != 0) {
          const float clicksPerHour =
              observation.rainRateHigh ? 57600.0f / static_cast<float>(raw)
                                       : 3600.0f / static_cast<float>(raw);
          // Legacy decoded value retained for display/tests. The archive path
          // uses the raw value and the server-side bucket size instead.
          millimetresPerHour = clicksPerHour * 0.2f;
        }
      }
      observation.set(DavisField::RainRate, millimetresPerHour);
      break;
    }
    case DavisMessageType::UvIndex: {
      // VP2 UV index: raw / 50, 10-bit. 0x3FF is the "sensor absent" sentinel,
      // so the frame is still valid for its wind fields but carries no UV.
      const uint16_t raw = tenBitField(b3, b4);
      if (raw < 0x3FFu) {
        observation.set(DavisField::UvIndex, static_cast<float>(raw) / 50.0f);
      }
      break;
    }
    case DavisMessageType::SolarRadiationWm2: {
      // VP2 solar radiation: raw * 1.757936 W/m2, 10-bit. 0x3FE and 0x3FF both
      // mean the sensor is absent.
      const uint16_t raw = tenBitField(b3, b4);
      if (raw < 0x3FEu) {
        observation.set(DavisField::SolarRadiation, static_cast<float>(raw) * 1.757936f);
      }
      break;
    }
    case DavisMessageType::SolarRadiation: {
      const float watts = static_cast<float>(b3) * 4.0f +
                          static_cast<float>((b4 & 0xC0u) >> 6);
      observation.set(DavisField::SolarRadiation, watts);
      break;
    }
    case DavisMessageType::Temperature: {
      const float fahrenheit =
          static_cast<float>((static_cast<uint16_t>(b3) << 8) | b4) / 160.0f;
      const float celsius = (fahrenheit - 32.0f) * 5.0f / 9.0f;
      if (!validTemperature(celsius)) {
        result.status = DavisDecodeStatus::InvalidValue;
        return result;
      }
      observation.set(DavisField::Temperature, celsius);
      break;
    }
    case DavisMessageType::GustSpeed:
      observation.set(DavisField::WindGust,
                      static_cast<float>(b3) * kMphToMetresPerSecond);
      break;
    case DavisMessageType::Humidity: {
      const uint16_t raw =
          static_cast<uint16_t>((static_cast<uint16_t>(b4 >> 4) << 8) | b3);
      const float humidity = static_cast<float>(raw) / 10.0f;
      if (!validHumidity(humidity)) {
        result.status = DavisDecodeStatus::InvalidValue;
        return result;
      }
      observation.set(DavisField::Humidity, humidity);
      break;
    }
    case DavisMessageType::RainCounter:
      // The rain counter is a 7-bit rolling tip count, not a distance.
      observation.rainCounter = static_cast<uint8_t>(b3 & 0x7Fu);
      observation.rainCounterValid = true;
      break;
    case DavisMessageType::Unknown3:
    default:
      break;
  }

  result.status = DavisDecodeStatus::Ok;
  return result;
}

}  // namespace akashavani
