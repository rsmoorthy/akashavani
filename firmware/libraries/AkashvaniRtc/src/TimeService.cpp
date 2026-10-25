#include "TimeService.h"

#include <sys/time.h>
#include <time.h>

#include "Log.h"

namespace akashvani {
namespace {
constexpr int64_t kMinValidEpoch = 946684800;  // 2000-01-01
}

TimeService::TimeService(const TimeServiceConfig& config) : config_(config) {}

Status TimeService::begin(IClock& rtc) {
  rtc_ = &rtc;
  if (seedSystemTimeFromRtc()) {
    return Status::Ok;
  }
  AK_LOG_WARN("time", "no valid RTC time; waiting for SNTP");
  return Status::NotReady;
}

bool TimeService::systemTimeValid() const {
  const time_t now = time(nullptr);
  return static_cast<int64_t>(now) >= kMinValidEpoch;
}

bool TimeService::seedSystemTimeFromRtc() {
  if (rtc_ == nullptr || !rtc_->isValid()) {
    return false;
  }
  const int64_t epoch = rtc_->nowEpoch();
  if (epoch < kMinValidEpoch) {
    return false;
  }
  timeval value{};
  value.tv_sec = static_cast<time_t>(epoch);
  settimeofday(&value, nullptr);
  systemSeeded_ = true;
  AK_LOG_INFO("time", "system time seeded from %s", rtc_->name());
  return true;
}

Status TimeService::syncFromNtp() {
  if (!ntpConfigured_) {
    configTime(0, 0, config_.ntpServer1, config_.ntpServer2);
    ntpConfigured_ = true;
  }

  const uint32_t started = millis();
  while (millis() - started < config_.syncTimeoutMs) {
    if (systemTimeValid()) {
      ntpSynced_ = true;
      break;
    }
    delay(250);
  }
  if (!ntpSynced_) {
    AK_LOG_WARN("time", "SNTP sync timed out");
    return Status::Timeout;
  }

  if (config_.writeBackToRtc && rtc_ != nullptr) {
    const int64_t epoch = static_cast<int64_t>(time(nullptr));
    if (isOk(rtc_->setEpoch(epoch))) {
      AK_LOG_INFO("time", "RTC updated from SNTP");
    }
  }
  AK_LOG_INFO("time", "SNTP synced");
  return Status::Ok;
}

int64_t TimeService::nowEpoch() {
  if (systemTimeValid()) {
    return static_cast<int64_t>(time(nullptr));
  }
  if (rtc_ != nullptr && rtc_->isValid()) {
    return rtc_->nowEpoch();
  }
  return 0;
}

bool TimeService::isValid() { return nowEpoch() >= kMinValidEpoch; }

const char* TimeService::source() const {
  if (systemTimeValid()) {
    return ntpSynced_ ? "sntp" : (systemSeeded_ ? "rtc" : "system");
  }
  return "invalid";
}

}  // namespace akashvani
