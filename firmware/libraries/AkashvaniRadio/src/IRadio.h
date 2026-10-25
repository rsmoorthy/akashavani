#pragma once

#include <cstddef>
#include <cstdint>

#include "RadioConfig.h"
#include "RadioFrame.h"
#include "Status.h"

namespace akashvani {

class IRadio {
 public:
  virtual ~IRadio() = default;

  virtual Status begin(const RadioConfig& config) = 0;

  // Waits up to timeoutMs for one frame. Returns Timeout when no frame arrived.
  virtual Status receive(RadioFrame& frame, uint32_t timeoutMs) = 0;

  // Sends one complete frame. The simulator uses this; the receiver may leave
  // it unsupported.
  virtual Status transmit(const uint8_t* data, size_t length) = 0;

  virtual bool isReady() const = 0;
  virtual const char* name() const = 0;
};

}  // namespace akashvani
