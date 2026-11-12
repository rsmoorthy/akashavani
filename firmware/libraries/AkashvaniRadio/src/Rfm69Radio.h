#pragma once

#include <Arduino.h>
#include <SPI.h>

#include <cstddef>
#include <cstdint>

#include "IRadio.h"
#include "RadioConfig.h"
#include "RadioFrame.h"

namespace akashvani {

// RFM69 (SX1231/SX1231H) driver for the Davis ISS receive/transmit path.
//
// Configuration is taken from the validated prototype in
// prototype/lib2/DavisRFM69. Keep the modem values, sync bytes and fixed
// eight-byte payload in step with that code and with the transmitter firmware.
// The RFM69 sends/receives the least-significant bit of each byte first; the
// prototype stores Davis bytes reversed on air, so this driver reverses each
// byte on transmit and receive.
class Rfm69Radio final : public IRadio {
 public:
  Rfm69Radio(SPIClass& spi, const RadioPins& pins);

  Status begin(const RadioConfig& config) override;
  Status receive(RadioFrame& frame, uint32_t timeoutMs) override;
  Status transmit(const uint8_t* data, size_t length) override;

  bool isReady() const override { return ready_; }
  const char* name() const override { return "rfm69"; }

  // Davis channel index (0..50). Writing the channel restarts RX.
  Status setChannel(uint8_t channel);
  uint8_t channel() const { return channel_; }
  Status hop();
  Status hopBack();

  // Diagnostic register access; useful for the bench bring-up sketches.
  uint8_t readRegister(uint8_t address);
  void writeRegister(uint8_t address, uint8_t value);
  int16_t readRssiDbm();

 private:
  void select();
  void unselect();
  uint8_t readReg(uint8_t address);
  void writeReg(uint8_t address, uint8_t value);
  void setModeStandby();
  void setModeRx();
  void setModeTx();
  void restartRx();
  void applyChannel(uint8_t channel);
  void configurePower();
  void configureModem();
  static uint8_t reverseBits(uint8_t value);

  SPIClass& spi_;
  RadioPins pins_;
  RadioConfig config_;
  bool ready_ = false;
  bool highPower_ = true;
  uint8_t channel_ = 0;
  uint8_t payloadLength_ = 8;
  int8_t powerDbm_ = 17;
};

}  // namespace akashvani
