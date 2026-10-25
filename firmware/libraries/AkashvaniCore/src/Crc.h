#pragma once

#include <cstddef>
#include <cstdint>

namespace akashvani {

// CRC-16/CCITT-FALSE: poly 0x1021, init 0xFFFF, no reflection, xorout 0x0000.
uint16_t crc16Ccitt(const uint8_t* data, size_t length, uint16_t initial = 0xFFFF);

// CRC-16/XMODEM: poly 0x1021, init 0x0000, no reflection, xorout 0x0000.
uint16_t crc16Xmodem(const uint8_t* data, size_t length, uint16_t initial = 0x0000);

// CRC-16/ARC: reflected poly 0xA001, init 0x0000, xorout 0x0000.
uint16_t crc16Arc(const uint8_t* data, size_t length, uint16_t initial = 0x0000);

}  // namespace akashvani
