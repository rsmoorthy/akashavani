#include "SdArchive.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "Log.h"

namespace akashavani {

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
    lastStatus_ = Status::NotReady;
    return lastStatus_;
  }
  if (record == nullptr || length == 0) {
    lastStatus_ = Status::InvalidArgument;
    return lastStatus_;
  }
  if (config_.deviceId == nullptr) {
    lastStatus_ = Status::NotReady;
    return lastStatus_;
  }

  // The caller stamps records; archive the UTC receipt time.
  const int64_t now = static_cast<int64_t>(time(nullptr));
  char path[kArchivePathMax];
  if (!buildArchivePath(config_.deviceId, now, path, sizeof(path))) {
    lastStatus_ = Status::InvalidArgument;
    return lastStatus_;
  }
  if (std::strcmp(path, currentPath_) != 0) {
    const Status status = openForDay(path);
    if (!isOk(status)) {
      lastStatus_ = status;
      return status;
    }
  }

  if (file_.write(reinterpret_cast<const uint8_t*>(record), length) != length ||
      file_.write(reinterpret_cast<const uint8_t*>("\n"), 1) != 1) {
    AK_LOG_ERROR("sd", "short write: %s", currentPath_);
    lastStatus_ = Status::IoError;
    return lastStatus_;
  }
  ++recordsWritten_;
  lastStatus_ = Status::Ok;
  if (config_.flushEachRecord) {
    lastStatus_ = flush();
    return lastStatus_;
  }
  return Status::Ok;
}

void SdArchive::closeCurrentFile() {
  if (file_) {
    file_.flush();
    file_.close();
  }
  currentPath_[0] = '\0';
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


namespace {

constexpr int64_t kSecondsPerDay = 86400;
constexpr int kReplayMaxDays = 45;

int64_t daysFromCivil(int year, unsigned month, unsigned day) {
  year -= month <= 2;
  const int era = (year >= 0 ? year : year - 399) / 400;
  const unsigned yearOfEra = static_cast<unsigned>(year - era * 400);
  const unsigned dayOfYear =
      (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
  const unsigned dayOfEra =
      yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
  return static_cast<int64_t>(era) * 146097 + static_cast<int64_t>(dayOfEra) - 719468;
}

int64_t parseBucketEpoch(const char* line) {
  const char* key = std::strstr(line, "\"bucket_end_epoch\"");
  if (key == nullptr) {
    return -1;
  }
  const char* colon = std::strchr(key, ':');
  if (colon == nullptr) {
    return -1;
  }
  const char* cursor = colon + 1;
  while (*cursor == ' ' || *cursor == '\t') {
    ++cursor;
  }
  return std::strtoll(cursor, nullptr, 10);
}

}  // namespace

Status SdArchive::deviceDirectory(char* output, size_t outputSize) const {
  if (config_.deviceId == nullptr || config_.deviceId[0] == '\0') {
    return Status::InvalidArgument;
  }
  const int written = std::snprintf(output, outputSize, "/archive/%s", config_.deviceId);
  if (written <= 0 || static_cast<size_t>(written) >= outputSize) {
    return Status::InvalidArgument;
  }
  return Status::Ok;
}

int64_t SdArchive::lastSentEpoch() {
  if (!mounted_) {
    return 0;
  }
  char directory[kArchivePathMax];
  if (!isOk(deviceDirectory(directory, sizeof(directory)))) {
    return 0;
  }
  char path[kArchivePathMax];
  std::snprintf(path, sizeof(path), "%s/last_sent", directory);
  File file = SD.open(path, FILE_READ);
  if (!file) {
    return 0;
  }
  String value = file.readStringUntil('\n');
  file.close();
  value.trim();
  if (value.length() == 0) {
    return 0;
  }
  return std::strtoll(value.c_str(), nullptr, 10);
}

Status SdArchive::setLastSentEpoch(int64_t epoch) {
  if (!mounted_) {
    return Status::NotReady;
  }
  char directory[kArchivePathMax];
  const Status directoryStatus = deviceDirectory(directory, sizeof(directory));
  if (!isOk(directoryStatus)) {
    return directoryStatus;
  }
  const Status made = ensureDirectory(directory);
  if (!isOk(made)) {
    return made;
  }
  char path[kArchivePathMax];
  std::snprintf(path, sizeof(path), "%s/last_sent", directory);
  File file = SD.open(path, FILE_WRITE);
  if (!file) {
    return Status::IoError;
  }
  file.printf("%lld\n", static_cast<long long>(epoch));
  file.close();
  return Status::Ok;
}

void SdArchive::closeReplayFile() {
  if (replayFile_) {
    replayFile_.close();
  }
}

Status SdArchive::beginReplay(int64_t afterEpoch, int64_t nowEpoch) {
  if (!mounted_) {
    return Status::NotReady;
  }
  if (nowEpoch <= 0) {
    return Status::InvalidArgument;
  }
  closeReplayFile();
  // A replay may read today's file. Close the append handle first; on some SD
  // implementations a second read handle on the open file returns no data.
  closeCurrentFile();
  replayAfter_ = afterEpoch > 0 ? afterEpoch : 0;
  replayEnd_ = nowEpoch;
  replayEndDay_ = nowEpoch / kSecondsPerDay;
  int64_t startDay =
      replayAfter_ > 0 ? replayAfter_ / kSecondsPerDay : replayEndDay_ - kReplayMaxDays;
  if (startDay < replayEndDay_ - kReplayMaxDays) {
    startDay = replayEndDay_ - kReplayMaxDays;
  }
  replayDay_ = startDay;
  replay_ = true;
  return Status::Ok;
}

Status SdArchive::beginReplayRange(int64_t startEpoch, int64_t endEpoch) {
  if (!mounted_) {
    return Status::NotReady;
  }
  if (startEpoch <= 0 || endEpoch < startEpoch) {
    return Status::InvalidArgument;
  }
  closeReplayFile();
  closeCurrentFile();
  replayAfter_ = startEpoch - 1;
  replayEnd_ = endEpoch;
  replayDay_ = startEpoch / kSecondsPerDay;
  replayEndDay_ = endEpoch / kSecondsPerDay;
  replay_ = true;
  return Status::Ok;
}

bool SdArchive::nextReplayLine(char* output, size_t outputSize, int64_t& lineEpoch,
                               bool& done) {
  done = false;
  if (!replay_ || output == nullptr || outputSize == 0) {
    done = true;
    return false;
  }
  char path[kArchivePathMax];
  while (replayDay_ <= replayEndDay_) {
    if (!replayFile_) {
      if (!buildArchivePath(config_.deviceId, replayDay_ * kSecondsPerDay, path,
                            sizeof(path))) {
        done = true;
        return false;
      }
      replayFile_ = SD.open(path, FILE_READ);
      ++replayDay_;
      if (!replayFile_) {
        continue;
      }
    }

    size_t length = 0;
    bool gotLine = false;
    bool overflow = false;
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
      if (c == '\r') {
        continue;
      }
      if (length + 1 < sizeof(replayLine_)) {
        replayLine_[length++] = c;
      } else {
        overflow = true;
      }
    }
    replayLine_[length] = '\0';
    if (!gotLine && length == 0) {
      closeReplayFile();
      continue;
    }
    if (overflow) {
      continue;
    }
    const int64_t epoch = parseBucketEpoch(replayLine_);
    if (epoch > replayAfter_ && (replayEnd_ <= 0 || epoch <= replayEnd_)) {
      const size_t copy = length < outputSize - 1 ? length : outputSize - 1;
      std::memcpy(output, replayLine_, copy);
      output[copy] = '\0';
      lineEpoch = epoch;
      return true;
    }
  }
  closeReplayFile();
  replay_ = false;
  done = true;
  return false;
}

void SdArchive::endReplay() {
  closeReplayFile();
  replay_ = false;
}

Status SdArchive::pruneBefore(int64_t cutoffEpoch) {
  if (!mounted_ || cutoffEpoch <= 0) {
    return Status::InvalidArgument;
  }
  char directory[kArchivePathMax];
  const Status status = deviceDirectory(directory, sizeof(directory));
  if (!isOk(status)) {
    return status;
  }
  return pruneDirectory(directory, 0, cutoffEpoch);
}

Status SdArchive::pruneDirectory(const char* path, int depth, int64_t cutoffEpoch) {
  File dir = SD.open(path);
  if (!dir || !dir.isDirectory()) {
    return Status::NotFound;
  }
  File entry;
  while ((entry = dir.openNextFile())) {
    char child[kArchivePathMax];
    const char* entryPath = entry.path();
    if (entryPath != nullptr && entryPath[0] == '/') {
      std::snprintf(child, sizeof(child), "%s", entryPath);
    } else {
      std::snprintf(child, sizeof(child), "%s/%s", path, entry.name());
    }
    const bool isDirectory = entry.isDirectory();
    entry.close();

    if (depth < 2) {
      if (isDirectory) {
        pruneDirectory(child, depth + 1, cutoffEpoch);
      }
      continue;
    }

    int year = 0;
    int month = 0;
    int day = 0;
    if (std::sscanf(child, "/archive/%*[^/]/%d/%d/%d.jsonl", &year, &month, &day) != 3) {
      continue;
    }
    const int64_t epoch =
        daysFromCivil(year, static_cast<unsigned>(month), static_cast<unsigned>(day)) *
        86400;
    if (epoch > 0 && epoch < cutoffEpoch) {
      SD.remove(child);
      AK_LOG_INFO("sd", "retention: removed %s", child);
    }
  }
  dir.close();

  if (depth > 0) {
    SD.rmdir(path);  // succeeds only when the directory is empty
  }
  return Status::Ok;
}

}  // namespace akashavani
