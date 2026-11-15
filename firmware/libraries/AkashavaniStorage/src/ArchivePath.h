#pragma once

#include <cstddef>
#include <cstdint>

namespace akashavani {

constexpr size_t kArchivePathMax = 128;

// Builds `/archive/<device>/<YYYY>/<MM>/<DD>.jsonl`.
bool buildArchivePath(const char* deviceId, int64_t epochSeconds, char* output,
                      size_t outputSize);

// Builds `/archive/<device>/<YYYY>/<MM>`.
bool buildArchiveDirectory(const char* deviceId, int64_t epochSeconds, char* output,
                           size_t outputSize);

}  // namespace akashavani
