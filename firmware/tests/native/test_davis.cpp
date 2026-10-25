#include <gtest/gtest.h>

#include <cmath>
#include <cstring>

#include "DavisConstants.h"
#include "DavisDecoder.h"
#include "DavisEncoder.h"
#include "DavisObservation.h"
#include "DavisSimulator.h"

namespace {

using namespace akashvani;

TEST(DavisDecoder, RejectsEmptyAndShortFrames) {
  const uint8_t frame[kDavisMaxFrameBytes] = {};
  EXPECT_EQ(DavisDecodeStatus::TooShort, decodeDavisFrame(nullptr, 0).status);
  EXPECT_EQ(DavisDecodeStatus::TooShort,
            decodeDavisFrame(frame, kDavisMinFrameBytes - 1).status);
  EXPECT_EQ(DavisDecodeStatus::UnsupportedProtocol,
            decodeDavisFrame(frame, kDavisMinFrameBytes).status);
}

TEST(DavisDecoder, RejectsOversizedFrames) {
  const uint8_t frame[kDavisMaxFrameBytes + 1] = {};
  EXPECT_EQ(DavisDecodeStatus::TooLong, decodeDavisFrame(frame, sizeof(frame)).status);
}

TEST(DavisDecoder, StatusStringsExist) {
  EXPECT_STREQ("ok", toString(DavisDecodeStatus::Ok));
  EXPECT_STREQ("crc-mismatch", toString(DavisDecodeStatus::CrcMismatch));
  EXPECT_STREQ("buffer-too-small", toString(DavisEncodeStatus::BufferTooSmall));
}

TEST(DavisEncoder, StubDoesNotClaimSuccess) {
  DavisObservation observation;
  observation.set(DavisField::Temperature, 20.0f);
  uint8_t buffer[32];
  size_t written = 99;
  EXPECT_EQ(DavisEncodeStatus::UnsupportedProtocol,
            encodeDavisFrame(observation, buffer, sizeof(buffer), written));
  EXPECT_EQ(0u, written);
}

TEST(DavisObservation, ValidityMaskTracksFields) {
  DavisObservation observation;
  EXPECT_FALSE(observation.has(DavisField::Temperature));
  observation.set(DavisField::Temperature, 19.5f);
  EXPECT_TRUE(observation.has(DavisField::Temperature));
  EXPECT_FLOAT_EQ(19.5f, observation.temperatureC);
  EXPECT_FALSE(observation.has(DavisField::Humidity));

  observation.clear();
  EXPECT_FALSE(observation.has(DavisField::Temperature));
  EXPECT_TRUE(std::isnan(observation.temperatureC));
}

TEST(DavisSimulator, IsDeterministic) {
  DavisSimulator first(12345, 2);
  DavisSimulator second(12345, 2);
  for (int i = 0; i < 50; ++i) {
    const DavisObservation a = first.next();
    const DavisObservation b = second.next();
    EXPECT_EQ(a.sequence, b.sequence);
    EXPECT_FLOAT_EQ(a.temperatureC, b.temperatureC);
    EXPECT_FLOAT_EQ(a.humidityPct, b.humidityPct);
    EXPECT_FLOAT_EQ(a.pressureHpa, b.pressureHpa);
    EXPECT_FLOAT_EQ(a.windSpeedMs, b.windSpeedMs);
    EXPECT_FLOAT_EQ(a.rainDayMm, b.rainDayMm);
  }
}

TEST(DavisSimulator, ProducesBoundedCompleteObservations) {
  DavisSimulator simulator(20261025, 1);
  for (int i = 0; i < 1000; ++i) {
    const DavisObservation observation = simulator.next();
    EXPECT_EQ(kAllDavisFields, observation.validFields);
    EXPECT_GE(observation.temperatureC, -25.0f);
    EXPECT_LE(observation.temperatureC, 48.0f);
    EXPECT_GE(observation.humidityPct, 1.0f);
    EXPECT_LE(observation.humidityPct, 100.0f);
    EXPECT_GE(observation.pressureHpa, 950.0f);
    EXPECT_LE(observation.pressureHpa, 1050.0f);
    EXPECT_GE(observation.windSpeedMs, 0.0f);
    EXPECT_GE(observation.windDirectionDeg, 0.0f);
    EXPECT_LT(observation.windDirectionDeg, 360.0f);
    EXPECT_GE(observation.rainDayMm, 0.0f);
    EXPECT_GE(observation.uvIndex, 0.0f);
    EXPECT_LE(observation.uvIndex, 12.0f);
    EXPECT_GE(observation.batteryVoltageV, 2.4f);
    EXPECT_LE(observation.batteryVoltageV, 3.6f);
    EXPECT_EQ(static_cast<uint32_t>(i + 1), observation.sequence);
  }
}

TEST(DavisSimulator, ClampsTransmitterId) {
  DavisSimulator simulator(1, 0);
  EXPECT_EQ(1, static_cast<int>(simulator.transmitterId()));
  simulator.setTransmitterId(99);
  EXPECT_EQ(8, static_cast<int>(simulator.transmitterId()));
}

}  // namespace
