#pragma once

#include "IDisplay.h"

namespace akashavani {

// Used when no panel is fitted or the driver is not ready.
class NullDisplay final : public IDisplay {
 public:
  Status begin() override { return Status::Ok; }
  void render(const DisplayState& state) override { (void)state; }
  void loop() override {}
  const char* name() const override { return "none"; }
};

}  // namespace akashavani
