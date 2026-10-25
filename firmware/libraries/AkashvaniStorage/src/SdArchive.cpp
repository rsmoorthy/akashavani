#include "SdArchive.h"

#include <cstring>
#include <ctime>

#include "Log.h"

namespace akashvani {

SdArchive::SdArchive(SPIClass& spi, const SdArchiveConfig& config)
    : spi_(spi), config_(config) {}

Status SdArchive::begin() {
  spi_.begin(config_.sckPin, config_.misoPin, config_.mosiPin, config_.csPin);
  if (!SD.begin(config_.csPin, spi_, config_.frequencyHz)) {
    mounted_ = false;
    AK_LOG_ERROR("sd", "mount failed (cs=%u)", config_.csPin);
    return Status::IoError;
  }
  if (SD.cardType() == CARD_NONE) {
    mounted_ = false;
    AK_LOG_ERROR("sd", "no card detected");
    return Status::NotFound;
  }
  mounted_ = true;
  AK_LOG_INFO("sd", "mounted, card type=%u, size=%lluMB",
              static_cast<unsigned>(SD.cardType()),
              static_cast<unsigned long long>(SD.cardSize() / (1024ULL * 1024ULL)));
  return Status::Ok;
}

Status SdArchive::ensureDirectory(const char* directory) {
  if (!mounted_ || directory == nullptr || directory[0] != '/') {
    return Status::InvalidArgument;
  }
  char buffer[kArchivePathMax];
  const size_t length = std::strlen(directory);
  if (length >= sizeof(buffer)) {
    return Status::InvalidArgument;
  }
  std::memcpy(buffer, directory, length + 1);
  for (size_t i = 1; i <= length; ++i) {
    if (buffer[i] == '/' || buffer[i] == '\0') {
      const char saved = buffer[i];
      buffer[i] = '\0';
      if (!SD.exists(buffer)) {
        if (!SD.mkdir(buffer)) {
          AK_LOG_ERROR("sd", "mkdir failed: %s", buffer);
          return Status::IoError;
        }
      }
      buffer[i] = saved;
    }
  }
  return Status::Ok;
}

Status SdArchive::openForDay(const char* path) {
  const char* lastSlash = std::strrchr(path, '/');
  if (lastSlash == nullptr) {
    return Status::InvalidArgument;
  }
  char directory[kArchivePathMax];
  const size_t directoryLength = static_cast<size_t>(lastSlash - path);
  if (directoryLength == 0 || directoryLength >= sizeof(directory)) {
    return Status::InvalidArgument;
  }
  std::memcpy(directory, path, directoryLength);
  directory[directoryLength] = '\0';

  Status status = ensureDirectory(directory);
  if (!isOk(status)) {
    return status;
  }
  if (file_) {
    file_.close();
  }
  file_ = SD.open(path, FILE_APPEND);
  if (!file_) {
    AK_LOG_ERROR("sd", "open failed: %s", path);
    return Status::IoError;
  }
  std::strncpy(currentPath_, path, sizeof(currentPath_) - 1);
  currentPath_[sizeof(currentPath_) - 1] = '\0';
  return Status::Ok;
}

Status SdArchive::append(const char* record, size_t length) {
  if (!mounted_) {
    return Status::NotReady;
  }
  if (record == nullptr || length == 0) {
    return Status::InvalidArgument;
  }
  if (config_.deviceId == nullptr) {
    return Status::NotReady;
  }

  // The caller stamps records; archive the local receipt time.
  const int64_t now = static_cast<int64_t>(time(nullptr));
  char path[kArchivePathMax];
  if (!buildArchivePath(config_.deviceId, now, path, sizeof(path))) {
    return Status::InvalidArgument;
  }
  if (std::strcmp(path, currentPath_) != 0) {
    const Status status = openForDay(path);
    if (!isOk(status)) {
      return status;
    }
  }

  if (file_.write(reinterpret_cast<const uint8_t*>(record), length) != length ||
      file_.write(reinterpret_cast<const uint8_t*>("\n"), 1) != 1) {
    AK_LOG_ERROR("sd", "short write: %s", currentPath_);
    return Status::IoError;
  }
  ++recordsWritten_;
  if (config_.flushEachRecord) {
    return flush();
  }
  return Status::Ok;
}

Status SdArchive::flush() {
  if (!mounted_) {
    return Status::NotReady;
  }
  if (file_) {
    file_.flush();
    if (!file_) {
      return Status::IoError;
    }
  }
  return Status::Ok;
}

}  // namespace akashvani
