#pragma once

#include <cstddef>
#include <cstdint>

namespace akashvani {

// Bounds used by the decoder before handing bytes to the protocol-specific
// parser. Adjust once the real frame sizes are confirmed.
constexpr size_t kDavisMinFrameBytes = 8;
constexpr size_t kDavisMaxFrameBytes = 64;

// Davis transmitter IDs are commonly 1..8. The simulator exposes one ID.
constexpr uint8_t kDavisMinTransmitterId = 1;
constexpr uint8_t kDavisMaxTransmitterId = 8;

// TODO(protocol): document frequency, deviation, bitrate, sync word, framing,
// CRC and hopping sequence here once verified against the prototype.
constexpr float kDavisDefaultFrequencyMhz = 915.0f;

}  // namespace akashvani
