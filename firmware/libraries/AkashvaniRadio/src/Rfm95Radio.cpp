#include "Rfm95Radio.h"

#include "Log.h"

namespace akashvani {
namespace {

constexpr uint8_t kRegVersion = 0x42;
constexpr uint8_t kRfm95Version = 0x12;
constexpr uint8_t kRegOpMode = 0x01;
constexpr uint8_t kModeSleep = 0x00;
constexpr uint8_t kModeStandby = 0x01;

}  // namespace

Rfm95Radio::Rfm95Radio(SPIClass& spi, const RadioPins& pins) : spi_(spi), pins_(pins) {}

uint16_t Rfm95Radio::readRegisterValue(uint8_t address) { return readRegister(address); }

uint8_t Rfm95Radio::readRegister(uint8_t address) {
  select();
  spi_.transfer(address & 0x7F);
  const uint8_t value = spi_.transfer(0x00);
  deselect();
  return value;
}

void Rfm95Radio::writeRegister(uint8_t address, uint8_t value) {
  select();
  spi_.transfer(address | 0x80);
  spi_.transfer(value);
  deselect();
}

void Rfm95Radio::select() {
  spi_.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
  digitalWrite(pins_.cs, LOW);
}

void Rfm95Radio::deselect() {
  digitalWrite(pins_.cs, HIGH);
  spi_.endTransaction();
}

Status Rfm95Radio::begin(const RadioConfig& config) {
  config_ = config;
  pinMode(pins_.cs, OUTPUT);
  digitalWrite(pins_.cs, HIGH);
  pinMode(pins_.reset, OUTPUT);
  pinMode(pins_.dio0, INPUT);

  spi_.begin(pins_.sck, pins_.miso, pins_.mosi, pins_.cs);

  digitalWrite(pins_.reset, LOW);
  delay(10);
  digitalWrite(pins_.reset, HIGH);
  delay(10);

  version_ = readRegister(kRegVersion);
  if (version_ != kRfm95Version) {
    AK_LOG_ERROR("radio", "RFM95 not found (version=0x%02x)", version_);
    ready_ = false;
    return Status::NotFound;
  }

  // A standby/sleep transition proves that register writes reach the device.
  writeRegister(kRegOpMode, kModeSleep);
  delay(2);
  writeRegister(kRegOpMode, kModeStandby);
  delay(2);

  AK_LOG_WARN("radio",
              "RFM95 detected but modem setup is not ported; receiver will stay idle");
  ready_ = false;
  // TODO(port): configure frequency, modulation, bitrate, deviation, bandwidth,
  // sync/preamble, FIFO and IRQs from the validated prototype. Return
  // Status::Ok only when receive() can actually deliver frames.
  return Status::Unsupported;
}

Status Rfm95Radio::receive(RadioFrame& frame, uint32_t timeoutMs) {
  (void)frame;
  (void)timeoutMs;
  // TODO(port): wait for DIO0/RX_DONE, read FIFO, fill frame, signal strength.
  return Status::Unsupported;
}

Status Rfm95Radio::transmit(const uint8_t* data, size_t length) {
  (void)data;
  (void)length;
  // TODO(port): only needed by the transmitter simulator board.
  return Status::Unsupported;
}

}  // namespace akashvani
