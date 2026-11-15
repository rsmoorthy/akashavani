#include "RawLog.h"

#include <cstdio>
#include <cstring>
#include <ctime>

#include "Log.h"

namespace akashvani {
namespace {

int64_t daysFromCivil(int year, unsigned month, unsigned day) {
  year -= month <= 2;
  const int era = (year >= 0 ? year : year - 399) / 400;
  const unsigned yearOfEra = static_cast<unsigned>(year - era * 400);
  const unsigned dayOfYear = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
  const unsigned dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
  return static_cast<int64_t>(era) * 146097 + static_cast<int64_t>(dayOfEra) - 719468;
}

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

void RawLog::formatUtc(int64_t epochSeconds, char* output, size_t size) {
  const time_t utc = static_cast<time_t>(epochSeconds);
  struct tm parts = {};
  gmtime_r(&utc, &parts);
  std::snprintf(output, size, "%04d-%02d-%02dT%02d:%02d:%02dZ", parts.tm_year + 1900,
                parts.tm_mon + 1, parts.tm_mday, parts.tm_hour, parts.tm_min, parts.tm_sec);
}

bool RawLog::ensureFile(int64_t epochSeconds) {
  const time_t utc = static_cast<time_t>(epochSeconds);
  struct tm parts = {};
  gmtime_r(&utc, &parts);
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
    formatUtc(entry.epoch, stamp, sizeof(stamp));
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

Status RawLog::beginReplayRange(int64_t startEpoch, int64_t endEpoch) {
  if (!ready_ || startEpoch <= 0 || endEpoch < startEpoch) {
    return Status::InvalidArgument;
  }
  replay_ = true;
  replayStart_ = startEpoch;
  replayEnd_ = endEpoch;
  replayWeekday_ = 0;
  if (replayFile_) {
    replayFile_.close();
  }
  return Status::Ok;
}

bool RawLog::nextReplayRecord(Record& record) {
  if (!replay_) {
    return false;
  }
  char path[64];
  while (replayWeekday_ < 7) {
    if (!replayFile_) {
      std::snprintf(path, sizeof(path), "%s/raw-data-%s", config_.directory,
                    weekdayName(replayWeekday_));
      replayFile_ = SD.open(path, FILE_READ);
      ++replayWeekday_;
      if (!replayFile_) {
        continue;
      }
    }
    size_t length = 0;
    bool gotLine = false;
    while (replayFile_.available()) {
      const int raw = replayFile_.read();
      if (raw < 0) {
        break;
      }
      const char c = static_cast<char>(raw);
      if (c == '\n') {
        gotLine = true;
        break;
      }
      if (c != '\r' && length + 1 < sizeof(replayLine_)) {
        replayLine_[length++] = c;
      }
    }
    replayLine_[length] = '\0';
    if (!gotLine && length == 0) {
      replayFile_.close();
      continue;
    }
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;
    if (std::sscanf(replayLine_, "%d-%d-%dT%d:%d:%dZ,", &year, &month, &day, &hour,
                    &minute, &second) != 6) {
      continue;
    }
    const int64_t epoch =
        daysFromCivil(year, static_cast<unsigned>(month), static_cast<unsigned>(day)) * 86400 +
        hour * 3600 + minute * 60 + second;
    if (epoch < replayStart_ || epoch > replayEnd_) {
      continue;
    }
    const char* fields = std::strchr(replayLine_, ',');
    if (fields == nullptr) {
      continue;
    }
    ++fields;
    const char* lastComma = std::strrchr(fields, ',');
    if (lastComma == nullptr) {
      continue;
    }
    const char* rssiStart = lastComma;
    while (rssiStart > fields && *(rssiStart - 1) != ',') {
      --rssiStart;
    }
    if (rssiStart == fields) {
      continue;
    }
    unsigned bytes[8] = {};
    const int parsed = std::sscanf(fields, "%2X %2X %2X %2X %2X %2X %2X %2X", &bytes[0],
                                   &bytes[1], &bytes[2], &bytes[3], &bytes[4], &bytes[5],
                                   &bytes[6], &bytes[7]);
    if (parsed <= 0) {
      continue;
    }
    record.epoch = epoch;
    record.length = static_cast<uint8_t>(parsed);
    for (int i = 0; i < parsed && i < 8; ++i) {
      record.frame[i] = static_cast<uint8_t>(bytes[i]);
    }
    record.rssi = static_cast<int16_t>(std::atoi(rssiStart));
    record.channel = static_cast<uint8_t>(std::atoi(lastComma + 1));
    return true;
  }
  replayFile_.close();
  replay_ = false;
  return false;
}

void RawLog::endReplay() {
  if (replayFile_) {
    replayFile_.close();
  }
  replay_ = false;
}

}  // namespace akashvani
