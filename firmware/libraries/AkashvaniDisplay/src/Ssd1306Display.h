#pragma once

#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <Wire.h>

#include "IDisplay.h"

namespace akashvani {

struct Ssd1306Config {
  uint8_t address = 0x3C;
  uint8_t sdaPin = 21;
  uint8_t sclPin = 22;
  uint16_t width = 128;
  uint16_t height = 64;
  int8_t resetPin = -1;
  uint32_t i2cFrequencyHz = 400000;
};

// SSD1306-class 128x64 monochrome OLED on the shared I2C bus.
//
// The bench panel answers at 0x3C on SDA 21 / SCL 22. It shares the bus with the
// DS3231 and BME280, so begin() only starts Wire if the application has not
// already done so. Rendering is deliberately plain text: the display is a
// status panel, not a UI. Keep drawing off the timing-critical RF path.
class Ssd1306Display final : public IDisplay {
 public:
  Ssd1306Display(TwoWire& wire, const Ssd1306Config& config);

  Status begin() override;
  void render(const DisplayState& state) override;
  void loop() override;
  const char* name() const override { return "ssd1306"; }

 private:
  void drawHeader();
  void drawRow(uint8_t row, const char* text);

  TwoWire& wire_;
  Ssd1306Config config_;
  Adafruit_SSD1306 driver_;
  bool ready_ = false;
};

}  // namespace akashvani
