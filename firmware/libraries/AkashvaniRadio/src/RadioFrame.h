#pragma once

#include <cstddef>
#include <cstdint>

namespace akashvani {

constexpr size_t kMaxRadioFrameBytes = 256;

struct RadioFrame {
  uint8_t data[kMaxRadioFrameBytes] = {};
  size_t length = 0;
  int16_t rssiDbm = 0;
  uint32_t receivedAtMs = 0;
};

}  // namespace akashvani
