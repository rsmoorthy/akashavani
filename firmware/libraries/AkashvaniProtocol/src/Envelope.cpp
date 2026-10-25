#include "Envelope.h"

#include <cstring>

#include "DeviceId.h"
#include "Protocol.h"

namespace akashvani {
namespace protocol {
namespace {

bool isValidMessageId(const char* value) {
  if (value == nullptr) {
    return false;
  }
  const size_t length = std::strlen(value);
  if (length < 8 || length > kMessageIdMaxLength) {
    return false;
  }
  for (size_t i = 0; i < length; ++i) {
    const char c = value[i];
    const bool allowed = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                         (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' ||
                         c == ':';
    if (!allowed) {
      return false;
    }
  }
  return true;
}

}  // namespace

const char* toString(EnvelopeValidation validation) {
  switch (validation) {
    case EnvelopeValidation::Ok:
      return "ok";
    case EnvelopeValidation::UnsupportedVersion:
      return "unsupported-version";
    case EnvelopeValidation::UnknownMessageType:
      return "unknown-message-type";
    case EnvelopeValidation::InvalidDeviceId:
      return "invalid-device-id";
    case EnvelopeValidation::InvalidMessageId:
      return "invalid-message-id";
    case EnvelopeValidation::InvalidTimestamp:
      return "invalid-timestamp";
  }
  return "unknown";
}

EnvelopeValidation validateEnvelope(const Envelope& envelope) {
  if (envelope.version != kVersion) {
    return EnvelopeValidation::UnsupportedVersion;
  }
  if (envelope.type == MessageType::Unknown) {
    return EnvelopeValidation::UnknownMessageType;
  }
  if (!isValidDeviceId(envelope.deviceId)) {
    return EnvelopeValidation::InvalidDeviceId;
  }
  if (!isValidMessageId(envelope.messageId)) {
    return EnvelopeValidation::InvalidMessageId;
  }
  // 2000-01-01 .. 2100-01-01. Catches unset clocks before data reaches cloud.
  constexpr int64_t kMinEpoch = 946684800;
  constexpr int64_t kMaxEpoch = 4102444800;
  if (envelope.observedAtEpoch < kMinEpoch || envelope.observedAtEpoch >= kMaxEpoch) {
    return EnvelopeValidation::InvalidTimestamp;
  }
  return EnvelopeValidation::Ok;
}

}  // namespace protocol
}  // namespace akashvani
