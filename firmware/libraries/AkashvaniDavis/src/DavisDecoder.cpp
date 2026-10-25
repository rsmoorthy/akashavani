#include "DavisDecoder.h"

#include "DavisConstants.h"

namespace akashvani {

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

DavisDecodeResult decodeDavisFrame(const uint8_t* frame, size_t length,
                                   uint32_t receivedAtMs) {
  (void)receivedAtMs;
  DavisDecodeResult result;
  if (frame == nullptr || length == 0) {
    result.status = DavisDecodeStatus::TooShort;
    return result;
  }
  if (length < kDavisMinFrameBytes) {
    result.status = DavisDecodeStatus::TooShort;
    return result;
  }
  if (length > kDavisMaxFrameBytes) {
    result.status = DavisDecodeStatus::TooLong;
    return result;
  }

  // TODO(protocol): port the validated prototype here.
  //   - verify CRC / sync
  //   - extract temperature, humidity, pressure, wind, rain, UV, solar, battery
  //   - set the corresponding DavisField bits
  //   - return InvalidValue for out-of-range readings rather than silently
  //     publishing bad weather data.
  result.status = DavisDecodeStatus::UnsupportedProtocol;
  return result;
}

}  // namespace akashvani
