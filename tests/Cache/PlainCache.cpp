////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for arangodb::cache::PlainCache
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

#include "Cache/PlainCache.h"
#include "Basics/Common.h"
#include "Cache/Common.h"
#include "Cache/Manager.h"
#include "Random/RandomGenerator.h"

#include "MockScheduler.h"
#include "catch.hpp"

#include <stdint.h>
#include <string>
#include <thread>
#include <vector>

using namespace arangodb;
using namespace arangodb::cache;

TEST_CASE("cache::PlainCache", "[cache][!hide][longRunning]") {
  SECTION("test basic cache creation") {
    auto postFn = [](std::function<void()>) -> bool { return false; };
    Manager manager(postFn, 1024 * 1024);
    auto cache1 = manager.createCache(CacheType::Plain, false, 256 * 1024);
    REQUIRE(true);
    auto cache2 = manager.createCache(CacheType::Plain, false, 512 * 1024);

    REQUIRE(0 == cache1->usage());
    REQUIRE(256 * 1024 >= cache1->size());
    REQUIRE(0 == cache2->usage());
    REQUIRE(512 * 1024 >= cache2->size());

    manager.destroyCache(cache1);
    manager.destroyCache(cache2);
  }

  SECTION("check that insertion works as expected") {
    uint64_t cacheLimit = 256 * 1024;
    auto postFn = [](std::function<void()>) -> bool { return false; };
    Manager manager(postFn, 4 * cacheLimit);
    auto cache = manager.createCache(CacheType::Plain, false, cacheLimit);

    for (uint64_t i = 0; i < 1024; i++) {
      CachedValue* value =
          CachedValue::construct(&i, sizeof(uint64_t), &i, sizeof(uint64_t));
      TRI_ASSERT(value != nullptr);
      auto status = cache->insert(value);
      if (status.ok()) {
        auto f = cache->find(&i, sizeof(uint64_t));
        REQUIRE(f.found());
      } else {
        delete value;
      }
    }

    for (uint64_t i = 0; i < 1024; i++) {
      uint64_t j = 2 * i;
      CachedValue* value =
          CachedValue::construct(&i, sizeof(uint64_t), &j, sizeof(uint64_t));
      TRI_ASSERT(value != nullptr);
      auto status = cache->insert(value);
      if (status.ok()) {
        auto f = cache->find(&i, sizeof(uint64_t));
        REQUIRE(f.found());
        REQUIRE(0 == memcmp(f.value()->value(), &j, sizeof(uint64_t)));
      } else {
        delete value;
      }
    }

    for (uint64_t i = 1024; i < 256 * 1024; i++) {
      CachedValue* value =
          CachedValue::construct(&i, sizeof(uint64_t), &i, sizeof(uint64_t));
      TRI_ASSERT(value != nullptr);
      auto status = cache->insert(value);
      if (status.ok()) {
        auto f = cache->find(&i, sizeof(uint64_t));
        REQUIRE(f.found());
      } else {
        delete value;
      }
    }
    REQUIRE(cache->size() <= 256 * 1024);

    manager.destroyCache(cache);
  }

  SECTION("test that removal works as expected") {
    uint64_t cacheLimit = 256 * 1024;
    auto postFn = [](std::function<void()>) -> bool { return false; };
    Manager manager(postFn, 4 * cacheLimit);
    auto cache = manager.createCache(CacheType::Plain, false, cacheLimit);

    for (uint64_t i = 0; i < 1024; i++) {
      CachedValue* value =
          CachedValue::construct(&i, sizeof(uint64_t), &i, sizeof(uint64_t));
      TRI_ASSERT(value != nullptr);
      auto status = cache->insert(value);
      if (status.ok()) {
        auto f = cache->find(&i, sizeof(uint64_t));
        REQUIRE(f.found());
        REQUIRE(f.value() != nullptr);
        REQUIRE(f.value()->sameKey(&i, sizeof(uint64_t)));
      } else {
        delete value;
      }
    }
    uint64_t inserted = 0;
    for (uint64_t j = 0; j < 1024; j++) {
      auto f = cache->find(&j, sizeof(uint64_t));
      if (f.found()) {
        inserted++;
        REQUIRE(f.value() != nullptr);
        REQUIRE(f.value()->sameKey(&j, sizeof(uint64_t)));
      }
    }

    // test removal of bogus keys
    for (uint64_t i = 1024; i < 2048; i++) {
      auto status = cache->remove(&i, sizeof(uint64_t));
      REQUIRE(status.ok());
      // ensure existing keys not removed
      uint64_t found = 0;
      for (uint64_t j = 0; j < 1024; j++) {
        auto f = cache->find(&j, sizeof(uint64_t));
        if (f.found()) {
          found++;
          REQUIRE(f.value() != nullptr);
          REQUIRE(f.value()->sameKey(&j, sizeof(uint64_t)));
        }
      }
      REQUIRE(inserted == found);
    }

    // remove actual keys
    for (uint64_t i = 0; i < 1024; i++) {
      auto status = cache->remove(&i, sizeof(uint64_t));
      REQUIRE(status.ok());
      auto f = cache->find(&i, sizeof(uint64_t));
      REQUIRE(!f.found());
    }

    manager.destroyCache(cache);
  }

  SECTION("verify that cache can indeed grow when it runs out of space") {
    uint64_t minimumUsage = 1024 * 1024;
    MockScheduler scheduler(4);
    auto postFn = [&scheduler](std::function<void()> fn) -> bool {
      scheduler.post(fn);
      return true;
    };
    Manager manager(postFn, 1024 * 1024 * 1024);
    auto cache = manager.createCache(CacheType::Plain);

    for (uint64_t i = 0; i < 4 * 1024 * 1024; i++) {
      CachedValue* value =
          CachedValue::construct(&i, sizeof(uint64_t), &i, sizeof(uint64_t));
      TRI_ASSERT(value != nullptr);
      auto status = cache->insert(value);
      if (status.fail()) {
        delete value;
      }
    }

    CHECK(cache->usage() > minimumUsage);

    manager.destroyCache(cache);
  }

  SECTION("test behavior under mixed load") {
    RandomGenerator::initialize(RandomGenerator::RandomType::MERSENNE);
    MockScheduler scheduler(4);
    auto postFn = [&scheduler](std::function<void()> fn) -> bool {
      scheduler.post(fn);
      return true;
    };
    Manager manager(postFn, 1024 * 1024 * 1024);
    size_t threadCount = 4;
    std::shared_ptr<Cache> cache = manager.createCache(CacheType::Plain);

    uint64_t chunkSize = 16 * 1024 * 1024;
    uint64_t initialInserts = 4 * 1024 * 1024;
    uint64_t operationCount = 16 * 1024 * 1024;
    std::atomic<uint64_t> hitCount(0);
    std::atomic<uint64_t> missCount(0);
    auto worker = [&manager, &cache, initialInserts, operationCount, &hitCount,
                   &missCount](uint64_t lower, uint64_t upper) -> void {
      // fill with some initial data
      for (uint64_t i = 0; i < initialInserts; i++) {
        uint64_t item = lower + i;
        CachedValue* value = CachedValue::construct(&item, sizeof(uint64_t),
                                                    &item, sizeof(uint64_t));
        TRI_ASSERT(value != nullptr);
        auto status = cache->insert(value);
        if (status.fail()) {
          delete value;
        }
      }

      // initialize valid range for keys that *might* be in cache
      uint64_t validLower = lower;
      uint64_t validUpper = lower + initialInserts - 1;

      // commence mixed workload
      for (uint64_t i = 0; i < operationCount; i++) {
        uint32_t r = RandomGenerator::interval(static_cast<uint32_t>(99UL));

        if (r >= 99) {  // remove something
          if (validLower == validUpper) {
            continue;  // removed too much
          }

          uint64_t item = validLower++;

          cache->remove(&item, sizeof(uint64_t));
        } else if (r >= 95) {  // insert something
          if (validUpper == upper) {
            continue;  // already maxed out range
          }

          uint64_t item = ++validUpper;
          CachedValue* value = CachedValue::construct(&item, sizeof(uint64_t),
                                                      &item, sizeof(uint64_t));
          TRI_ASSERT(value != nullptr);
          auto status = cache->insert(value);
          if (status.fail()) {
            delete value;
          }
        } else {  // lookup something
          uint64_t item =
              RandomGenerator::interval(static_cast<int64_t>(validLower),
                                        static_cast<int64_t>(validUpper));

          Finding f = cache->find(&item, sizeof(uint64_t));
          if (f.found()) {
            hitCount++;
            TRI_ASSERT(f.value() != nullptr);
            TRI_ASSERT(f.value()->sameKey(&item, sizeof(uint64_t)));
          } else {
            missCount++;
            TRI_ASSERT(f.value() == nullptr);
          }
        }
      }
    };

    std::vector<std::thread*> threads;
    // dispatch threads
    for (size_t i = 0; i < threadCount; i++) {
      uint64_t lower = i * chunkSize;
      uint64_t upper = ((i + 1) * chunkSize) - 1;
      threads.push_back(new std::thread(worker, lower, upper));
    }

    // join threads
    for (auto t : threads) {
      t->join();
      delete t;
    }

    manager.destroyCache(cache);
    RandomGenerator::shutdown();
  }

  SECTION("test hit rate statistics reporting") {
    uint64_t cacheLimit = 256 * 1024;
    auto postFn = [](std::function<void()>) -> bool { return false; };
    Manager manager(postFn, 4 * cacheLimit);
    auto cacheMiss = manager.createCache(CacheType::Plain, true, cacheLimit);
    auto cacheHit = manager.createCache(CacheType::Plain, true, cacheLimit);
    auto cacheMixed = manager.createCache(CacheType::Plain, true, cacheLimit);

    for (uint64_t i = 0; i < 1024; i++) {
      CachedValue* value =
          CachedValue::construct(&i, sizeof(uint64_t), &i, sizeof(uint64_t));
      TRI_ASSERT(value != nullptr);
      auto status = cacheHit->insert(value);
      if (status.fail()) {
        delete value;
      }

      value =
          CachedValue::construct(&i, sizeof(uint64_t), &i, sizeof(uint64_t));
      TRI_ASSERT(value != nullptr);
      status = cacheMiss->insert(value);
      if (status.fail()) {
        delete value;
      }

      value =
          CachedValue::construct(&i, sizeof(uint64_t), &i, sizeof(uint64_t));
      TRI_ASSERT(value != nullptr);
      status = cacheMixed->insert(value);
      if (status.fail()) {
        delete value;
      }
    }

    for (uint64_t i = 0; i < 1024; i++) {
      auto f = cacheHit->find(&i, sizeof(uint64_t));
    }
    {
      auto cacheStats = cacheHit->hitRates();
      auto managerStats = manager.globalHitRates();
      REQUIRE(cacheStats.first >= 40.0);
      REQUIRE(cacheStats.second >= 40.0);
      REQUIRE(managerStats.first >= 40.0);
      REQUIRE(managerStats.second >= 40.0);
    }

    for (uint64_t i = 1024; i < 2048; i++) {
      auto f = cacheMiss->find(&i, sizeof(uint64_t));
    }
    {
      auto cacheStats = cacheMiss->hitRates();
      auto managerStats = manager.globalHitRates();
      REQUIRE(cacheStats.first == 0.0);
      REQUIRE(cacheStats.second == 0.0);
      REQUIRE(managerStats.first > 10.0);
      REQUIRE(managerStats.first < 60.0);
      REQUIRE(managerStats.second > 10.0);
      REQUIRE(managerStats.second < 60.0);
    }

    for (uint64_t i = 0; i < 1024; i++) {
      auto f = cacheMixed->find(&i, sizeof(uint64_t));
    }
    for (uint64_t i = 1024; i < 2048; i++) {
      auto f = cacheMixed->find(&i, sizeof(uint64_t));
    }
    {
      auto cacheStats = cacheMixed->hitRates();
      auto managerStats = manager.globalHitRates();
      REQUIRE(cacheStats.first > 10.0);
      REQUIRE(cacheStats.first < 60.0);
      REQUIRE(cacheStats.second > 10.0);
      REQUIRE(cacheStats.second < 60.0);
      REQUIRE(managerStats.first > 10.0);
      REQUIRE(managerStats.first < 60.0);
      REQUIRE(managerStats.second > 10.0);
      REQUIRE(managerStats.second < 60.0);
    }

    manager.destroyCache(cacheHit);
    manager.destroyCache(cacheMiss);
    manager.destroyCache(cacheMixed);
  }
}
