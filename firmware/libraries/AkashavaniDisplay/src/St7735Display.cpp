#include "St7735Display.h"

#include "Log.h"

namespace akashavani {

St7735Display::St7735Display(SPIClass& spi, const St7735Config& config)
    : spi_(spi), config_(config) {}

Status St7735Display::begin() {
  pinMode(config_.csPin, OUTPUT);
  digitalWrite(config_.csPin, HIGH);
  pinMode(config_.dcPin, OUTPUT);
  pinMode(config_.resetPin, OUTPUT);
  spi_.begin(config_.sckPin, config_.misoPin, config_.mosiPin, config_.csPin);
  ready_ = false;
  AK_LOG_WARN("display", "ST7735 driver is a stub; panel will stay blank");
  // TODO(display): reset, init sequence, orientation, backlight, framebuffer.
  return Status::Unsupported;
}

void St7735Display::render(const DisplayState& state) {
  (void)state;
  // TODO(display): draw title/lines/status. Do not block the receive loop.
}

void St7735Display::loop() {}

}  // namespace akashavani
