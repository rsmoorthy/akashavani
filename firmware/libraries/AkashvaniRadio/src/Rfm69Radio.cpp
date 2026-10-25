#include "Rfm69Radio.h"

#include "Log.h"

namespace akashvani {

Rfm69Radio::Rfm69Radio(SPIClass& spi, const RadioPins& pins) : spi_(spi), pins_(pins) {}

Status Rfm69Radio::begin(const RadioConfig& config) {
  (void)config;
  pinMode(pins_.cs, OUTPUT);
  digitalWrite(pins_.cs, HIGH);
  pinMode(pins_.reset, OUTPUT);
  pinMode(pins_.dio0, INPUT);
  spi_.begin(pins_.sck, pins_.miso, pins_.mosi, pins_.cs);

  AK_LOG_WARN("radio", "RFM69 driver is a planned stub; no frames will be received");
  ready_ = false;
  // TODO(port): configure SX1231 frequency/modulation/bitrate/sync/FIFO/IRQs.
  return Status::Unsupported;
}

Status Rfm69Radio::receive(RadioFrame& frame, uint32_t timeoutMs) {
  (void)frame;
  (void)timeoutMs;
  return Status::Unsupported;
}

Status Rfm69Radio::transmit(const uint8_t* data, size_t length) {
  (void)data;
  (void)length;
  return Status::Unsupported;
}

}  // namespace akashvani
