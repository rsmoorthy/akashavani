#include "DeviceId.h"

#include <cstdio>
#include <cstring>

namespace akashavani {

bool isValidDeviceId(const char* deviceId) {
  if (deviceId == nullptr) {
    return false;
  }
  const size_t length = std::strlen(deviceId);
  if (length < 3 || length > 32 || deviceId[length - 1] == '-') {
    return false;
  }
  for (size_t i = 0; i < length; ++i) {
    const char c = deviceId[i];
    const bool alnum = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
    if (!alnum && c != '-') {
      return false;
    }
  }
  return true;
}

bool makeDeviceIdFromMac(const uint8_t mac[6], char* output, size_t outputSize) {
  if (mac == nullptr || output == nullptr || outputSize < 10) {
    return false;
  }
  const int written = std::snprintf(output, outputSize, "ak-%02x%02x%02x", mac[3], mac[4],
                                    mac[5]);
  return written == 9;
}

}  // namespace akashavani
