////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Dan Larkin-York
/// @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include <stdint.h>
#include <memory>

#include "Basics/SpinLocker.h"
#include "Cache/Metadata.h"
#include "Cache/PlainCache.h"
#include "Cache/Table.h"

using namespace arangodb::cache;
using SpinLocker = ::arangodb::basics::SpinLocker;

TEST(CacheMetadataTest, test_basic_constructor) {
  std::uint64_t usageLimit = 1024;
  std::uint64_t fixed = 128;
  std::uint64_t table = Table::allocationSize(Table::minLogSize);
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

TEST(CacheMetadataTest, verify_usage_limits_are_adjusted_and_enforced_correctly) {
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
