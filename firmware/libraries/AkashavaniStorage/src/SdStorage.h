#pragma once

#include <Arduino.h>
#include <SD.h>

#include <cstddef>
#include <cstdint>

#include "Status.h"

namespace akashavani {

struct SdStorageEntry {
  char name[64] = {};
  bool directory = false;
  uint32_t size = 0;
  uint32_t modified = 0;
};

// Small, bounded helpers for the operator-facing SD inspection commands.
// They only expose the data roots, never arbitrary flash or secret storage.
class SdStorage {
 public:
  static constexpr size_t kMaxPath = 128;

  static bool isValidPath(const char* path, bool allowRoot);
  static bool isAllowedPath(const char* path, bool allowRoot);

  static Status status(uint64_t& totalBytes, uint64_t& usedBytes);
  static Status list(const char* path, SdStorageEntry* entries, size_t capacity,
                     size_t& count, bool& truncated);
  static Status readChunk(const char* path, uint32_t offset, uint8_t* buffer,
                          size_t capacity, size_t& read, bool& eof);
  static Status ensureDirectory(const char* directory);
};

}  // namespace akashavani
