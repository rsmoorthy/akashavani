#include "Ds3231Clock.h"

#include "Log.h"

namespace akashvani {

Ds3231Clock::Ds3231Clock(TwoWire& wire, uint8_t sdaPin, uint8_t sclPin, uint8_t address)
    : wire_(wire), sdaPin_(sdaPin), sclPin_(sclPin), address_(address) {}

Status Ds3231Clock::begin() {
  wire_.begin(sdaPin_, sclPin_);
  present_ = rtc_.begin(&wire_);
  if (!present_) {
    AK_LOG_ERROR("rtc", "DS3231 not found at 0x%02x", address_);
    return Status::NotFound;
  }
  lostPower_ = rtc_.lostPower();
  if (lostPower_) {
    AK_LOG_WARN("rtc", "DS3231 reports lost power; time is not trustworthy until sync");
  }
  AK_LOG_INFO("rtc", "DS3231 ready, temperature=%.2fC", rtc_.getTemperature());
  return Status::Ok;
}

bool Ds3231Clock::isValid() const {
  if (!present_ || lostPower_) {
    return false;
  }
  return rtc_.now().unixtime() >= 946684800UL;
}

int64_t Ds3231Clock::nowEpoch() { return static_cast<int64_t>(rtc_.now().unixtime()); }

Status Ds3231Clock::setEpoch(int64_t epochSeconds) {
  if (!present_ || epochSeconds < 0 || epochSeconds > 4102444800LL) {
    return Status::InvalidArgument;
  }
  rtc_.adjust(DateTime(static_cast<uint32_t>(epochSeconds)));
  lostPower_ = false;
  return Status::Ok;
}

float Ds3231Clock::temperatureC() {
  if (!present_) {
    return 0.0f;
  }
  return rtc_.getTemperature();
}

}  // namespace akashvani
