#pragma once

#include <Arduino.h>

#include <cstdint>

#include "IClock.h"

namespace akashvani {

struct TimeServiceConfig {
  const char* ntpServer1 = "pool.ntp.org";
  const char* ntpServer2 = "time.cloudflare.com";
  uint32_t syncTimeoutMs = 15000;
  bool writeBackToRtc = true;
};

class TimeService {
 public:
  explicit TimeService(const TimeServiceConfig& config = TimeServiceConfig());

  Status begin(IClock& rtc);
  Status syncFromNtp();

  int64_t nowEpoch();
  bool isValid();
  const char* source() const;

 private:
  bool seedSystemTimeFromRtc();
  bool systemTimeValid() const;

  TimeServiceConfig config_;
  IClock* rtc_ = nullptr;
  bool ntpConfigured_ = false;
  bool ntpSynced_ = false;
  bool systemSeeded_ = false;
};

}  // namespace akashvani
