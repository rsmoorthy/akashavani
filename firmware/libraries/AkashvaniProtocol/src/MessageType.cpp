#include "MessageType.h"

#include <cstring>

namespace akashvani {
namespace protocol {

const char* toString(MessageType type) {
  switch (type) {
    case MessageType::Observation:
      return "observation";
    case MessageType::Command:
      return "command";
    case MessageType::CommandAck:
      return "command_ack";
    case MessageType::FirmwareCheck:
      return "firmware_check";
    case MessageType::Unknown:
      return "unknown";
  }
  return "unknown";
}

MessageType messageTypeFromString(const char* value) {
  if (value == nullptr) {
    return MessageType::Unknown;
  }
  if (std::strcmp(value, "observation") == 0) {
    return MessageType::Observation;
  }
  if (std::strcmp(value, "command") == 0) {
    return MessageType::Command;
  }
  if (std::strcmp(value, "command_ack") == 0) {
    return MessageType::CommandAck;
  }
  if (std::strcmp(value, "firmware_check") == 0) {
    return MessageType::FirmwareCheck;
  }
  return MessageType::Unknown;
}

}  // namespace protocol
}  // namespace akashvani
