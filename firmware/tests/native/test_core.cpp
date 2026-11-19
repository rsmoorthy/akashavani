#include <gtest/gtest.h>

#include <cstring>

#include "Crc.h"
#include "DeviceId.h"
#include "Hex.h"
#include "Status.h"
#include "TimeUtils.h"

namespace {

using namespace akashavani;

TEST(Status, StringNamesAreStable) {
  EXPECT_STREQ("ok", statusToString(Status::Ok));
  EXPECT_STREQ("invalid-argument", statusToString(Status::InvalidArgument));
  EXPECT_STREQ("unsupported", statusToString(Status::Unsupported));
  EXPECT_TRUE(isOk(Status::Ok));
  EXPECT_FALSE(isOk(Status::Timeout));
}

TEST(Crc, KnownCheckVectors) {
  const uint8_t data[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
  EXPECT_EQ(0x29B1, crc16Ccitt(data, sizeof(data)));
  EXPECT_EQ(0x31C3, crc16Xmodem(data, sizeof(data)));
  EXPECT_EQ(0xBB3D, crc16Arc(data, sizeof(data)));
  EXPECT_EQ(0xFFFF, crc16Ccitt(nullptr, 0));
}

TEST(Crc, HandlesEmptyAndNullSafely) {
  EXPECT_EQ(0xFFFF, crc16Ccitt(nullptr, 0));
  EXPECT_EQ(0x0000, crc16Arc(nullptr, 0));
  EXPECT_EQ(0x0000, crc16Ccitt(nullptr, 4));
  EXPECT_EQ(0x0000, crc16Arc(nullptr, 4));
}

TEST(Hex, RoundTripAndCase) {
  const uint8_t bytes[] = {0x00, 0x0F, 0xA5, 0xFF};
  char lower[9];
  char upper[9];
  ASSERT_EQ(8u, toHex(bytes, sizeof(bytes), lower, sizeof(lower), false));
  ASSERT_EQ(8u, toHex(bytes, sizeof(bytes), upper, sizeof(upper), true));
  EXPECT_STREQ("000fa5ff", lower);
  EXPECT_STREQ("000FA5FF", upper);

  uint8_t decoded[4] = {};
  size_t written = 0;
  ASSERT_TRUE(fromHex("000fa5ff", decoded, sizeof(decoded), written));
  EXPECT_EQ(4u, written);
  EXPECT_EQ(0, std::memcmp(bytes, decoded, sizeof(bytes)));
}

TEST(Hex, RejectsInvalidInput) {
  uint8_t output[8] = {};
  size_t written = 99;
  EXPECT_FALSE(fromHex("abc", output, sizeof(output), written));
  EXPECT_FALSE(fromHex("zz", output, sizeof(output), written));
  EXPECT_FALSE(fromHex("000102030405060708", output, sizeof(output), written));

  char tooSmall[3];
  const uint8_t bytes[] = {0xAA, 0xBB};
  EXPECT_EQ(0u, toHex(bytes, sizeof(bytes), tooSmall, sizeof(tooSmall)));
}

TEST(Time, FormatsKnownInstant) {
  char buffer[21];
  ASSERT_TRUE(formatIso8601Utc(1792908000, buffer, sizeof(buffer)));
  EXPECT_STREQ("2026-10-25T06:00:00Z", buffer);
}

TEST(Time, ParsesRoundTrip) {
  const int64_t expected = 1792908000;
  char buffer[21];
  ASSERT_TRUE(formatIso8601Utc(expected, buffer, sizeof(buffer)));
  int64_t parsed = 0;
  ASSERT_TRUE(parseIso8601Utc(buffer, parsed));
  EXPECT_EQ(expected, parsed);
}

TEST(Time, HandlesLeapDay) {
  char buffer[21];
  ASSERT_TRUE(formatIso8601Utc(1709251199, buffer, sizeof(buffer)));
  EXPECT_STREQ("2024-02-29T23:59:59Z", buffer);
  int64_t parsed = 0;
  ASSERT_TRUE(parseIso8601Utc(buffer, parsed));
  EXPECT_EQ(1709251199, parsed);
}

TEST(Time, RejectsMalformedValues) {
  int64_t parsed = 0;
  EXPECT_FALSE(parseIso8601Utc(nullptr, parsed));
  EXPECT_FALSE(parseIso8601Utc("2026-10-25 06:00:00Z", parsed));
  EXPECT_FALSE(parseIso8601Utc("2026-13-01T00:00:00Z", parsed));
  EXPECT_FALSE(parseIso8601Utc("2026-02-30T00:00:00Z", parsed));
  EXPECT_FALSE(parseIso8601Utc("2026-10-25T24:00:00Z", parsed));

  char tooSmall[10];
  EXPECT_FALSE(formatIso8601Utc(1792908000, tooSmall, sizeof(tooSmall)));
}

TEST(DeviceId, ValidationRules) {
  EXPECT_TRUE(isValidDeviceId("ak-3f2a1b"));
  EXPECT_TRUE(isValidDeviceId("abc"));
  EXPECT_TRUE(isValidDeviceId("a-1"));
  EXPECT_FALSE(isValidDeviceId(nullptr));
  EXPECT_FALSE(isValidDeviceId(""));
  EXPECT_FALSE(isValidDeviceId("ab"));
  EXPECT_FALSE(isValidDeviceId("Abc"));
  EXPECT_FALSE(isValidDeviceId("ab-"));
  EXPECT_FALSE(isValidDeviceId("ab cd"));
  EXPECT_FALSE(isValidDeviceId("abcdefghijklmnopqrstuvwxyz0123456"));
}

TEST(DeviceId, DerivesFromMac) {
  const uint8_t mac[6] = {0x00, 0x11, 0x22, 0x3F, 0x2A, 0x1B};
  char id[12];
  ASSERT_TRUE(makeDeviceIdFromMac(mac, id, sizeof(id)));
  EXPECT_STREQ("ak-3f2a1b", id);

  char tooSmall[8];
  EXPECT_FALSE(makeDeviceIdFromMac(mac, tooSmall, sizeof(tooSmall)));
}

}  // namespace
