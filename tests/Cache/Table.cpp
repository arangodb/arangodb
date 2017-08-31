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

#include "Cache/Table.h"
#include "Basics/Common.h"
#include "Cache/Common.h"
#include "Cache/PlainBucket.h"

#include "catch.hpp"

#include <stdint.h>
#include <memory>

using namespace arangodb::cache;

TEST_CASE("cache::Table", "[cache]") {
  SECTION("test static allocation size method") {
    for (uint32_t i = Table::minLogSize; i <= Table::maxLogSize; i++) {
      REQUIRE(Table::allocationSize(i) ==
              (sizeof(Table) + (BUCKET_SIZE << i) + Table::padding));
    }
  }

  SECTION("test basic constructor behavior") {
    for (uint32_t i = Table::minLogSize; i <= 20; i++) {
      auto table = std::make_shared<Table>(i);
      REQUIRE(table.get() != nullptr);
      REQUIRE(table->memoryUsage() ==
              (sizeof(Table) + (BUCKET_SIZE << i) + Table::padding));
      REQUIRE(table->logSize() == i);
      REQUIRE(table->size() == (static_cast<uint64_t>(1) << i));
    }
  }

  SECTION("test basic bucket-fetching behavior") {
    auto table = std::make_shared<Table>(Table::minLogSize);
    REQUIRE(table.get() != nullptr);
    table->enable();
    for (uint64_t i = 0; i < table->size(); i++) {
      uint32_t hash = static_cast<uint32_t>(i << (32 - Table::minLogSize));
      auto pair = table->fetchAndLockBucket(hash, -1);
      auto bucket = reinterpret_cast<PlainBucket*>(pair.first);
      auto source = pair.second;
      REQUIRE(bucket != nullptr);
      REQUIRE(bucket->isLocked());
      REQUIRE(source != nullptr);
      REQUIRE(source == table.get());

      auto rawBucket = reinterpret_cast<PlainBucket*>(table->primaryBucket(i));
      REQUIRE(bucket == rawBucket);

      auto badPair = table->fetchAndLockBucket(hash, 10);
      auto badBucket = reinterpret_cast<PlainBucket*>(badPair.first);
      auto badSource = badPair.second;
      REQUIRE(badBucket == nullptr);
      REQUIRE(badSource == nullptr);

      bucket->unlock();
    }
  }

  SECTION("make sure migration functions work as intended") {
    auto small = std::make_shared<Table>(Table::minLogSize);
    auto large = std::make_shared<Table>(Table::minLogSize + 2);
    auto huge = std::make_shared<Table>(Table::minLogSize + 4);
    small->enable();
    large->enable();
    huge->enable();

    SECTION("check that setAuxiliary works as intended") {
      auto res = small->setAuxiliary(large);
      REQUIRE(res.get() == nullptr);
      res = small->setAuxiliary(huge);
      REQUIRE(res.get() == huge.get());
      res = small->setAuxiliary(std::shared_ptr<Table>(nullptr));
      REQUIRE(res.get() == large.get());
    }

    SECTION("check that bucket locking falls through appropriately") {
      auto res = small->setAuxiliary(large);
      REQUIRE(res.get() == nullptr);

      uint32_t indexSmall = 17;  // picked something at "random"
      uint32_t indexLarge = indexSmall << 2;
      uint32_t hash = indexSmall << (32 - small->logSize());

      auto pair = small->fetchAndLockBucket(hash, -1);
      auto bucket = reinterpret_cast<PlainBucket*>(pair.first);
      auto source = pair.second;
      REQUIRE(bucket ==
              reinterpret_cast<PlainBucket*>(small->primaryBucket(indexSmall)));
      bucket->_state.toggleFlag(BucketState::Flag::migrated);
      bucket->unlock();
      REQUIRE(source == small.get());

      pair = small->fetchAndLockBucket(hash, -1);
      bucket = reinterpret_cast<PlainBucket*>(pair.first);
      source = pair.second;
      REQUIRE(bucket ==
              reinterpret_cast<PlainBucket*>(large->primaryBucket(indexLarge)));
      REQUIRE(source == large.get());
      pair = small->fetchAndLockBucket(hash, 10);
      REQUIRE(pair.first == nullptr);
      REQUIRE(pair.second == nullptr);
      bucket->unlock();
    }

    SECTION("check subtable fetching for moving to a smaller table") {
      auto res = large->setAuxiliary(small);
      REQUIRE(res.get() == nullptr);

      uint32_t indexLarge = 822;  // picked something at "random"
      uint32_t indexSmall = indexLarge >> 2;
      uint32_t hash = indexLarge << (32 - large->logSize());

      auto subtable = large->auxiliaryBuckets(indexLarge);
      REQUIRE(subtable.get() != nullptr);
      auto bucket = subtable->fetchBucket(hash);
      REQUIRE(bucket == small->primaryBucket(indexSmall));
    }

    SECTION("check subtable fetching for moving to a larger table") {
      auto res = small->setAuxiliary(large);
      REQUIRE(res.get() == nullptr);

      uint32_t indexSmall = 217;  // picked something at "random"
      uint32_t indexLargeBase = indexSmall << 2;

      auto subtable = small->auxiliaryBuckets(indexSmall);
      REQUIRE(subtable.get() != nullptr);
      for (uint32_t i = 0; i < 4; i++) {
        uint32_t indexLarge = indexLargeBase + i;
        uint32_t hash = indexLarge << (32 - large->logSize());
        REQUIRE(subtable->fetchBucket(hash) ==
                large->primaryBucket(indexLarge));
      }
    }

    SECTION("check subtable apply all works") {
      auto res = small->setAuxiliary(large);
      REQUIRE(res.get() == nullptr);

      uint32_t indexSmall = 172;  // picked something at "random"
      uint32_t indexLargeBase = indexSmall << 2;

      auto subtable = small->auxiliaryBuckets(indexSmall);
      REQUIRE(subtable.get() != nullptr);
      subtable->applyToAllBuckets([](void* ptr) -> bool {
        PlainBucket* bucket = reinterpret_cast<PlainBucket*>(ptr);
        return bucket->lock(-1);
      });
      for (uint32_t i = 0; i < 4; i++) {
        uint32_t indexLarge = indexLargeBase + i;
        uint32_t hash = indexLarge << (32 - large->logSize());
        auto bucket =
            reinterpret_cast<PlainBucket*>(subtable->fetchBucket(hash));
        REQUIRE(bucket->isLocked());
      }
      subtable->applyToAllBuckets([](void* ptr) -> bool {
        PlainBucket* bucket = reinterpret_cast<PlainBucket*>(ptr);
        bucket->unlock();
        return true;
      });
    }

    SECTION("test fill ratio methods") {
      for (uint64_t i = 0; i < large->size(); i++) {
        bool res = large->slotFilled();
        if (static_cast<double>(i + 1) <
            0.04 * static_cast<double>(large->size())) {
          REQUIRE(large->idealSize() == large->logSize() - 1);
          REQUIRE(res == false);
        } else if (static_cast<double>(i + 1) >
                                 0.25 * static_cast<double>(large->size())) {
          REQUIRE(large->idealSize() == large->logSize() + 1);
          REQUIRE(res == true);
        } else {
          REQUIRE(large->idealSize() == large->logSize());
          REQUIRE(res == false);
        }
      }
      for (uint64_t i = large->size(); i > 0; i--) {
        bool res = large->slotEmptied();
        if (static_cast<double>(i - 1) <
            0.04 * static_cast<double>(large->size())) {
          REQUIRE(large->idealSize() == large->logSize() - 1);
          REQUIRE(res == true);
        } else if (static_cast<double>(i - 1) >
                                 0.25 * static_cast<double>(large->size())) {
          REQUIRE(large->idealSize() == large->logSize() + 1);
          REQUIRE(res == false);
        } else {
          REQUIRE(large->idealSize() == large->logSize());
          REQUIRE(res == false);
        }
      }
    }
  }
}
