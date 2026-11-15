#pragma once

#include <cstdint>

namespace akashavani {
namespace protocol {

enum class MessageType : uint8_t {
  Observation = 0,
  Command = 1,
  CommandAck = 2,
  FirmwareCheck = 3,
  Unknown = 255,
};

const char* toString(MessageType type);
MessageType messageTypeFromString(const char* value);

}  // namespace protocol
}  // namespace akashavani
