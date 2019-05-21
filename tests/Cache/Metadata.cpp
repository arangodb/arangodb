////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for arangodb::cache::Metadata
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Daniel H. Larkin
/// @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"
#include "Cache/Metadata.h"
#include "Cache/PlainCache.h"
#include "Cache/Table.h"

#include "gtest/gtest.h"

#include <stdint.h>
#include <memory>

using namespace arangodb::cache;

TEST(CacheMetadataTest, test_basic_constructor) {
  uint64_t usageLimit = 1024;
  uint64_t fixed = 128;
  uint64_t table = Table::allocationSize(Table::minLogSize);
  uint64_t max = UINT64_MAX;
  Metadata metadata(usageLimit, fixed, table, max);

  ASSERT_TRUE(metadata.fixedSize == fixed);
  ASSERT_TRUE(metadata.tableSize == table);
  ASSERT_TRUE(metadata.maxSize == max);
  ASSERT_TRUE(metadata.allocatedSize > (usageLimit + fixed + table));
  ASSERT_TRUE(metadata.deservedSize == metadata.allocatedSize);

  ASSERT_TRUE(metadata.usage == 0);
  ASSERT_TRUE(metadata.softUsageLimit == usageLimit);
  ASSERT_TRUE(metadata.hardUsageLimit == usageLimit);
}

TEST(CacheMetadataTest, verify_usage_limits_are_adjusted_and_enforced_correctly) {
  uint64_t overhead = 80;
  Metadata metadata(1024, 0, 0, 2048 + overhead);

  metadata.writeLock();

  ASSERT_TRUE(metadata.adjustUsageIfAllowed(512));
  ASSERT_TRUE(metadata.adjustUsageIfAllowed(512));
  ASSERT_TRUE(!metadata.adjustUsageIfAllowed(512));

  ASSERT_TRUE(!metadata.adjustLimits(2048, 2048));
  ASSERT_TRUE(metadata.allocatedSize == 1024 + overhead);
  ASSERT_TRUE(metadata.adjustDeserved(2048 + overhead));
  ASSERT_TRUE(metadata.adjustLimits(2048, 2048));
  ASSERT_TRUE(metadata.allocatedSize == 2048 + overhead);

  ASSERT_TRUE(metadata.adjustUsageIfAllowed(1024));

  ASSERT_TRUE(metadata.adjustLimits(1024, 2048));
  ASSERT_TRUE(metadata.allocatedSize == 2048 + overhead);

  ASSERT_TRUE(!metadata.adjustUsageIfAllowed(512));
  ASSERT_TRUE(metadata.adjustUsageIfAllowed(-512));
  ASSERT_TRUE(metadata.adjustUsageIfAllowed(512));
  ASSERT_TRUE(metadata.adjustUsageIfAllowed(-1024));
  ASSERT_TRUE(!metadata.adjustUsageIfAllowed(512));

  ASSERT_TRUE(metadata.adjustLimits(1024, 1024));
  ASSERT_TRUE(metadata.allocatedSize == 1024 + overhead);
  ASSERT_TRUE(!metadata.adjustLimits(512, 512));

  ASSERT_TRUE(!metadata.adjustLimits(2049, 2049));
  ASSERT_TRUE(metadata.allocatedSize == 1024 + overhead);

  metadata.writeUnlock();
}

TEST(CacheMetadataTest, verify_table_methods_work_correctly) {
  uint64_t overhead = 80;
  Metadata metadata(1024, 0, 512, 2048 + overhead);

  metadata.writeLock();

  ASSERT_TRUE(!metadata.migrationAllowed(1024));
  ASSERT_TRUE(2048 + overhead == metadata.adjustDeserved(2048 + overhead));

  ASSERT_TRUE(metadata.migrationAllowed(1024));
  metadata.changeTable(1024);
  ASSERT_TRUE(metadata.tableSize == 1024);
  ASSERT_TRUE(metadata.allocatedSize == 2048 + overhead);

  ASSERT_TRUE(!metadata.migrationAllowed(1025));
  ASSERT_TRUE(metadata.migrationAllowed(512));
  metadata.changeTable(512);
  ASSERT_TRUE(metadata.tableSize == 512);
  ASSERT_TRUE(metadata.allocatedSize == 1536 + overhead);

  metadata.writeUnlock();
}
