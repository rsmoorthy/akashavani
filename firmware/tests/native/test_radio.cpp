#include <gtest/gtest.h>

#include <cstring>

#include "DavisChannels.h"
#include "MockRadio.h"

namespace {

using namespace akashavani;

TEST(DavisChannels, TableMatchesPrototypeAndBand) {
  EXPECT_EQ(51u, kDavisChannelCount);
  EXPECT_NEAR(911.4138f, davisChannelFrequencyMhz(0), 0.001f);
  EXPECT_NEAR(911.9152f, davisChannelFrequencyMhz(2), 0.001f);
  EXPECT_NEAR(925.4624f, davisChannelFrequencyMhz(50), 0.001f);

  for (size_t channel = 0; channel < kDavisChannelCount; ++channel) {
    const float frequency = davisChannelFrequencyMhz(static_cast<uint8_t>(channel));
    EXPECT_GE(frequency, 902.0f);
    EXPECT_LE(frequency, 928.0f);
  }
}

TEST(DavisChannels, HopWrapsInBothDirections) {
  EXPECT_EQ(0u, static_cast<unsigned>(nextDavisChannel(50)));
  EXPECT_EQ(50u, static_cast<unsigned>(previousDavisChannel(0)));
  EXPECT_EQ(17u, static_cast<unsigned>(nextDavisChannel(16)));
  EXPECT_EQ(16u, static_cast<unsigned>(previousDavisChannel(17)));
}

TEST(MockRadio, RequiresBegin) {
  MockRadio radio;
  RadioFrame frame;
  EXPECT_EQ(Status::NotReady, radio.receive(frame, 0));
  const uint8_t payload[] = {1, 2, 3};
  EXPECT_EQ(Status::NotReady, radio.transmit(payload, sizeof(payload)));
}

TEST(MockRadio, ReceivesQueuedFramesInOrder) {
  MockRadio radio;
  ASSERT_EQ(Status::Ok, radio.begin(RadioConfig{}));

  const uint8_t first[] = {0x10, 0x11};
  const uint8_t second[] = {0x20, 0x21, 0x22};
  ASSERT_TRUE(radio.queueFrame(first, sizeof(first), -60, 100));
  ASSERT_TRUE(radio.queueFrame(second, sizeof(second), -70, 200));

  RadioFrame frame;
  ASSERT_EQ(Status::Ok, radio.receive(frame, 0));
  EXPECT_EQ(2u, frame.length);
  EXPECT_EQ(0, std::memcmp(first, frame.data, sizeof(first)));
  EXPECT_EQ(-60, frame.rssiDbm);
  EXPECT_EQ(100u, frame.receivedAtMs);

  ASSERT_EQ(Status::Ok, radio.receive(frame, 0));
  EXPECT_EQ(3u, frame.length);
  EXPECT_EQ(0, std::memcmp(second, frame.data, sizeof(second)));

  EXPECT_EQ(Status::Timeout, radio.receive(frame, 0));
}

TEST(MockRadio, RejectsQueueOverflowAndBadTransmit) {
  MockRadio radio;
  ASSERT_EQ(Status::Ok, radio.begin(RadioConfig{}));

  const uint8_t payload[] = {1};
  for (size_t i = 0; i < MockRadio::kQueueDepth; ++i) {
    ASSERT_TRUE(radio.queueFrame(payload, sizeof(payload)));
  }
  EXPECT_FALSE(radio.queueFrame(payload, sizeof(payload)));

  EXPECT_EQ(Status::InvalidArgument, radio.transmit(nullptr, 0));
  const uint8_t tooLarge[kMaxRadioFrameBytes + 1] = {};
  EXPECT_EQ(Status::InvalidArgument, radio.transmit(tooLarge, sizeof(tooLarge)));
}

TEST(MockRadio, RecordsTransmissions) {
  MockRadio radio;
  ASSERT_EQ(Status::Ok, radio.begin(RadioConfig{}));

  const uint8_t payload[] = {0xAA, 0xBB, 0xCC};
  ASSERT_EQ(Status::Ok, radio.transmit(payload, sizeof(payload)));
  EXPECT_EQ(1u, radio.transmitCount());
  EXPECT_EQ(sizeof(payload), radio.lastTransmission().length);
  EXPECT_EQ(0, std::memcmp(payload, radio.lastTransmission().data, sizeof(payload)));
}

}  // namespace
