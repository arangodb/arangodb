////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for arangodb::cache::State
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
/// @author Dan Larkin-York
/// @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include <cstdint>
#include <memory>

#include "Cache/Common.h"
#include "Cache/PlainBucket.h"
#include "Cache/Table.h"

using namespace arangodb::cache;

TEST(CacheTableTest, test_static_allocation_size_method) {
  for (std::uint32_t i = Table::minLogSize; i <= Table::maxLogSize; i++) {
    ASSERT_TRUE(Table::allocationSize(i) ==
                (sizeof(Table) + (BUCKET_SIZE << i) + Table::padding));
  }
}

TEST(CacheTableTest, test_basic_constructor_behavior) {
  for (std::uint32_t i = Table::minLogSize; i <= 20; i++) {
    auto table = std::make_shared<Table>(i);
    ASSERT_NE(table.get(), nullptr);
    ASSERT_EQ(table->memoryUsage(), (sizeof(Table) + (BUCKET_SIZE << i) + Table::padding));
    ASSERT_EQ(table->logSize(), i);
    ASSERT_EQ(table->size(), (static_cast<std::uint64_t>(1) << i));
  }
}

TEST(CacheTableTest, test_basic_bucket_fetching_behavior) {
  auto table = std::make_shared<Table>(Table::minLogSize);
  ASSERT_NE(table.get(), nullptr);
  table->enable();
  for (std::uint64_t i = 0; i < table->size(); i++) {
    std::uint32_t hash = static_cast<std::uint32_t>(i << (32 - Table::minLogSize));
    Table::BucketLocker guard = table->fetchAndLockBucket(hash, -1);
    ASSERT_TRUE(guard.isValid());
    ASSERT_TRUE(guard.isLocked());
    ASSERT_TRUE(guard.bucket<PlainBucket>().isLocked());
    ASSERT_NE(guard.source(), nullptr);
    ASSERT_EQ(guard.source(), table.get());

    auto rawBucket = reinterpret_cast<PlainBucket*>(table->primaryBucket(i));
    ASSERT_EQ(&guard.bucket<PlainBucket>(), rawBucket);

    Table::BucketLocker badGuard = table->fetchAndLockBucket(hash, 10);
    ASSERT_FALSE(badGuard.isValid());
    ASSERT_EQ(badGuard.source(), nullptr);
  }
}

class CacheTableMigrationTest : public ::testing::Test {
 protected:
  std::shared_ptr<Table> small;
  std::shared_ptr<Table> large;
  std::shared_ptr<Table> huge;

  CacheTableMigrationTest()
      : small(std::make_shared<Table>(Table::minLogSize)),
        large(std::make_shared<Table>(Table::minLogSize + 2)),
        huge(std::make_shared<Table>(Table::minLogSize + 4)) {
    small->enable();
    large->enable();
    huge->enable();
  }
};

TEST_F(CacheTableMigrationTest, check_that_setauxiliary_works_as_intended) {
  auto res = small->setAuxiliary(large);
  ASSERT_EQ(res.get(), nullptr);
  res = small->setAuxiliary(huge);
  ASSERT_EQ(res.get(), huge.get());
  res = small->setAuxiliary(std::shared_ptr<Table>(nullptr));
  ASSERT_EQ(res.get(), large.get());
}

TEST_F(CacheTableMigrationTest, check_that_bucket_locking_falls_through_appropriately) {
  auto res = small->setAuxiliary(large);
  ASSERT_EQ(res.get(), nullptr);

  std::uint32_t indexSmall = 17;  // picked something at "random"
  std::uint32_t indexLarge = indexSmall << 2;
  std::uint32_t hash = indexSmall << (32 - small->logSize());

  {
    Table::BucketLocker guard = small->fetchAndLockBucket(hash, -1);
    ASSERT_EQ(&guard.bucket<PlainBucket>(),
              reinterpret_cast<PlainBucket*>(small->primaryBucket(indexSmall)));
    guard.bucket<PlainBucket>()._state.toggleFlag(BucketState::Flag::migrated);
    ASSERT_EQ(guard.source(), small.get());
  }

  Table::BucketLocker guard = small->fetchAndLockBucket(hash, -1);
  ASSERT_EQ(&guard.bucket<PlainBucket>(),
            reinterpret_cast<PlainBucket*>(large->primaryBucket(indexLarge)));
  ASSERT_EQ(guard.source(), large.get());
  Table::BucketLocker busyGuard = small->fetchAndLockBucket(hash, 10);
  ASSERT_FALSE(busyGuard.isValid());
  ASSERT_EQ(busyGuard.source(), nullptr);
}

TEST_F(CacheTableMigrationTest, check_subtable_fetching_for_moving_to_a_smaller_table) {
  auto res = large->setAuxiliary(small);
  ASSERT_EQ(res.get(), nullptr);

  std::uint32_t indexLarge = 822;  // picked something at "random"
  std::uint32_t indexSmall = indexLarge >> 2;
  std::uint32_t hash = indexLarge << (32 - large->logSize());

  auto subtable = large->auxiliaryBuckets(indexLarge);
  ASSERT_NE(subtable.get(), nullptr);
  auto bucket = subtable->fetchBucket(hash);
  ASSERT_EQ(bucket, small->primaryBucket(indexSmall));
}

TEST_F(CacheTableMigrationTest, check_subtable_fetching_for_moving_to_a_larger_table) {
  auto res = small->setAuxiliary(large);
  ASSERT_EQ(res.get(), nullptr);

  std::uint32_t indexSmall = 217;  // picked something at "random"
  std::uint32_t indexLargeBase = indexSmall << 2;

  auto subtable = small->auxiliaryBuckets(indexSmall);
  ASSERT_NE(subtable.get(), nullptr);
  for (std::uint32_t i = 0; i < 4; i++) {
    std::uint32_t indexLarge = indexLargeBase + i;
    std::uint32_t hash = indexLarge << (32 - large->logSize());
    ASSERT_EQ(subtable->fetchBucket(hash), large->primaryBucket(indexLarge));
  }
}

TEST_F(CacheTableMigrationTest, check_subtable_apply_all_works) {
  auto res = small->setAuxiliary(large);
  ASSERT_EQ(res.get(), nullptr);

  std::uint32_t indexSmall = 172;  // picked something at "random"
  std::uint32_t indexLargeBase = indexSmall << 2;

  auto subtable = small->auxiliaryBuckets(indexSmall);
  ASSERT_NE(subtable.get(), nullptr);
  subtable->applyToAllBuckets<PlainBucket>(
      [](PlainBucket& bucket) -> bool { return bucket.lock(-1); });
  for (std::uint32_t i = 0; i < 4; i++) {
    std::uint32_t indexLarge = indexLargeBase + i;
    std::uint32_t hash = indexLarge << (32 - large->logSize());
    auto bucket = reinterpret_cast<PlainBucket*>(subtable->fetchBucket(hash));
    ASSERT_TRUE(bucket->isLocked());
  }
  subtable->applyToAllBuckets<PlainBucket>([](PlainBucket& bucket) -> bool {
    bucket.unlock();
    return true;
  });
}

TEST_F(CacheTableMigrationTest, test_fill_ratio_methods) {
  for (std::uint64_t i = 0; i < large->size(); i++) {
    bool res = large->slotFilled();
    if (static_cast<double>(i + 1) < 0.04 * static_cast<double>(large->size())) {
      ASSERT_EQ(large->idealSize(), large->logSize() - 1);
      ASSERT_FALSE(res);
    } else if (static_cast<double>(i + 1) > 0.25 * static_cast<double>(large->size())) {
      ASSERT_EQ(large->idealSize(), large->logSize() + 1);
      ASSERT_TRUE(res);
    } else {
      ASSERT_EQ(large->idealSize(), large->logSize());
      ASSERT_FALSE(res);
    }
  }
  for (std::uint64_t i = large->size(); i > 0; i--) {
    bool res = large->slotEmptied();
    if (static_cast<double>(i - 1) < 0.04 * static_cast<double>(large->size())) {
      ASSERT_EQ(large->idealSize(), large->logSize() - 1);
      ASSERT_TRUE(res);
    } else if (static_cast<double>(i - 1) > 0.25 * static_cast<double>(large->size())) {
      ASSERT_EQ(large->idealSize(), large->logSize() + 1);
      ASSERT_FALSE(res);
    } else {
      ASSERT_EQ(large->idealSize(), large->logSize());
      ASSERT_FALSE(res);
    }
  }
}
