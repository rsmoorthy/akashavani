#include "SdLogSink.h"

#include <cstdio>
#include <cstring>
#include <ctime>

#include "SdStorage.h"

namespace akashavani {
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

SdLogSink::SdLogSink(const SdLogConfig& config) : config_(config) {}

Status SdLogSink::begin() {
  ready_ = false;
  length_ = 0;
  writtenBytes_ = 0;
  dropped_ = 0;
  currentPath_[0] = '\0';
  if (config_.deviceId == nullptr || config_.deviceId[0] == '\0') {
    lastStatus_ = Status::InvalidArgument;
    return lastStatus_;
  }
  if (SD.cardType() == CARD_NONE) {
    lastStatus_ = Status::NotReady;
    return lastStatus_;
  }
  if (mutex_ == nullptr) {
    mutex_ = xSemaphoreCreateMutex();
    if (mutex_ == nullptr) {
      lastStatus_ = Status::NoMemory;
      return lastStatus_;
    }
  }
  char directory[128];
  std::snprintf(directory, sizeof(directory), "%s/%s", config_.root, config_.deviceId);
  const Status status = SdStorage::ensureDirectory(directory);
  if (!isOk(status)) {
    lastStatus_ = status;
    return lastStatus_;
  }
  lastFlushMs_ = millis();
  lastStatus_ = Status::Ok;
  ready_ = true;
  pruneOldFiles();
  return Status::Ok;
}

void SdLogSink::pruneOldFiles() {
  if (config_.retentionDays == 0) {
    return;
  }
  const time_t now = time(nullptr);
  if (now < 946684800) {
    return;
  }
  const time_t cutoff = now - static_cast<time_t>(config_.retentionDays) * 86400;
  char root[128];
  std::snprintf(root, sizeof(root), "%s/%s", config_.root, config_.deviceId);
  File yearDir = SD.open(root);
  if (!yearDir || !yearDir.isDirectory()) {
    return;
  }
  File year = yearDir.openNextFile();
  while (year) {
    if (year.isDirectory()) {
      char yearPath[128];
      std::snprintf(yearPath, sizeof(yearPath), "%s/%s", root, year.name());
      File monthDir = SD.open(yearPath);
      File month = monthDir ? monthDir.openNextFile() : File();
      while (month) {
        if (month.isDirectory()) {
          char monthPath[128];
          std::snprintf(monthPath, sizeof(monthPath), "%s/%s", yearPath, month.name());
          File dayDir = SD.open(monthPath);
          File day = dayDir ? dayDir.openNextFile() : File();
          while (day) {
            if (!day.isDirectory()) {
              const time_t modified = day.getLastWrite();
              if (modified > 0 && modified < cutoff) {
                char filePath[128];
                std::snprintf(filePath, sizeof(filePath), "%s/%s", monthPath, day.name());
                SD.remove(filePath);
              }
            }
            day.close();
            day = dayDir.openNextFile();
          }
          dayDir.close();
        }
        month.close();
        month = monthDir.openNextFile();
      }
      monthDir.close();
    }
    year.close();
    year = yearDir.openNextFile();
  }
  yearDir.close();
}

bool SdLogSink::buildPath(char* output, size_t size) const {
  const time_t now = time(nullptr);
  if (now < 946684800) {
    return false;
  }
  struct tm parts = {};
  gmtime_r(&now, &parts);
  const int written = std::snprintf(
      output, size, "%s/%s/%04d/%02d/%02d.log", config_.root, config_.deviceId,
      parts.tm_year + 1900, parts.tm_mon + 1, parts.tm_mday);
  return written > 0 && static_cast<size_t>(written) < size;
}

bool SdLogSink::ensureDirectory(const char* path) const {
  char buffer[128];
  std::snprintf(buffer, sizeof(buffer), "%s", path);
  char* slash = std::strrchr(buffer, '/');
  if (slash == nullptr || slash == buffer) {
    return false;
  }
  *slash = '\0';
  return isOk(SdStorage::ensureDirectory(buffer));
}

void SdLogSink::write(LogLevel level, const char* tag, const char* message) {
  if (!ready_ || level > config_.minLevel) {
    return;
  }
  const time_t now = time(nullptr);
  if (now < 946684800) {
    return;
  }
  struct tm parts = {};
  gmtime_r(&now, &parts);
  char line[kLineBytes];
  const int written = std::snprintf(
      line, sizeof(line), "%04d-%02d-%02dT%02d:%02d:%02dZ %s %-10s %s\n",
      parts.tm_year + 1900, parts.tm_mon + 1, parts.tm_mday, parts.tm_hour,
      parts.tm_min, parts.tm_sec, levelName(level), tag == nullptr ? "-" : tag,
      message == nullptr ? "" : message);
  if (written <= 0) {
    return;
  }
  size_t length = static_cast<size_t>(written);
  if (length >= sizeof(line)) {
    length = sizeof(line) - 1;
    line[length - 1] = '\n';
  }
  if (xSemaphoreTake(mutex_, 0) != pdTRUE) {
    ++dropped_;
    return;
  }
  if (!ready_ || length_ + length > sizeof(buffer_)) {
    ++dropped_;
    xSemaphoreGive(mutex_);
    return;
  }
  std::memcpy(buffer_ + length_, line, length);
  length_ += length;
  xSemaphoreGive(mutex_);
}

Status SdLogSink::flush() {
  if (!ready_ || mutex_ == nullptr) {
    return Status::NotReady;
  }
  if (xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) {
    return Status::Timeout;
  }
  if (length_ == 0) {
    xSemaphoreGive(mutex_);
    return Status::Ok;
  }
  char path[128];
  if (!buildPath(path, sizeof(path))) {
    length_ = 0;
    xSemaphoreGive(mutex_);
    lastStatus_ = Status::NotReady;
    return lastStatus_;
  }
  if (!ensureDirectory(path)) {
    length_ = 0;
    xSemaphoreGive(mutex_);
    lastStatus_ = Status::IoError;
    return lastStatus_;
  }
  File file = SD.open(path, FILE_APPEND);
  if (!file) {
    length_ = 0;
    xSemaphoreGive(mutex_);
    lastStatus_ = Status::IoError;
    return lastStatus_;
  }
  if (file.size() >= config_.maxBytesPerDay) {
    dropped_ += static_cast<uint32_t>(length_);
    length_ = 0;
    file.close();
    xSemaphoreGive(mutex_);
    lastStatus_ = Status::NoMemory;
    return lastStatus_;
  }
  const size_t written = file.write(reinterpret_cast<const uint8_t*>(buffer_), length_);
  file.flush();
  file.close();
  if (written != length_) {
    dropped_ += static_cast<uint32_t>(length_ - written);
    lastStatus_ = Status::IoError;
  } else {
    writtenBytes_ += written;
    lastStatus_ = Status::Ok;
  }
  length_ = 0;
  lastFlushMs_ = millis();
  std::snprintf(currentPath_, sizeof(currentPath_), "%s", path);
  xSemaphoreGive(mutex_);
  return lastStatus_;
}

void SdLogSink::loop(uint32_t nowMs) {
  if (!ready_ || length_ == 0) {
    return;
  }
  if (length_ >= kFlushThreshold ||
      (lastFlushMs_ != 0 && nowMs - lastFlushMs_ >= config_.flushIntervalMs)) {
    flush();
  }
}

}  // namespace akashavani
