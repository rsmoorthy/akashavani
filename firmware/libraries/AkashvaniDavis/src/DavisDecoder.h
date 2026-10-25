#pragma once

#include <cstddef>
#include <cstdint>

#include "DavisObservation.h"

namespace akashvani {

enum class DavisDecodeStatus : uint8_t {
  Ok = 0,
  TooShort,
  TooLong,
  CrcMismatch,
  UnsupportedProtocol,
  InvalidValue,
};

const char* toString(DavisDecodeStatus status);

struct DavisDecodeResult {
  DavisDecodeStatus status = DavisDecodeStatus::UnsupportedProtocol;
  DavisObservation observation;
};

// Decodes one raw radio frame. The caller supplies the receive timestamp and
// transmitter metadata when known.
DavisDecodeResult decodeDavisFrame(const uint8_t* frame, size_t length,
                                   uint32_t receivedAtMs = 0);

}  // namespace akashvani
