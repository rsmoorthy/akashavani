#pragma once

#include <Arduino.h>
#include <SD.h>

#include <cstdint>

#include "Status.h"

namespace akashvani {

struct RawLogConfig {
  const char* directory = "/raw";
  uint32_t flushIntervalMs = 5UL * 60UL * 1000UL;
  uint16_t capacity = 48;
};

// Rolling raw-frame log: one file per weekday (`raw-data-monday` ...). The file
// for a weekday is truncated when that weekday comes round again, so only the
// last week is kept. Frames are buffered in RAM and flushed every few minutes
// so the RF receive loop is never blocked per frame.
class RawLog {
 public:
  explicit RawLog(const RawLogConfig& config);

  Status begin(int64_t epochSeconds);
  void append(int64_t epochSeconds, const uint8_t* frame, size_t length, int16_t rssiDbm,
              uint8_t channel);
  void loop(uint32_t nowMs);
  Status flush();
  uint32_t written() const { return written_; }

 private:
  struct Entry {
    int64_t epoch = 0;
    uint8_t frame[8] = {};
    uint8_t length = 0;
    int16_t rssi = 0;
    uint8_t channel = 0;
  };

  static const char* weekdayName(int weekday);
  bool ensureFile(int64_t epochSeconds);
  void formatIst(int64_t epochSeconds, char* output, size_t size);

  RawLogConfig config_;
  Entry entries_[64] = {};
  uint16_t count_ = 0;
  uint32_t written_ = 0;
  uint32_t lastFlushMs_ = 0;
  int currentWeekday_ = -1;
  File file_;
  bool ready_ = false;
};

}  // namespace akashvani
