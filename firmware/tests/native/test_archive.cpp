#include <gtest/gtest.h>

#include <cmath>

#include "ArchiveCollector.h"
#include "DavisObservation.h"

namespace {

using namespace akashvani;

DavisObservation makeObservation(float windSpeedMs, float windDirDeg) {
  DavisObservation observation;
  observation.set(DavisField::WindSpeed, windSpeedMs);
  observation.set(DavisField::WindDirection, windDirDeg);
  return observation;
}

TEST(ArchiveCollector, ProducesFiveMinuteRecordAndSums) {
  ArchiveCollector collector(300, 3000);
  const int64_t start = 1793547000;  // arbitrary epoch, not a bucket boundary

  DavisObservation temperature = makeObservation(2.0f, 90.0f);
  temperature.set(DavisField::Temperature, 20.0f);
  temperature.set(DavisField::Humidity, 50.0f);
  temperature.set(DavisField::WindGust, 4.0f);
  temperature.set(DavisField::BatteryVoltage, 3.2f);
  ASSERT_TRUE(collector.add(temperature, -50, false, start));

  DavisObservation second = makeObservation(4.0f, 270.0f);
  second.set(DavisField::Temperature, 22.0f);
  second.set(DavisField::Humidity, 60.0f);
  second.set(DavisField::WindGust, 6.0f);
  second.set(DavisField::BatteryVoltage, 3.0f);
  ASSERT_TRUE(collector.add(second, -60, true, start + 3));

  ASSERT_TRUE(collector.addBme280(25.0f, 40.0f, 910.0f, start + 4));
  ASSERT_TRUE(collector.addBme280(27.0f, 50.0f, 912.0f, start + 5));

  ArchiveRecord record;
  ASSERT_FALSE(collector.poll(start + 10, record));
  ASSERT_TRUE(collector.poll(start + 300, record));

  EXPECT_EQ(5u, record.intervalMinutes);
  EXPECT_EQ(2u, record.framesValid);
  EXPECT_EQ(2u, record.outTempCount);
  EXPECT_NEAR(42.0, record.outTempCSum, 0.001);
  EXPECT_EQ(2u, record.outHumidityCount);
  EXPECT_NEAR(110.0, record.outHumidityPctSum, 0.001);
  EXPECT_EQ(2u, record.windSpeedCount);
  EXPECT_NEAR(6.0, record.windSpeedMsSum, 0.001);
  EXPECT_EQ(2u, record.windDirCount);
  EXPECT_NEAR(6.0, record.windGustMaxMs, 0.001);
  EXPECT_NEAR(270.0, record.windGustDirDeg, 0.001);
  EXPECT_EQ(2u, record.inTempCount);
  EXPECT_NEAR(52.0, record.inTempCSum, 0.001);
  EXPECT_EQ(2u, record.pressureCount);
  EXPECT_NEAR(1822.0, record.pressureHpaSum, 0.001);
  EXPECT_TRUE(record.batteryLow);
  EXPECT_EQ(2u, record.supercapCount);
  EXPECT_NEAR(3.0, record.supercapMinV, 0.001);
  EXPECT_EQ(2u, record.rssiCount);
  EXPECT_EQ(-60, record.rssiMin);
  EXPECT_EQ(-50, record.rssiMax);
}

TEST(ArchiveCollector, RainCounterAccumulatesAndHandlesWrap) {
  ArchiveCollector collector(300, 3000);
  const int64_t start = 1793547000;

  DavisObservation first;
  first.rainCounter = 126;
  first.rainCounterValid = true;
  ASSERT_TRUE(collector.add(first, -50, false, start));

  DavisObservation second;
  second.rainCounter = 2;
  second.rainCounterValid = true;
  ASSERT_TRUE(collector.add(second, -50, false, start + 3));

  ArchiveRecord record;
  ASSERT_TRUE(collector.poll(start + 300, record));
  EXPECT_EQ(4u, record.rainTips);  // 126 -> 127, 0, 1, 2
  EXPECT_EQ(2u, record.rainCount);
}

TEST(ArchiveCollector, RawRainRateIsCarriedAndNotConverted) {
  ArchiveCollector collector(300, 3000);
  const int64_t start = 1793547000;

  DavisObservation observation = makeObservation(1.0f, 10.0f);
  observation.rainRateValid = true;
  observation.rainRateRaw = 720;
  observation.rainRateHigh = false;
  ASSERT_TRUE(collector.add(observation, -50, false, start));

  ArchiveRecord record;
  ASSERT_TRUE(collector.poll(start + 300, record));
  EXPECT_TRUE(record.rainRateValid);
  EXPECT_EQ(720u, record.rainRateRaw);
  EXPECT_FALSE(record.rainRateHigh);
}

TEST(ArchiveCollector, VectorDirectionSumsHandleWrap) {
  ArchiveCollector collector(300, 3000);
  const int64_t start = 1793547000;

  ASSERT_TRUE(collector.add(makeObservation(1.0f, 350.0f), -50, false, start));
  ASSERT_TRUE(collector.add(makeObservation(1.0f, 10.0f), -50, false, start + 3));

  ArchiveRecord record;
  ASSERT_TRUE(collector.poll(start + 300, record));
  EXPECT_EQ(2u, record.windDirCount);
  EXPECT_NEAR(0.98480775, record.windDirCosSum / 2.0, 0.0001);
  EXPECT_NEAR(0.0, record.windDirSinSum, 0.01);
}

TEST(ArchiveCollector, BmeOnlyBucketIsEmitted) {
  // Every 5-minute bucket doubles as the radio-health report, even when no
  // Davis frame arrived.
  ArchiveCollector collector(300, 3000);
  const int64_t start = 1793547000;
  ASSERT_TRUE(collector.addBme280(25.0f, 40.0f, 910.0f, start));
  ArchiveRecord record;
  ASSERT_TRUE(collector.poll(start + 300, record));
  ASSERT_EQ(record.framesValid, 0u);
  ASSERT_EQ(record.framesExpected, 100u);
}

TEST(ArchiveCollector, CurrentBucketExposesFrameQuality) {
  ArchiveCollector collector(300, 3000);
  const int64_t start = 1793547000;

  EXPECT_EQ(100u, collector.expectedFrames());
  EXPECT_EQ(0u, collector.current().framesValid);
  EXPECT_EQ(0u, collector.current().framesCrcBad);

  ASSERT_TRUE(collector.add(makeObservation(1.0f, 10.0f), -50, false, start));
  collector.noteBadFrames(2, -80, -160, -80);

  EXPECT_EQ(1u, collector.current().framesValid);
  EXPECT_EQ(100u, collector.current().framesExpected);
  EXPECT_EQ(2u, collector.current().framesCrcBad);
}

TEST(ArchiveCollector, EmptyBucketIsEmitted) {
  ArchiveCollector collector(300, 3000);
  const int64_t start = 1793547000;
  ASSERT_TRUE(collector.add(makeObservation(1.0f, 10.0f), -50, false, start));
  ArchiveRecord record;
  ASSERT_TRUE(collector.poll(start + 300, record));
  ASSERT_TRUE(collector.poll(start + 600, record));
  ASSERT_EQ(record.framesValid, 0u);
  ASSERT_EQ(record.framesExpected, 100u);
  ASSERT_FALSE(collector.poll(start + 600, record));
}

}  // namespace
