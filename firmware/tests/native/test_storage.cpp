#include <gtest/gtest.h>

#include <cstring>

#include "ArchivePath.h"

namespace {

using namespace akashavani;

TEST(ArchivePath, BuildsDailyJsonLinesPath) {
  char path[kArchivePathMax];
  ASSERT_TRUE(buildArchivePath("ak-3f2a1b", 1792908000, path, sizeof(path)));
  EXPECT_STREQ("/archive/ak-3f2a1b/2026/10/25.jsonl", path);
}

TEST(ArchivePath, BuildsMonthDirectory) {
  char path[kArchivePathMax];
  ASSERT_TRUE(buildArchiveDirectory("ak-3f2a1b", 1792908000, path, sizeof(path)));
  EXPECT_STREQ("/archive/ak-3f2a1b/2026/10", path);
}

TEST(ArchivePath, HandlesEpochZero) {
  char path[kArchivePathMax];
  ASSERT_TRUE(buildArchivePath("ak-3f2a1b", 0, path, sizeof(path)));
  EXPECT_STREQ("/archive/ak-3f2a1b/1970/01/01.jsonl", path);
}

TEST(ArchivePath, RejectsInvalidArguments) {
  char path[kArchivePathMax];
  EXPECT_FALSE(buildArchivePath("", 1792908000, path, sizeof(path)));
  EXPECT_FALSE(buildArchivePath("BAD", 1792908000, path, sizeof(path)));
  EXPECT_FALSE(buildArchivePath("ak-3f2a1b", 1792908000, nullptr, sizeof(path)));
  EXPECT_FALSE(buildArchivePath("ak-3f2a1b", 1792908000, path, 8));
}

}  // namespace
