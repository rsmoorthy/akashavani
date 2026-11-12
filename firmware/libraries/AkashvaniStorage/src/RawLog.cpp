#include "RawLog.h"

#include <cstdio>
#include <cstring>
#include <ctime>

#include "Log.h"

namespace akashvani {
namespace {

constexpr int64_t kIstOffsetSeconds = 5 * 3600 + 30 * 60;  // +05:30, no DST

}  // namespace

RawLog::RawLog(const RawLogConfig& config) : config_(config) {}

const char* RawLog::weekdayName(int weekday) {
  switch (weekday) {
    case 0:
      return "sunday";
    case 1:
      return "monday";
    case 2:
      return "tuesday";
    case 3:
      return "wednesday";
    case 4:
      return "thursday";
    case 5:
      return "friday";
    case 6:
      return "saturday";
  }
  return "unknown";
}

Status RawLog::begin(int64_t epochSeconds) {
  ready_ = false;
  count_ = 0;
  written_ = 0;
  currentWeekday_ = -1;
  if (epochSeconds <= 0) {
    return Status::InvalidArgument;
  }
  if (!SD.exists(config_.directory)) {
    if (!SD.mkdir(config_.directory)) {
      AK_LOG_ERROR("raw", "mkdir failed: %s", config_.directory);
      return Status::IoError;
    }
  }
  ready_ = true;
  lastFlushMs_ = millis();
  return Status::Ok;
}

void RawLog::formatIst(int64_t epochSeconds, char* output, size_t size) {
  const time_t ist = static_cast<time_t>(epochSeconds + kIstOffsetSeconds);
  struct tm parts = {};
  gmtime_r(&ist, &parts);
  std::snprintf(output, size, "%04d-%02d-%02dT%02d:%02d:%02d+05:30", parts.tm_year + 1900,
                parts.tm_mon + 1, parts.tm_mday, parts.tm_hour, parts.tm_min, parts.tm_sec);
}

bool RawLog::ensureFile(int64_t epochSeconds) {
  const time_t ist = static_cast<time_t>(epochSeconds + kIstOffsetSeconds);
  struct tm parts = {};
  gmtime_r(&ist, &parts);
  if (parts.tm_wday == currentWeekday_ && file_) {
    return true;
  }
  if (file_) {
    file_.close();
  }
  char path[64];
  std::snprintf(path, sizeof(path), "%s/raw-data-%s", config_.directory,
                weekdayName(parts.tm_wday));
  // Truncate when the weekday changes: the same weekday next week replaces the
  // previous week's file for that day.
  file_ = SD.open(path, FILE_WRITE);
  if (!file_) {
    AK_LOG_ERROR("raw", "open failed: %s", path);
    ready_ = false;
    return false;
  }
  currentWeekday_ = parts.tm_wday;
  return true;
}

void RawLog::append(int64_t epochSeconds, const uint8_t* frame, size_t length,
                    int16_t rssiDbm, uint8_t channel) {
  if (!ready_ || frame == nullptr || length == 0 || length > 8 || epochSeconds <= 0) {
    return;
  }
  if (!ensureFile(epochSeconds)) {
    return;
  }
  if (count_ >= config_.capacity || count_ >= 64) {
    flush();
    if (!ready_) {
      return;
    }
  }
  Entry& entry = entries_[count_++];
  entry.epoch = epochSeconds;
  std::memcpy(entry.frame, frame, length);
  entry.length = static_cast<uint8_t>(length);
  entry.rssi = rssiDbm;
  entry.channel = channel;
}

Status RawLog::flush() {
  if (!ready_ || !file_) {
    return Status::NotReady;
  }
  for (uint16_t i = 0; i < count_; ++i) {
    const Entry& entry = entries_[i];
    char stamp[32];
    formatIst(entry.epoch, stamp, sizeof(stamp));
    file_.print(stamp);
    file_.print(',');
    for (uint8_t b = 0; b < entry.length; ++b) {
      file_.printf("%02X%s", entry.frame[b], b + 1 == entry.length ? "" : " ");
    }
    file_.printf(",%d,%u\n", static_cast<int>(entry.rssi),
                 static_cast<unsigned>(entry.channel));
    ++written_;
  }
  count_ = 0;
  file_.flush();
  lastFlushMs_ = millis();
  return Status::Ok;
}

void RawLog::loop(uint32_t nowMs) {
  if (!ready_) {
    return;
  }
  if (count_ > 0 && nowMs - lastFlushMs_ >= config_.flushIntervalMs) {
    flush();
  }
}

}  // namespace akashvani
