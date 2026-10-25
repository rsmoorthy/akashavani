#pragma once

#include <Arduino.h>
#include <RTClib.h>
#include <Wire.h>

#include "IClock.h"

namespace akashvani {

class Ds3231Clock final : public IClock {
 public:
  Ds3231Clock(TwoWire& wire, uint8_t sdaPin, uint8_t sclPin, uint8_t address = 0x68);

  Status begin() override;
  bool isValid() const override;
  int64_t nowEpoch() override;
  Status setEpoch(int64_t epochSeconds) override;
  const char* name() const override { return "ds3231"; }

  bool lostPower() const { return lostPower_; }
  float temperatureC();

 private:
  TwoWire& wire_;
  mutable RTC_DS3231 rtc_;
  uint8_t sdaPin_;
  uint8_t sclPin_;
  uint8_t address_;
  bool present_ = false;
  bool lostPower_ = false;
};

}  // namespace akashvani
