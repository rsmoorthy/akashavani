#pragma once

#include <cstdint>

#include "IClock.h"

namespace akashvani {

// A settable clock for native tests and the simulator. Host-safe.
class FixedClock final : public IClock {
 public:
  explicit FixedClock(int64_t epochSeconds = 0) : epochSeconds_(epochSeconds) {}

  Status begin() override { return Status::Ok; }
  bool isValid() const override { return epochSeconds_ >= 946684800; }
  int64_t nowEpoch() override { return epochSeconds_; }
  Status setEpoch(int64_t epochSeconds) override {
    epochSeconds_ = epochSeconds;
    return Status::Ok;
  }
  const char* name() const override { return "fixed"; }

  void advance(int64_t seconds) { epochSeconds_ += seconds; }

 private:
  int64_t epochSeconds_;
};

}  // namespace akashvani
