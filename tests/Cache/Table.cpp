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
/// @author Daniel H. Larkin
/// @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"
#include "Cache/Common.h"
#include "Cache/PlainBucket.h"
#include "Cache/Table.h"

#include "gtest/gtest.h"

#include <stdint.h>
#include <memory>

using namespace arangodb::cache;

TEST(CacheTableTest, test_static_allocation_size_method) {
  for (uint32_t i = Table::minLogSize; i <= Table::maxLogSize; i++) {
    ASSERT_TRUE(Table::allocationSize(i) ==
                (sizeof(Table) + (BUCKET_SIZE << i) + Table::padding));
  }
}

TEST(CacheTableTest, test_basic_constructor_behavior) {
  for (uint32_t i = Table::minLogSize; i <= 20; i++) {
    auto table = std::make_shared<Table>(i);
    ASSERT_TRUE(table.get() != nullptr);
    ASSERT_TRUE(table->memoryUsage() == (sizeof(Table) + (BUCKET_SIZE << i) + Table::padding));
    ASSERT_TRUE(table->logSize() == i);
    ASSERT_TRUE(table->size() == (static_cast<uint64_t>(1) << i));
  }
}

TEST(CacheTableTest, test_basic_bucket_fetching_behavior) {
  auto table = std::make_shared<Table>(Table::minLogSize);
  ASSERT_TRUE(table.get() != nullptr);
  table->enable();
  for (uint64_t i = 0; i < table->size(); i++) {
    uint32_t hash = static_cast<uint32_t>(i << (32 - Table::minLogSize));
    auto pair = table->fetchAndLockBucket(hash, -1);
    auto bucket = reinterpret_cast<PlainBucket*>(pair.first);
    auto source = pair.second;
    ASSERT_TRUE(bucket != nullptr);
    ASSERT_TRUE(bucket->isLocked());
    ASSERT_TRUE(source != nullptr);
    ASSERT_TRUE(source == table.get());

    auto rawBucket = reinterpret_cast<PlainBucket*>(table->primaryBucket(i));
    ASSERT_TRUE(bucket == rawBucket);

    auto badPair = table->fetchAndLockBucket(hash, 10);
    auto badBucket = reinterpret_cast<PlainBucket*>(badPair.first);
    auto badSource = badPair.second;
    ASSERT_TRUE(badBucket == nullptr);
    ASSERT_TRUE(badSource == nullptr);

    bucket->unlock();
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
  ASSERT_TRUE(res.get() == nullptr);
  res = small->setAuxiliary(huge);
  ASSERT_TRUE(res.get() == huge.get());
  res = small->setAuxiliary(std::shared_ptr<Table>(nullptr));
  ASSERT_TRUE(res.get() == large.get());
}

TEST_F(CacheTableMigrationTest, check_that_bucket_locking_falls_through_appropriately) {
  auto res = small->setAuxiliary(large);
  ASSERT_TRUE(res.get() == nullptr);

  uint32_t indexSmall = 17;  // picked something at "random"
  uint32_t indexLarge = indexSmall << 2;
  uint32_t hash = indexSmall << (32 - small->logSize());

  auto pair = small->fetchAndLockBucket(hash, -1);
  auto bucket = reinterpret_cast<PlainBucket*>(pair.first);
  auto source = pair.second;
  ASSERT_TRUE(bucket == reinterpret_cast<PlainBucket*>(small->primaryBucket(indexSmall)));
  bucket->_state.toggleFlag(BucketState::Flag::migrated);
  bucket->unlock();
  ASSERT_TRUE(source == small.get());

  pair = small->fetchAndLockBucket(hash, -1);
  bucket = reinterpret_cast<PlainBucket*>(pair.first);
  source = pair.second;
  ASSERT_TRUE(bucket == reinterpret_cast<PlainBucket*>(large->primaryBucket(indexLarge)));
  ASSERT_TRUE(source == large.get());
  pair = small->fetchAndLockBucket(hash, 10);
  ASSERT_TRUE(pair.first == nullptr);
  ASSERT_TRUE(pair.second == nullptr);
  bucket->unlock();
}

TEST_F(CacheTableMigrationTest, check_subtable_fetching_for_moving_to_a_smaller_table) {
  auto res = large->setAuxiliary(small);
  ASSERT_TRUE(res.get() == nullptr);

  uint32_t indexLarge = 822;  // picked something at "random"
  uint32_t indexSmall = indexLarge >> 2;
  uint32_t hash = indexLarge << (32 - large->logSize());

  auto subtable = large->auxiliaryBuckets(indexLarge);
  ASSERT_TRUE(subtable.get() != nullptr);
  auto bucket = subtable->fetchBucket(hash);
  ASSERT_TRUE(bucket == small->primaryBucket(indexSmall));
}

TEST_F(CacheTableMigrationTest, check_subtable_fetching_for_moving_to_a_larger_table) {
  auto res = small->setAuxiliary(large);
  ASSERT_TRUE(res.get() == nullptr);

  uint32_t indexSmall = 217;  // picked something at "random"
  uint32_t indexLargeBase = indexSmall << 2;

  auto subtable = small->auxiliaryBuckets(indexSmall);
  ASSERT_TRUE(subtable.get() != nullptr);
  for (uint32_t i = 0; i < 4; i++) {
    uint32_t indexLarge = indexLargeBase + i;
    uint32_t hash = indexLarge << (32 - large->logSize());
    ASSERT_TRUE(subtable->fetchBucket(hash) == large->primaryBucket(indexLarge));
  }
}

TEST_F(CacheTableMigrationTest, check_subtable_apply_all_works) {
  auto res = small->setAuxiliary(large);
  ASSERT_TRUE(res.get() == nullptr);

  uint32_t indexSmall = 172;  // picked something at "random"
  uint32_t indexLargeBase = indexSmall << 2;

  auto subtable = small->auxiliaryBuckets(indexSmall);
  ASSERT_TRUE(subtable.get() != nullptr);
  subtable->applyToAllBuckets([](void* ptr) -> bool {
    PlainBucket* bucket = reinterpret_cast<PlainBucket*>(ptr);
    return bucket->lock(-1);
  });
  for (uint32_t i = 0; i < 4; i++) {
    uint32_t indexLarge = indexLargeBase + i;
    uint32_t hash = indexLarge << (32 - large->logSize());
    auto bucket = reinterpret_cast<PlainBucket*>(subtable->fetchBucket(hash));
    ASSERT_TRUE(bucket->isLocked());
  }
  subtable->applyToAllBuckets([](void* ptr) -> bool {
    PlainBucket* bucket = reinterpret_cast<PlainBucket*>(ptr);
    bucket->unlock();
    return true;
  });
}

TEST_F(CacheTableMigrationTest, test_fill_ratio_methods) {
  for (uint64_t i = 0; i < large->size(); i++) {
    bool res = large->slotFilled();
    if (static_cast<double>(i + 1) < 0.04 * static_cast<double>(large->size())) {
      ASSERT_TRUE(large->idealSize() == large->logSize() - 1);
      ASSERT_TRUE(res == false);
    } else if (static_cast<double>(i + 1) > 0.25 * static_cast<double>(large->size())) {
      ASSERT_TRUE(large->idealSize() == large->logSize() + 1);
      ASSERT_TRUE(res == true);
    } else {
      ASSERT_TRUE(large->idealSize() == large->logSize());
      ASSERT_TRUE(res == false);
    }
  }
  for (uint64_t i = large->size(); i > 0; i--) {
    bool res = large->slotEmptied();
    if (static_cast<double>(i - 1) < 0.04 * static_cast<double>(large->size())) {
      ASSERT_TRUE(large->idealSize() == large->logSize() - 1);
      ASSERT_TRUE(res == true);
    } else if (static_cast<double>(i - 1) > 0.25 * static_cast<double>(large->size())) {
      ASSERT_TRUE(large->idealSize() == large->logSize() + 1);
      ASSERT_TRUE(res == false);
    } else {
      ASSERT_TRUE(large->idealSize() == large->logSize());
      ASSERT_TRUE(res == false);
    }
  }
}
