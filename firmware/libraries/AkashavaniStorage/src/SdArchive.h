#pragma once

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>

#include "ArchivePath.h"
#include "IArchive.h"
#include "Status.h"

namespace akashavani {

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

  // Offline catch-up support. `last_sent` is the bucket-end epoch of the newest
  // record the server acknowledged; replay walks the daily files from that day
  // up to `nowEpoch`, returning each raw JSONL line in order.
  int64_t lastSentEpoch();
  Status setLastSentEpoch(int64_t epoch);
  Status beginReplay(int64_t afterEpoch, int64_t nowEpoch);
  Status beginReplayRange(int64_t startEpoch, int64_t endEpoch);
  bool nextReplayLine(char* output, size_t outputSize, int64_t& lineEpoch, bool& done);
  void endReplay();

  // Remove archive day files older than `cutoffEpoch` (3-year retention).
  Status pruneBefore(int64_t cutoffEpoch);

 private:
  Status ensureDirectory(const char* directory);
  Status openForDay(const char* path);
  Status deviceDirectory(char* output, size_t outputSize) const;
  void closeReplayFile();
  Status pruneDirectory(const char* path, int depth, int64_t cutoffEpoch);

  SPIClass& spi_;
  SdArchiveConfig config_;
  File file_;
  bool mounted_ = false;
  char currentPath_[kArchivePathMax] = {};
  uint64_t recordsWritten_ = 0;

  bool replay_ = false;
  int64_t replayAfter_ = 0;
  int64_t replayDay_ = 0;
  int64_t replayEndDay_ = 0;
  File replayFile_;
  char replayLine_[1536] = {};
};

}  // namespace akashavani
