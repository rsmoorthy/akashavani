#pragma once

#include <cstddef>
#include <cstdint>

#include "DavisObservation.h"

namespace akashvani {

enum class DavisEncodeStatus : uint8_t {
  Ok = 0,
  BufferTooSmall,
  UnsupportedProtocol,
  InvalidObservation,
};

const char* toString(DavisEncodeStatus status);

// Encodes one observation into a raw radio frame for the transmitter simulator.
// On success `written` contains the number of bytes placed in `output`.
DavisEncodeStatus encodeDavisFrame(const DavisObservation& observation, uint8_t* output,
                                   size_t outputSize, size_t& written);

}  // namespace akashvani
