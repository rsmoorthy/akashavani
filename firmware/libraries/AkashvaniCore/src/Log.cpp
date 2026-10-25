#include "Log.h"

namespace akashvani {

LogLevel Log::currentLevel_ = LogLevel::Info;

namespace {

const char* levelName(LogLevel level) {
  switch (level) {
    case LogLevel::Error:
      return "E";
    case LogLevel::Warn:
      return "W";
    case LogLevel::Info:
      return "I";
    case LogLevel::Debug:
      return "D";
  }
  return "?";
}

}  // namespace

void Log::begin(unsigned long baud) {
  Serial.begin(baud);
  delay(50);
}

void Log::setLevel(LogLevel level) { currentLevel_ = level; }

LogLevel Log::level() { return currentLevel_; }

void Log::printf(LogLevel level, const char* tag, const char* format, ...) {
  if (level > currentLevel_) {
    return;
  }
  char message[256];
  va_list args;
  va_start(args, format);
  vsnprintf(message, sizeof(message), format, args);
  va_end(args);
  Serial.printf("[%8lu] %s %-10s %s\n", static_cast<unsigned long>(millis()),
                levelName(level), tag == nullptr ? "-" : tag, message);
}

void Log::hexDump(LogLevel level, const char* tag, const uint8_t* data, size_t length) {
  if (level > currentLevel_ || data == nullptr) {
    return;
  }
  const size_t lineBytes = 16;
  for (size_t offset = 0; offset < length; offset += lineBytes) {
    char line[lineBytes * 3 + 1];
    char* cursor = line;
    const size_t count = (length - offset < lineBytes) ? length - offset : lineBytes;
    for (size_t i = 0; i < count; ++i) {
      cursor += snprintf(cursor, sizeof(line) - static_cast<size_t>(cursor - line), "%02x ",
                         data[offset + i]);
    }
    *cursor = '\0';
    printf(level, tag, "%04u: %s", static_cast<unsigned>(offset), line);
  }
}

}  // namespace akashvani
