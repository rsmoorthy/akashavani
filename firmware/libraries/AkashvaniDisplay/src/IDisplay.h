#pragma once

#include <cstdint>

#include "Status.h"

namespace akashvani {

struct DisplayState {
  char title[24] = "Akashvani";
  char line1[32] = {};
  char line2[32] = {};
  char line3[32] = {};
  bool wifiConnected = false;
  bool radioReady = false;
  bool timeValid = false;
  bool archiveReady = false;
  uint32_t queuedUploads = 0;
  uint32_t recordsToday = 0;
  int16_t rssiDbm = 0;
};

class IDisplay {
 public:
  virtual ~IDisplay() = default;

  virtual Status begin() = 0;
  virtual void render(const DisplayState& state) = 0;
  virtual void loop() = 0;
  virtual const char* name() const = 0;
};

}  // namespace akashvani
