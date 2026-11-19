#pragma once

#include <cstddef>
#include <cstdint>

#include "DavisDecoder.h"
#include "DavisObservation.h"

namespace akashavani {

enum class DavisEncodeStatus : uint8_t {
  Ok = 0,
  BufferTooSmall,
  UnsupportedProtocol,
  InvalidObservation,
};

const char* toString(DavisEncodeStatus status);

// Encodes one Davis ISS message. Davis frames are always eight bytes: six data
// bytes followed by the CRC-16/CCITT. `testSequence` is written to byte 5, which
// is unused by the decoded weather fields; the bench simulator uses it to count
// slots. Set it to zero for captured/real-looking frames.
DavisEncodeStatus encodeDavisFrame(const DavisObservation& observation,
                                   DavisMessageType messageType, uint8_t* output,
                                   size_t outputSize, size_t& written,
                                   uint8_t testSequence = 0);

// Convenience overload used by the generic simulator: picks the first available
// message-specific field in the observation.
DavisEncodeStatus encodeDavisFrame(const DavisObservation& observation, uint8_t* output,
                                   size_t outputSize, size_t& written);

}  // namespace akashavani
