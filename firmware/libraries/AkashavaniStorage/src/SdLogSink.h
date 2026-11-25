#pragma once

#include <Arduino.h>
#include <SD.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <cstddef>
#include <cstdint>

#include "Log.h"
#include "Status.h"

namespace akashavani {

struct SdLogConfig {
  const char* root = "/logs";
  const char* deviceId = nullptr;
  uint32_t flushIntervalMs = 60UL * 1000UL;
  uint32_t maxBytesPerDay = 256UL * 1024UL;
  uint32_t retentionDays = 30;
  LogLevel minLevel = LogLevel::Info;
};

// Buffered, thread-safe log sink. Log::printf() may run in the radio task, so
// write() only copies into a fixed RAM buffer. loop() drains it to SD from the
// main loop. If the buffer is full or busy, the line is dropped and counted.
class SdLogSink final : public ILogSink {
 public:
  explicit SdLogSink(const SdLogConfig& config);

  Status begin();
  void write(LogLevel level, const char* tag, const char* message) override;
  void loop(uint32_t nowMs);
  Status flush();

  uint64_t writtenBytes() const { return writtenBytes_; }
  uint32_t dropped() const { return dropped_; }
  Status lastStatus() const { return lastStatus_; }
  const char* currentPath() const { return currentPath_; }

 private:
  static constexpr size_t kBufferBytes = 4096;
  static constexpr size_t kFlushThreshold = 3072;
  static constexpr size_t kLineBytes = 256;

  bool buildPath(char* output, size_t size) const;
  bool ensureDirectory(const char* path) const;
  void pruneOldFiles();

  SdLogConfig config_;
  bool ready_ = false;
  SemaphoreHandle_t mutex_ = nullptr;
  char buffer_[kBufferBytes] = {};
  size_t length_ = 0;
  uint32_t lastFlushMs_ = 0;
  uint64_t writtenBytes_ = 0;
  uint32_t dropped_ = 0;
  Status lastStatus_ = Status::NotReady;
  char currentPath_[128] = {};
};

}  // namespace akashavani
