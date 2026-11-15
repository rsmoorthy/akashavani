#include "Crc.h"

namespace akashavani {
namespace {

uint16_t crc16CcittUpdate(uint16_t crc, const uint8_t* data, size_t length) {
  for (size_t i = 0; i < length; ++i) {
    crc ^= static_cast<uint16_t>(data[i]) << 8;
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc & 0x8000u) ? static_cast<uint16_t>((crc << 1) ^ 0x1021u)
                            : static_cast<uint16_t>(crc << 1);
    }
  }
  return crc;
}

}  // namespace

uint16_t crc16Ccitt(const uint8_t* data, size_t length, uint16_t initial) {
  if (data == nullptr && length != 0) {
    return 0;
  }
  return crc16CcittUpdate(initial, data, length);
}

uint16_t crc16Xmodem(const uint8_t* data, size_t length, uint16_t initial) {
  return crc16Ccitt(data, length, initial);
}

uint16_t crc16Arc(const uint8_t* data, size_t length, uint16_t initial) {
  if (data == nullptr && length != 0) {
    return 0;
  }
  uint16_t crc = initial;
  for (size_t i = 0; i < length; ++i) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc & 0x0001u) ? static_cast<uint16_t>((crc >> 1) ^ 0xA001u)
                            : static_cast<uint16_t>(crc >> 1);
    }
  }
  return crc;
}

}  // namespace akashavani
