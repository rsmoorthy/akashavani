#pragma once

#include <cstddef>
#include <cstdint>

namespace akashvani {

// Formats Unix seconds as `YYYY-MM-DDTHH:MM:SSZ`.
// Returns false for invalid arguments or a buffer that is too small.
bool formatIso8601Utc(int64_t epochSeconds, char* output, size_t outputSize);

// Parses exactly `YYYY-MM-DDTHH:MM:SSZ`. Returns false for malformed or
// out-of-range values. No timezone database is used.
bool parseIso8601Utc(const char* text, int64_t& epochSeconds);

// Splits Unix seconds into UTC calendar components.
struct UtcDateTime {
  int year = 1970;
  uint8_t month = 1;
  uint8_t day = 1;
  uint8_t hour = 0;
  uint8_t minute = 0;
  uint8_t second = 0;
};

bool epochToUtc(int64_t epochSeconds, UtcDateTime& out);
bool utcToEpoch(const UtcDateTime& value, int64_t& epochSeconds);

}  // namespace akashvani
