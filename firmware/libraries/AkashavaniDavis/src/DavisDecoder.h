#pragma once

#include <cstddef>
#include <cstdint>

#include "DavisObservation.h"

namespace akashavani {

enum class DavisDecodeStatus : uint8_t {
  Ok = 0,
  TooShort,
  TooLong,
  CrcMismatch,
  UnsupportedProtocol,
  InvalidValue,
};

const char* toString(DavisDecodeStatus status);

// Davis ISS message IDs. Each frame repeats wind speed, wind direction and
// battery state and adds one message-specific field.
enum class DavisMessageType : uint8_t {
  GoldcapVoltage = 0x2,
  Unknown3 = 0x3,
  // 0x4 and 0x6 are the VP2 UV and solar messages. Formulas and missing-sensor
  // sentinels are taken from the kobuki/VPTools reference
  // (Examples/ISSRx/ISSRx.ino, DavisRFM69.h). They are not yet verified against
  // a local console; see docs/specs/rf-davis-receive.md.
  UvIndex = 0x4,
  RainRate = 0x5,
  SolarRadiationWm2 = 0x6,
  // 0x7 is the legacy prototype mapping, retained unchanged until the field
  // comparison. The reference maps 0x7 to the Vue solar panel voltage, not to
  // VP2 solar radiation.
  SolarRadiation = 0x7,
  Temperature = 0x8,
  GustSpeed = 0x9,
  Humidity = 0xA,
  RainCounter = 0xE,
};

const char* toString(DavisMessageType type);

struct DavisDecodeResult {
  DavisDecodeStatus status = DavisDecodeStatus::UnsupportedProtocol;
  DavisObservation observation;
  DavisMessageType messageType = DavisMessageType::Unknown3;
  uint8_t messageId = 0;
  bool batteryLow = false;
  // The validated Davis byte 5 is not used by the decoded weather fields. The
  // bench simulator puts a wrapping test sequence there; real captures may show
  // any value, so this is diagnostic only.
  uint8_t benchSequence = 0;
};

// Decodes one raw radio frame. The caller supplies the receive timestamp.
DavisDecodeResult decodeDavisFrame(const uint8_t* frame, size_t length,
                                   uint32_t receivedAtMs = 0);

}  // namespace akashavani
