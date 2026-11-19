#pragma once

#include <cstdint>

#include "MessageType.h"
#include "Protocol.h"

namespace akashavani {
namespace protocol {

struct Envelope {
  int version = kVersion;
  MessageType type = MessageType::Observation;
  char deviceId[33] = {};
  char messageId[65] = {};
  int64_t observedAtEpoch = 0;
  uint32_t sequence = 0;
};

enum class EnvelopeValidation : uint8_t {
  Ok = 0,
  UnsupportedVersion,
  UnknownMessageType,
  InvalidDeviceId,
  InvalidMessageId,
  InvalidTimestamp,
};

const char* toString(EnvelopeValidation validation);
EnvelopeValidation validateEnvelope(const Envelope& envelope);

}  // namespace protocol
}  // namespace akashavani
