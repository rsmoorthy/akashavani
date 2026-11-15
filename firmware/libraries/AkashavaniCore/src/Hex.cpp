#include "Hex.h"

#include <cstring>

namespace akashavani {
namespace {

char nibbleToChar(uint8_t value, bool uppercase) {
  if (value < 10) {
    return static_cast<char>('0' + value);
  }
  return static_cast<char>((uppercase ? 'A' : 'a') + (value - 10));
}

int charToNibble(char value) {
  if (value >= '0' && value <= '9') {
    return value - '0';
  }
  if (value >= 'a' && value <= 'f') {
    return value - 'a' + 10;
  }
  if (value >= 'A' && value <= 'F') {
    return value - 'A' + 10;
  }
  return -1;
}

}  // namespace

size_t toHex(const uint8_t* data, size_t length, char* output, size_t outputSize,
             bool uppercase) {
  if (output == nullptr || (data == nullptr && length != 0)) {
    return 0;
  }
  const size_t needed = length * 2 + 1;
  if (outputSize < needed) {
    if (outputSize > 0) {
      output[0] = '\0';
    }
    return 0;
  }
  for (size_t i = 0; i < length; ++i) {
    output[i * 2] = nibbleToChar(static_cast<uint8_t>(data[i] >> 4), uppercase);
    output[i * 2 + 1] = nibbleToChar(static_cast<uint8_t>(data[i] & 0x0F), uppercase);
  }
  output[length * 2] = '\0';
  return length * 2;
}

bool fromHex(const char* text, uint8_t* output, size_t outputSize, size_t& bytesWritten) {
  bytesWritten = 0;
  if (text == nullptr || output == nullptr) {
    return false;
  }
  const size_t textLength = std::strlen(text);
  if ((textLength % 2) != 0) {
    return false;
  }
  const size_t byteCount = textLength / 2;
  if (byteCount > outputSize) {
    return false;
  }
  for (size_t i = 0; i < byteCount; ++i) {
    const int high = charToNibble(text[i * 2]);
    const int low = charToNibble(text[i * 2 + 1]);
    if (high < 0 || low < 0) {
      return false;
    }
    output[i] = static_cast<uint8_t>((high << 4) | low);
  }
  bytesWritten = byteCount;
  return true;
}

}  // namespace akashavani
