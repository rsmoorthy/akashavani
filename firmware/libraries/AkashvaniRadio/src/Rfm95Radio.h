#pragma once

#include <Arduino.h>
#include <SPI.h>

#include "IRadio.h"
#include "RadioConfig.h"
#include "RadioFrame.h"

namespace akashvani {

// Minimal RFM95 (SX1276) SPI driver seam.
//
// The scaffold validates the bus and chip identity. The modem configuration,
// FIFO handling, IRQ mapping and raw mode expected by the Davis protocol must
// be ported from the prototype inside this class.
class Rfm95Radio final : public IRadio {
 public:
  Rfm95Radio(SPIClass& spi, const RadioPins& pins);

  Status begin(const RadioConfig& config) override;
  Status receive(RadioFrame& frame, uint32_t timeoutMs) override;
  Status transmit(const uint8_t* data, size_t length) override;

  bool isReady() const override { return ready_; }
  const char* name() const override { return "rfm95"; }

  uint8_t versionRegister() const { return version_; }
  uint16_t readRegisterValue(uint8_t address);

 private:
  uint8_t readRegister(uint8_t address);
  void writeRegister(uint8_t address, uint8_t value);
  void select();
  void deselect();

  SPIClass& spi_;
  RadioPins pins_;
  RadioConfig config_;
  bool ready_ = false;
  uint8_t version_ = 0;
};

}  // namespace akashvani
