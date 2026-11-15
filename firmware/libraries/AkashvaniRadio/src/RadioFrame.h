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
  // UTC epoch captured when the frame was drained from the radio. Zero when
  // the clock was invalid at that moment.
  uint32_t receivedAtEpoch = 0;
  // Davis channel index the frame arrived on, when the driver knows it.
  uint8_t channel = 0;
};

}  // namespace akashvani
