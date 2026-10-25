#pragma once

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>

#include "ArchivePath.h"
#include "IArchive.h"
#include "Status.h"

namespace akashvani {

struct SdArchiveConfig {
  uint8_t csPin = 9;
  uint8_t sckPin = 12;
  uint8_t misoPin = 13;
  uint8_t mosiPin = 11;
  uint32_t frequencyHz = 4000000;
  const char* deviceId = nullptr;
  bool flushEachRecord = true;
};

// Appends one JSON object per line to a daily file on the SD card.
class SdArchive final : public IArchive {
 public:
  SdArchive(SPIClass& spi, const SdArchiveConfig& config);

  Status begin() override;
  Status append(const char* record, size_t length) override;
  Status flush() override;
  bool isMounted() const override { return mounted_; }
  uint64_t recordsWritten() const override { return recordsWritten_; }

  const char* currentPath() const { return currentPath_; }

 private:
  Status ensureDirectory(const char* directory);
  Status openForDay(const char* path);

  SPIClass& spi_;
  SdArchiveConfig config_;
  File file_;
  bool mounted_ = false;
  char currentPath_[kArchivePathMax] = {};
  uint64_t recordsWritten_ = 0;
};

}  // namespace akashvani
