#include <gtest/gtest.h>

#include <cstring>

#include "AkashavaniProtocol.h"

namespace {

using namespace akashavani::protocol;

Envelope validEnvelope() {
  Envelope envelope;
  envelope.version = kVersion;
  envelope.type = MessageType::Observation;
  std::strncpy(envelope.deviceId, "ak-3f2a1b", sizeof(envelope.deviceId) - 1);
  std::strncpy(envelope.messageId, "ak-3f2a1b-1792908000-1",
               sizeof(envelope.messageId) - 1);
  envelope.observedAtEpoch = 1792908000;
  envelope.sequence = 1;
  return envelope;
}

TEST(MessageType, RoundTrips) {
  EXPECT_EQ(MessageType::Observation, messageTypeFromString("observation"));
  EXPECT_EQ(MessageType::Command, messageTypeFromString("command"));
  EXPECT_EQ(MessageType::CommandAck, messageTypeFromString("command_ack"));
  EXPECT_EQ(MessageType::FirmwareCheck, messageTypeFromString("firmware_check"));
  EXPECT_EQ(MessageType::Unknown, messageTypeFromString("nonsense"));
  EXPECT_STREQ("command_ack", toString(MessageType::CommandAck));
}

TEST(CommandType, RoundTrips) {
  const CommandType types[] = {
      CommandType::Reboot,       CommandType::SyncTime,
      CommandType::SetConfig,    CommandType::CheckForUpdate,
      CommandType::UploadLogs,   CommandType::SelfTest,
      CommandType::GetConfig,    CommandType::PushConfig,
      CommandType::GetDataAgain, CommandType::DownloadRawData,
      CommandType::StorageStatus, CommandType::SdList,
      CommandType::SdRead};
  for (const CommandType type : types) {
    EXPECT_EQ(type, commandTypeFromString(toString(type)));
  }
  EXPECT_EQ(CommandType::Unknown, commandTypeFromString("format_disk"));
  EXPECT_STREQ("check_for_update", toString(CommandType::CheckForUpdate));
}

TEST(Envelope, AcceptsValid) {
  EXPECT_EQ(EnvelopeValidation::Ok, validateEnvelope(validEnvelope()));
}

TEST(Envelope, RejectsUnsupportedVersion) {
  Envelope envelope = validEnvelope();
  envelope.version = 2;
  EXPECT_EQ(EnvelopeValidation::UnsupportedVersion, validateEnvelope(envelope));
}

TEST(Envelope, RejectsUnknownType) {
  Envelope envelope = validEnvelope();
  envelope.type = MessageType::Unknown;
  EXPECT_EQ(EnvelopeValidation::UnknownMessageType, validateEnvelope(envelope));
}

TEST(Envelope, RejectsBadDeviceId) {
  Envelope envelope = validEnvelope();
  std::strncpy(envelope.deviceId, "INVALID", sizeof(envelope.deviceId) - 1);
  EXPECT_EQ(EnvelopeValidation::InvalidDeviceId, validateEnvelope(envelope));
}

TEST(Envelope, RejectsBadMessageId) {
  Envelope envelope = validEnvelope();
  std::strncpy(envelope.messageId, "short", sizeof(envelope.messageId) - 1);
  EXPECT_EQ(EnvelopeValidation::InvalidMessageId, validateEnvelope(envelope));

  std::strncpy(envelope.messageId, "has a space", sizeof(envelope.messageId) - 1);
  EXPECT_EQ(EnvelopeValidation::InvalidMessageId, validateEnvelope(envelope));
}

TEST(Envelope, RejectsInvalidTimestamps) {
  Envelope envelope = validEnvelope();
  envelope.observedAtEpoch = 0;
  EXPECT_EQ(EnvelopeValidation::InvalidTimestamp, validateEnvelope(envelope));
  envelope.observedAtEpoch = 4102444800LL;
  EXPECT_EQ(EnvelopeValidation::InvalidTimestamp, validateEnvelope(envelope));
}

TEST(Protocol, FieldNamesMatchSchemas) {
  EXPECT_STREQ("temperature_c", field::kTemperatureC);
  EXPECT_STREQ("wind_speed_ms", field::kWindSpeedMs);
  EXPECT_STREQ("rain_rate_mm_h", field::kRainRateMmH);
  EXPECT_EQ(1, kVersion);
}

}  // namespace
