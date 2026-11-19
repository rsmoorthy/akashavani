#pragma once

#include <cstddef>
#include <cstdint>

namespace akashavani {

// The Davis ISS transmits a fixed eight-byte frame: six data bytes followed by
// a two-byte CRC-16/CCITT. The RFM69 is configured for exactly this length.
constexpr size_t kDavisFrameBytes = 8;
constexpr size_t kDavisMinFrameBytes = kDavisFrameBytes;
constexpr size_t kDavisMaxFrameBytes = kDavisFrameBytes;
constexpr uint8_t kDavisSyncByte1 = 0xCB;
constexpr uint8_t kDavisSyncByte2 = 0x89;

// Davis transmitter IDs are commonly 1..8. The simulator exposes one ID.
constexpr uint8_t kDavisMinTransmitterId = 1;
constexpr uint8_t kDavisMaxTransmitterId = 8;

// TODO(protocol): document frequency, deviation, bitrate, sync word, framing,
// CRC and hopping sequence here once verified against the prototype.
constexpr float kDavisDefaultFrequencyMhz = 915.0f;

}  // namespace akashavani
