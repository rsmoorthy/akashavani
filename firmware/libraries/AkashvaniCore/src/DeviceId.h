#pragma once

#include <cstddef>
#include <cstdint>

namespace akashvani {

// Device IDs are lowercase, 3..32 characters, start with a letter or digit,
// contain only [a-z0-9-] and do not end with '-'.
bool isValidDeviceId(const char* deviceId);

// Writes `ak-` followed by the last three MAC bytes as lowercase hex.
// `output` must hold at least 10 bytes (e.g. `ak-3f2a1b\0`).
bool makeDeviceIdFromMac(const uint8_t mac[6], char* output, size_t outputSize);

}  // namespace akashvani
