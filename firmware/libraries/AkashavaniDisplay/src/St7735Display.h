#pragma once

#include <Arduino.h>
#include <SPI.h>

#include "IDisplay.h"

namespace akashavani {

struct St7735Config {
  uint8_t csPin = 8;
  uint8_t dcPin = 7;
  uint8_t resetPin = 6;
  uint8_t sckPin = 12;
  uint8_t misoPin = 13;
  uint8_t mosiPin = 11;
  uint16_t width = 80;
  uint16_t height = 160;
  uint8_t rotation = 0;
};

// Driver seam for the 1" ST7735-class panel.
//
// The panel is disabled by default. Port the proven Adafruit_ST7735/LovyanGFX
// setup here; keep the application's DisplayState model unchanged. A future 3.5"
// touch implementation can expose the same IDisplay interface.
class St7735Display final : public IDisplay {
 public:
  St7735Display(SPIClass& spi, const St7735Config& config);

  Status begin() override;
  void render(const DisplayState& state) override;
  void loop() override;
  const char* name() const override { return "st7735"; }

 private:
  SPIClass& spi_;
  St7735Config config_;
  bool ready_ = false;
};

}  // namespace akashavani
