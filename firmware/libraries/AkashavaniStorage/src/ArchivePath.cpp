#include "ArchivePath.h"

#include <cstdio>

#include "DeviceId.h"
#include "TimeUtils.h"

namespace akashavani {
namespace {

bool buildPrefix(const char* deviceId, int64_t epochSeconds, char* output, size_t outputSize,
                 bool includeDayFile) {
  if (output == nullptr || outputSize == 0 || !isValidDeviceId(deviceId)) {
    return false;
  }
  UtcDateTime date;
  if (!epochToUtc(epochSeconds, date)) {
    return false;
  }
  const int written =
      includeDayFile
          ? std::snprintf(output, outputSize, "/archive/%s/%04d/%02u/%02u.jsonl", deviceId,
                          date.year, date.month, date.day)
          : std::snprintf(output, outputSize, "/archive/%s/%04d/%02u", deviceId, date.year,
                          date.month);
  return written > 0 && static_cast<size_t>(written) < outputSize;
}

}  // namespace

bool buildArchivePath(const char* deviceId, int64_t epochSeconds, char* output,
                      size_t outputSize) {
  return buildPrefix(deviceId, epochSeconds, output, outputSize, true);
}

bool buildArchiveDirectory(const char* deviceId, int64_t epochSeconds, char* output,
                           size_t outputSize) {
  return buildPrefix(deviceId, epochSeconds, output, outputSize, false);
}

}  // namespace akashavani
