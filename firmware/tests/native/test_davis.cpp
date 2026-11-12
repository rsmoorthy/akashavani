#include <gtest/gtest.h>

#include <cmath>
#include <cstring>

#include "DavisConstants.h"
#include "DavisCrc.h"
#include "DavisDecoder.h"
#include "DavisEncoder.h"
#include "DavisObservation.h"
#include "DavisSimulator.h"

namespace {

using namespace akashvani;

TEST(DavisCrc, MatchesCapturedFrames) {
  const uint8_t first[6] = {0x87, 0x05, 0x2D, 0x32, 0x5B, 0x0F};
  EXPECT_EQ(0xB991u, davisCrc16(first, sizeof(first)));

  const uint8_t second[6] = {0xE7, 0x05, 0x3E, 0x83, 0x01, 0x0E};
  EXPECT_EQ(0x5E68u, davisCrc16(second, sizeof(second)));

  const uint8_t third[6] = {0x57, 0x04, 0x17, 0xFF, 0x71, 0x0B};
  EXPECT_EQ(0x3E39u, davisCrc16(third, sizeof(third)));
}

TEST(DavisDecoder, RejectsEmptyShortAndOversizedFrames) {
  const uint8_t frame[kDavisMaxFrameBytes + 1] = {};
  EXPECT_EQ(DavisDecodeStatus::TooShort, decodeDavisFrame(nullptr, 0).status);
  EXPECT_EQ(DavisDecodeStatus::TooShort,
            decodeDavisFrame(frame, kDavisMinFrameBytes - 1).status);
  EXPECT_EQ(DavisDecodeStatus::TooLong, decodeDavisFrame(frame, sizeof(frame)).status);
}

TEST(DavisDecoder, DecodesCapturedTemperatureFrame) {
  const uint8_t frame[8] = {0x87, 0x05, 0x2D, 0x32, 0x5B, 0x0F, 0xB9, 0x91};
  const DavisDecodeResult result = decodeDavisFrame(frame, sizeof(frame));
  ASSERT_EQ(DavisDecodeStatus::Ok, result.status);
  EXPECT_EQ(8u, static_cast<unsigned>(result.messageId));
  EXPECT_EQ(DavisMessageType::Temperature, result.messageType);
  EXPECT_EQ(7u, static_cast<unsigned>(result.observation.transmitterId));
  EXPECT_FALSE(result.batteryLow);
  EXPECT_TRUE(result.observation.has(DavisField::WindSpeed));
  EXPECT_TRUE(result.observation.has(DavisField::WindDirection));
  EXPECT_TRUE(result.observation.has(DavisField::Temperature));
  EXPECT_NEAR(5.0 * 0.44704, result.observation.windSpeedMs, 0.001);
  EXPECT_NEAR(26.98, result.observation.temperatureC, 0.02);
}

TEST(DavisDecoder, RejectsCorruptCrcAndKeepsMessageName) {
  uint8_t frame[8] = {0x87, 0x05, 0x2D, 0x32, 0x5B, 0x2A, 0xB9, 0x91};
  frame[7] ^= 0x01;
  const DavisDecodeResult result = decodeDavisFrame(frame, sizeof(frame));
  EXPECT_EQ(DavisDecodeStatus::CrcMismatch, result.status);
  EXPECT_STREQ("crc-mismatch", toString(result.status));
}

TEST(DavisDecoder, DecodesCapturedUvFrame) {
  // Captured ISS frame, previously rejected as an unknown message ID.
  const uint8_t frame[8] = {0x47, 0x05, 0x15, 0x24, 0x41, 0x00, 0x42, 0x1F};
  const DavisDecodeResult result = decodeDavisFrame(frame, sizeof(frame));
  ASSERT_EQ(DavisDecodeStatus::Ok, result.status);
  EXPECT_EQ(4u, static_cast<unsigned>(result.messageId));
  EXPECT_EQ(DavisMessageType::UvIndex, result.messageType);
  EXPECT_TRUE(result.observation.has(DavisField::UvIndex));
  // raw = (0x24 << 2) | (0x41 >> 6) = 145; UV index = 145 / 50.
  EXPECT_NEAR(2.9, result.observation.uvIndex, 0.001);
  EXPECT_TRUE(result.observation.has(DavisField::WindSpeed));
}

TEST(DavisDecoder, DecodesCapturedSolarFrame) {
  const uint8_t frame[8] = {0x67, 0x03, 0x57, 0x54, 0x43, 0x0B, 0x36, 0x67};
  const DavisDecodeResult result = decodeDavisFrame(frame, sizeof(frame));
  ASSERT_EQ(DavisDecodeStatus::Ok, result.status);
  EXPECT_EQ(6u, static_cast<unsigned>(result.messageId));
  EXPECT_EQ(DavisMessageType::SolarRadiationWm2, result.messageType);
  EXPECT_TRUE(result.observation.has(DavisField::SolarRadiation));
  // raw = (0x54 << 2) | (0x43 >> 6) = 337; W/m2 = 337 * 1.757936.
  EXPECT_NEAR(337.0 * 1.757936, result.observation.solarRadiationWm2, 0.01);
}

TEST(DavisDecoder, UvAndSolarMissingSentinelsKeepTheFrame) {
  uint8_t uv[8] = {0x47, 0x05, 0x15, 0xFF, 0xFF, 0x00, 0x00, 0x00};
  uint16_t crc = davisCrc16(uv, 6);
  uv[6] = static_cast<uint8_t>(crc >> 8);
  uv[7] = static_cast<uint8_t>(crc & 0xFF);
  DavisDecodeResult uvResult = decodeDavisFrame(uv, sizeof(uv));
  ASSERT_EQ(DavisDecodeStatus::Ok, uvResult.status);
  EXPECT_FALSE(uvResult.observation.has(DavisField::UvIndex));
  EXPECT_TRUE(uvResult.observation.has(DavisField::WindSpeed));

  uint8_t solar[8] = {0x67, 0x03, 0x57, 0xFF, 0x80, 0x00, 0x00, 0x00};
  crc = davisCrc16(solar, 6);
  solar[6] = static_cast<uint8_t>(crc >> 8);
  solar[7] = static_cast<uint8_t>(crc & 0xFF);
  DavisDecodeResult solarResult = decodeDavisFrame(solar, sizeof(solar));
  ASSERT_EQ(DavisDecodeStatus::Ok, solarResult.status);
  EXPECT_FALSE(solarResult.observation.has(DavisField::SolarRadiation));
  EXPECT_TRUE(solarResult.observation.has(DavisField::WindSpeed));
}

TEST(DavisCodec, RoundTripsEveryMessageType) {
  DavisObservation source;
  source.transmitterId = 3;
  source.set(DavisField::WindSpeed, 2.2352f);
  source.set(DavisField::WindDirection, 243.5f);
  source.set(DavisField::Temperature, 26.98f);
  source.set(DavisField::Humidity, 42.7f);
  source.set(DavisField::RainRate, 1.2f);
  source.set(DavisField::SolarRadiation, 350.0f);
  source.set(DavisField::WindGust, 4.5f);
  source.set(DavisField::BatteryVoltage, 3.15f);

  const DavisMessageType types[] = {
      DavisMessageType::Temperature,     DavisMessageType::Humidity,
      DavisMessageType::RainRate,        DavisMessageType::SolarRadiation,
      DavisMessageType::GustSpeed,       DavisMessageType::GoldcapVoltage,
      DavisMessageType::RainCounter,     DavisMessageType::Unknown3,
  };

  for (const DavisMessageType type : types) {
    uint8_t frame[8] = {};
    size_t written = 0;
    ASSERT_EQ(DavisEncodeStatus::Ok,
              encodeDavisFrame(source, type, frame, sizeof(frame), written, 42))
        << toString(type);
    ASSERT_EQ(8u, written);

    const DavisDecodeResult decoded = decodeDavisFrame(frame, written);
    ASSERT_EQ(DavisDecodeStatus::Ok, decoded.status) << toString(type);
    EXPECT_EQ(type, decoded.messageType);
    EXPECT_EQ(42u, static_cast<unsigned>(decoded.benchSequence));
    EXPECT_EQ(3u, static_cast<unsigned>(decoded.observation.transmitterId));
    EXPECT_NEAR(source.windSpeedMs, decoded.observation.windSpeedMs, 0.01);
    EXPECT_NEAR(source.windDirectionDeg, decoded.observation.windDirectionDeg, 1.0);

    switch (type) {
      case DavisMessageType::Temperature:
        EXPECT_NEAR(source.temperatureC, decoded.observation.temperatureC, 0.02);
        break;
      case DavisMessageType::Humidity:
        EXPECT_NEAR(source.humidityPct, decoded.observation.humidityPct, 0.05);
        break;
      case DavisMessageType::RainRate:
        EXPECT_NEAR(source.rainRateMmPerHour, decoded.observation.rainRateMmPerHour, 0.1);
        break;
      case DavisMessageType::SolarRadiation:
        EXPECT_NEAR(source.solarRadiationWm2, decoded.observation.solarRadiationWm2, 1.0);
        break;
      case DavisMessageType::GustSpeed:
        EXPECT_NEAR(source.windGustMs, decoded.observation.windGustMs, 0.25);
        break;
      case DavisMessageType::GoldcapVoltage:
        EXPECT_NEAR(source.batteryVoltageV, decoded.observation.batteryVoltageV, 0.01);
        break;
      default:
        break;
    }
  }
}

TEST(DavisEncoder, RejectsBadArgumentsAndUnknownFields) {
  DavisObservation observation;
  observation.transmitterId = 0;
  observation.set(DavisField::WindSpeed, 1.0f);
  observation.set(DavisField::WindDirection, 180.0f);
  uint8_t frame[8];
  size_t written = 99;
  EXPECT_EQ(DavisEncodeStatus::InvalidObservation,
            encodeDavisFrame(observation, DavisMessageType::Temperature, frame,
                             sizeof(frame), written));
  EXPECT_EQ(0u, written);

  observation.transmitterId = 1;
  uint8_t small[4];
  EXPECT_EQ(DavisEncodeStatus::BufferTooSmall,
            encodeDavisFrame(observation, DavisMessageType::Temperature, small,
                             sizeof(small), written));

  // Temperature is absent, so the generic overload has nothing to encode.
  EXPECT_EQ(DavisEncodeStatus::InvalidObservation,
            encodeDavisFrame(observation, frame, sizeof(frame), written));
}

TEST(DavisEncoder, GenericOverloadPrefersTemperature) {
  DavisObservation observation;
  observation.transmitterId = 1;
  observation.set(DavisField::WindSpeed, 1.0f);
  observation.set(DavisField::WindDirection, 180.0f);
  observation.set(DavisField::Temperature, 20.0f);

  uint8_t frame[8];
  size_t written = 0;
  ASSERT_EQ(DavisEncodeStatus::Ok,
            encodeDavisFrame(observation, frame, sizeof(frame), written));
  const DavisDecodeResult decoded = decodeDavisFrame(frame, written);
  ASSERT_EQ(DavisDecodeStatus::Ok, decoded.status);
  EXPECT_EQ(DavisMessageType::Temperature, decoded.messageType);
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
