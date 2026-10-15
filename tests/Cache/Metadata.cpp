////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include <stdint.h>
#include <limits>
#include <memory>

#include "Basics/SpinLocker.h"
#include "Cache/Manager.h"
#include "Cache/Metadata.h"
#include "Cache/PlainCache.h"
#include "Cache/Table.h"

using namespace arangodb::cache;
using SpinLocker = ::arangodb::basics::SpinLocker;

TEST(CacheMetadataTest, test_basic_constructor) {
  std::uint64_t usageLimit = 1024;
  std::uint64_t fixed = 128;
  std::uint64_t table = Table::allocationSize(Table::kMinLogSize);
  std::uint64_t max = UINT64_MAX;
  Metadata metadata(usageLimit, fixed, table, max);

  ASSERT_EQ(metadata.fixedSize, fixed);
  ASSERT_EQ(metadata.tableSize, table);
  ASSERT_EQ(metadata.maxSize, max);
  ASSERT_TRUE(metadata.allocatedSize > (usageLimit + fixed + table));
  ASSERT_EQ(metadata.deservedSize, metadata.allocatedSize);

  ASSERT_EQ(metadata.usage, 0);
  ASSERT_EQ(metadata.softUsageLimit, usageLimit);
  ASSERT_EQ(metadata.hardUsageLimit, usageLimit);
}

TEST(CacheMetadataTest,
     verify_usage_limits_are_adjusted_and_enforced_correctly) {
  std::uint64_t overhead = 80;
  Metadata metadata(1024, 0, 0, 2048 + overhead);

  SpinLocker guard(SpinLocker::Mode::Write, metadata.lock());

  ASSERT_TRUE(metadata.adjustUsageIfAllowed(512));
  ASSERT_TRUE(metadata.adjustUsageIfAllowed(512));
  ASSERT_FALSE(metadata.adjustUsageIfAllowed(512));

  ASSERT_FALSE(metadata.adjustLimits(2048, 2048));
  ASSERT_EQ(metadata.allocatedSize, 1024 + overhead);
  ASSERT_TRUE(metadata.adjustDeserved(2048 + overhead));
  ASSERT_TRUE(metadata.adjustLimits(2048, 2048));
  ASSERT_EQ(metadata.allocatedSize, 2048 + overhead);

  ASSERT_TRUE(metadata.adjustUsageIfAllowed(1024));

  ASSERT_TRUE(metadata.adjustLimits(1024, 2048));
  ASSERT_EQ(metadata.allocatedSize, 2048 + overhead);

  ASSERT_FALSE(metadata.adjustUsageIfAllowed(512));
  ASSERT_TRUE(metadata.adjustUsageIfAllowed(-512));
  ASSERT_TRUE(metadata.adjustUsageIfAllowed(512));
  ASSERT_TRUE(metadata.adjustUsageIfAllowed(-1024));
  ASSERT_FALSE(metadata.adjustUsageIfAllowed(512));

  ASSERT_TRUE(metadata.adjustLimits(1024, 1024));
  ASSERT_EQ(metadata.allocatedSize, 1024 + overhead);
  ASSERT_FALSE(metadata.adjustLimits(512, 512));

  ASSERT_FALSE(metadata.adjustLimits(2049, 2049));
  ASSERT_EQ(metadata.allocatedSize, 1024 + overhead);
}

TEST(CacheMetadataTest, verify_table_methods_work_correctly) {
  std::uint64_t overhead = 80;
  Metadata metadata(1024, 0, 512, 2048 + overhead);

  SpinLocker guard(SpinLocker::Mode::Write, metadata.lock());

  ASSERT_FALSE(metadata.migrationAllowed(1024));
  ASSERT_EQ(2048 + overhead, metadata.adjustDeserved(2048 + overhead));

  ASSERT_TRUE(metadata.migrationAllowed(1024));
  metadata.changeTable(1024);
  ASSERT_EQ(metadata.tableSize, 1024);
  ASSERT_EQ(metadata.allocatedSize, 2048 + overhead);

  ASSERT_FALSE(metadata.migrationAllowed(1025));
  ASSERT_TRUE(metadata.migrationAllowed(512));
  metadata.changeTable(512);
  ASSERT_EQ(metadata.tableSize, 512);
  ASSERT_EQ(metadata.allocatedSize, 1536 + overhead);
}

// regression test: newLimit() computed 4 * hardUsageLimit, which overflows for
// hardUsageLimit >= 2^62 and yielded a tiny (or zero) new limit
TEST(CacheMetadataTest, new_limit_does_not_overflow) {
  constexpr std::uint64_t max = std::numeric_limits<std::uint64_t>::max();
  std::uint64_t usageLimit = std::uint64_t(1) << 62;
  Metadata metadata(usageLimit, 0, 0, max);

  SpinLocker guard(SpinLocker::Mode::Write, metadata.lock());

  // deserved == allocated, so the new limit is the current hard limit
  ASSERT_EQ(usageLimit, metadata.newLimit());

  // with a huge deserved size, the new limit must not shrink
  metadata.adjustDeserved(max);
  ASSERT_EQ(max - Manager::kCacheRecordOverhead, metadata.newLimit());
  ASSERT_GE(metadata.newLimit(), metadata.hardUsageLimit);
}

// regression test: adjustLimits() checked (limit + fixed <= deserved), which
// wrapped around for huge limits and approved them, making allocatedSize
// overflow
TEST(CacheMetadataTest, adjust_limits_rejects_overflowing_limits) {
  constexpr std::uint64_t max = std::numeric_limits<std::uint64_t>::max();
  std::uint64_t fixed = 128;
  std::uint64_t table = Table::allocationSize(Table::kMinLogSize);
  Metadata metadata(1024, fixed, table, max);

  SpinLocker guard(SpinLocker::Mode::Write, metadata.lock());

  std::uint64_t allocatedBefore = metadata.allocatedSize;
  std::uint64_t huge = max - 16;
  ASSERT_FALSE(metadata.adjustLimits(huge, huge));
  ASSERT_EQ(1024, metadata.softUsageLimit);
  ASSERT_EQ(1024, metadata.hardUsageLimit);
  ASSERT_EQ(allocatedBefore, metadata.allocatedSize);
}

// regression test: migrationAllowed() must not approve a table size whose sum
// with the other sizes wraps around
TEST(CacheMetadataTest, migration_allowed_does_not_overflow) {
  constexpr std::uint64_t max = std::numeric_limits<std::uint64_t>::max();
  Metadata metadata(1024, 0, 512, max);

  SpinLocker guard(SpinLocker::Mode::Write, metadata.lock());

  ASSERT_FALSE(metadata.migrationAllowed(max - 512));
}

// decreasing the usage must always succeed, even if the usage is above the
// (already lowered) hard limit. otherwise freed memory is never accounted for
TEST(CacheMetadataTest, usage_decrease_always_allowed) {
  Metadata metadata(1024, 0, 0, 4096);

  SpinLocker guard(SpinLocker::Mode::Write, metadata.lock());

  metadata.usage.store(2048);
  ASSERT_TRUE(metadata.adjustUsageIfAllowed(-512));
  ASSERT_EQ(1536, metadata.usage.load());
  ASSERT_FALSE(metadata.adjustUsageIfAllowed(1));
}
