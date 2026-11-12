#pragma once

#include <cstddef>
#include <cstdint>

namespace akashvani {

// Davis ISS CRC-16/CCITT, initial value 0, polynomial 0x1021.
//
// This is the routine from the validated prototype
// (prototype/lib2/DavisRFM69/DavisRFM69.cpp, compute_crc16). It is computed over
// the six data bytes, most-significant byte first; the two CRC bytes are sent
// after them in the same order. Verified against captured frames, e.g.
// 87 05 2D 32 5B 0F -> B9 91.
inline uint16_t davisCrc16(const uint8_t* data, size_t length) {
  uint16_t crc = 0;
  for (size_t i = 0; i < length; ++i) {
    crc ^= static_cast<uint16_t>(data[i]) << 8;
    for (int bit = 0; bit < 8; ++bit) {
      if ((crc & 0x8000u) != 0u) {
        crc = static_cast<uint16_t>((crc << 1) ^ 0x1021u);
      } else {
        crc = static_cast<uint16_t>(crc << 1);
      }
    }
  }
  return crc;
}

}  // namespace akashvani
