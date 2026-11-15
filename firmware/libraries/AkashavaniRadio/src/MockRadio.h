#pragma once

#include <cstddef>
#include <cstdint>

#include "IRadio.h"

namespace akashavani {

// In-memory IRadio used by native tests and by the transmitter simulator's
// dry-run mode. It has no Arduino dependencies.
class MockRadio final : public IRadio {
 public:
  static constexpr size_t kQueueDepth = 4;
  static constexpr size_t kTransmitHistory = 4;

  Status begin(const RadioConfig& config) override {
    config_ = config;
    ready_ = true;
    return Status::Ok;
  }

  Status receive(RadioFrame& frame, uint32_t timeoutMs) override {
    (void)timeoutMs;
    if (!ready_) {
      return Status::NotReady;
    }
    if (queued_ == 0) {
      return Status::Timeout;
    }
    frame = queue_[head_];
    head_ = (head_ + 1) % kQueueDepth;
    --queued_;
    return Status::Ok;
  }

  Status transmit(const uint8_t* data, size_t length) override {
    if (!ready_) {
      return Status::NotReady;
    }
    if (data == nullptr || length == 0 || length > kMaxRadioFrameBytes) {
      return Status::InvalidArgument;
    }
    RadioFrame& slot = transmitted_[transmitHead_];
    for (size_t i = 0; i < length; ++i) {
      slot.data[i] = data[i];
    }
    slot.length = length;
    transmitHead_ = (transmitHead_ + 1) % kTransmitHistory;
    if (transmitCount_ < kTransmitHistory) {
      ++transmitCount_;
    }
    return Status::Ok;
  }

  bool isReady() const override { return ready_; }
  const char* name() const override { return "mock"; }

  bool queueFrame(const uint8_t* data, size_t length, int16_t rssiDbm = -70,
                  uint32_t receivedAtMs = 0) {
    if (data == nullptr || length == 0 || length > kMaxRadioFrameBytes ||
        queued_ == kQueueDepth) {
      return false;
    }
    const size_t tail = (head_ + queued_) % kQueueDepth;
    RadioFrame& slot = queue_[tail];
    for (size_t i = 0; i < length; ++i) {
      slot.data[i] = data[i];
    }
    slot.length = length;
    slot.rssiDbm = rssiDbm;
    slot.receivedAtMs = receivedAtMs;
    ++queued_;
    return true;
  }

  size_t transmitCount() const { return transmitCount_; }

  const RadioFrame& lastTransmission() const {
    const size_t index = (transmitHead_ + kTransmitHistory - 1) % kTransmitHistory;
    return transmitted_[index];
  }

 private:
  RadioConfig config_;
  bool ready_ = false;

  RadioFrame queue_[kQueueDepth];
  size_t head_ = 0;
  size_t queued_ = 0;

  RadioFrame transmitted_[kTransmitHistory];
  size_t transmitHead_ = 0;
  size_t transmitCount_ = 0;
};

}  // namespace akashavani
