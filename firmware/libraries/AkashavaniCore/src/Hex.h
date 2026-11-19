#pragma once

#include <cstddef>
#include <cstdint>

namespace akashavani {

// Writes `length` bytes as hex plus a terminating NUL.
// Returns the number of characters written, or 0 if the buffer is too small.
size_t toHex(const uint8_t* data, size_t length, char* output, size_t outputSize,
             bool uppercase = false);

// Parses a hex string. Returns false on odd length, invalid characters or a
// buffer that is too small.
bool fromHex(const char* text, uint8_t* output, size_t outputSize, size_t& bytesWritten);

}  // namespace akashavani
