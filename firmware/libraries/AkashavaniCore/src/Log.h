#pragma once

#include <Arduino.h>

#include <cstdarg>
#include <cstddef>
#include <cstdint>

namespace akashavani {

enum class LogLevel : uint8_t {
  Error = 0,
  Warn = 1,
  Info = 2,
  Debug = 3,
};

class Log {
 public:
  static void begin(unsigned long baud = 115200);
  static void setLevel(LogLevel level);
  static LogLevel level();

  static void printf(LogLevel level, const char* tag, const char* format, ...)
      __attribute__((format(printf, 3, 4)));
  static void hexDump(LogLevel level, const char* tag, const uint8_t* data, size_t length);

 private:
  static LogLevel currentLevel_;
};

}  // namespace akashavani

#define AK_LOG_ERROR(tag, ...) \
  ::akashavani::Log::printf(::akashavani::LogLevel::Error, tag, __VA_ARGS__)
#define AK_LOG_WARN(tag, ...) \
  ::akashavani::Log::printf(::akashavani::LogLevel::Warn, tag, __VA_ARGS__)
#define AK_LOG_INFO(tag, ...) \
  ::akashavani::Log::printf(::akashavani::LogLevel::Info, tag, __VA_ARGS__)
#define AK_LOG_DEBUG(tag, ...) \
  ::akashavani::Log::printf(::akashavani::LogLevel::Debug, tag, __VA_ARGS__)
