#pragma once

#include <Arduino.h>
#include <SPI.h>

#include "IRadio.h"
#include "RadioConfig.h"
#include "RadioFrame.h"

namespace akashvani {

// RFM69 (SX1231) driver seam.
//
// The board uses an RFM95 today; the RFM69 variant is planned. This class
// exists so the rest of the firmware can depend on IRadio, not on a chip. Port
// the register/modem implementation here, preferably by reusing the prototype's
// LowPowerLab RFM69 setup or a direct SX1231 driver.
class Rfm69Radio final : public IRadio {
 public:
  Rfm69Radio(SPIClass& spi, const RadioPins& pins);

  Status begin(const RadioConfig& config) override;
  Status receive(RadioFrame& frame, uint32_t timeoutMs) override;
  Status transmit(const uint8_t* data, size_t length) override;

  bool isReady() const override { return ready_; }
  const char* name() const override { return "rfm69"; }

 private:
  SPIClass& spi_;
  RadioPins pins_;
  bool ready_ = false;
};

}  // namespace akashvani
