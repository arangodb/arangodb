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

#include <cstdint>
#include <string>
#include <thread>
#include <vector>

#include "ApplicationFeatures/SharedPRNGFeature.h"
#include "Cache/Common.h"
#include "Cache/Manager.h"
#include "Cache/Transaction.h"
#include "Cache/TransactionalCache.h"
#include "Random/RandomGenerator.h"

#include "Mocks/Servers.h"
#include "MockScheduler.h"

using namespace arangodb;
using namespace arangodb::cache;
using namespace arangodb::tests::mocks;

// long-running

TEST(CacheTransactionalCacheTest, test_basic_cache_construction) {
  auto postFn = [](std::function<void()>) -> bool { return false; };
  MockMetricsServer server;
  SharedPRNGFeature& sharedPRNG = server.getFeature<SharedPRNGFeature>();
  Manager manager(sharedPRNG, postFn, 1024 * 1024);
  auto cache1 = manager.createCache(CacheType::Transactional, false, 256 * 1024);
  auto cache2 = manager.createCache(CacheType::Transactional, false, 512 * 1024);

  ASSERT_EQ(0, cache1->usage());
  ASSERT_TRUE(256 * 1024 >= cache1->size());
  ASSERT_EQ(0, cache2->usage());
  ASSERT_TRUE(512 * 1024 >= cache2->size());

  manager.destroyCache(cache1);
  manager.destroyCache(cache2);
}

TEST(CacheTransactionalCacheTest, verify_that_insertion_works_as_expected) {
  std::uint64_t cacheLimit = 128 * 1024;
  auto postFn = [](std::function<void()>) -> bool { return false; };
  MockMetricsServer server;
  SharedPRNGFeature& sharedPRNG = server.getFeature<SharedPRNGFeature>();
  Manager manager(sharedPRNG, postFn, 4 * cacheLimit);
  auto cache = manager.createCache(CacheType::Transactional, false, cacheLimit);

  for (std::uint64_t i = 0; i < 1024; i++) {
    CachedValue* value =
        CachedValue::construct(&i, sizeof(std::uint64_t), &i, sizeof(std::uint64_t));
    TRI_ASSERT(value != nullptr);
    auto status = cache->insert(value);
    if (status.ok()) {
      auto f = cache->find(&i, sizeof(std::uint64_t));
      ASSERT_TRUE(f.found());
    } else {
      delete value;
    }
  }

  for (std::uint64_t i = 0; i < 1024; i++) {
    std::uint64_t j = 2 * i;
    CachedValue* value =
        CachedValue::construct(&i, sizeof(std::uint64_t), &j, sizeof(std::uint64_t));
    TRI_ASSERT(value != nullptr);
    auto status = cache->insert(value);
    if (status.ok()) {
      auto f = cache->find(&i, sizeof(std::uint64_t));
      ASSERT_TRUE(f.found());
      ASSERT_EQ(0, memcmp(f.value()->value(), &j, sizeof(std::uint64_t)));
    } else {
      delete value;
    }
  }

  for (std::uint64_t i = 1024; i < 128 * 1024; i++) {
    CachedValue* value =
        CachedValue::construct(&i, sizeof(std::uint64_t), &i, sizeof(std::uint64_t));
    TRI_ASSERT(value != nullptr);
    auto status = cache->insert(value);
    if (status.ok()) {
      auto f = cache->find(&i, sizeof(std::uint64_t));
      ASSERT_TRUE(f.found());
    } else {
      delete value;
    }
  }
  ASSERT_TRUE(cache->size() <= 128 * 1024);

  manager.destroyCache(cache);
}

TEST(CacheTransactionalCacheTest, verify_removal_works_as_expected) {
  std::uint64_t cacheLimit = 128 * 1024;
  auto postFn = [](std::function<void()>) -> bool { return false; };
  MockMetricsServer server;
  SharedPRNGFeature& sharedPRNG = server.getFeature<SharedPRNGFeature>();
  Manager manager(sharedPRNG, postFn, 4 * cacheLimit);
  auto cache = manager.createCache(CacheType::Transactional, false, cacheLimit);

  for (std::uint64_t i = 0; i < 1024; i++) {
    CachedValue* value =
        CachedValue::construct(&i, sizeof(std::uint64_t), &i, sizeof(std::uint64_t));
    TRI_ASSERT(value != nullptr);
    auto status = cache->insert(value);
    if (status.ok()) {
      auto f = cache->find(&i, sizeof(std::uint64_t));
      ASSERT_TRUE(f.found());
      ASSERT_NE(f.value(), nullptr);
      ASSERT_TRUE(f.value()->sameKey(&i, sizeof(std::uint64_t)));
    } else {
      delete value;
    }
  }
  std::uint64_t inserted = 0;
  for (std::uint64_t j = 0; j < 1024; j++) {
    auto f = cache->find(&j, sizeof(std::uint64_t));
    if (f.found()) {
      inserted++;
      ASSERT_NE(f.value(), nullptr);
      ASSERT_TRUE(f.value()->sameKey(&j, sizeof(std::uint64_t)));
    }
  }

  // test removal of bogus keys
  for (std::uint64_t i = 1024; i < 1088; i++) {
    auto status = cache->remove(&i, sizeof(std::uint64_t));
    ASSERT_TRUE(status.ok());
    // ensure existing keys not removed
    std::uint64_t found = 0;
    for (std::uint64_t j = 0; j < 1024; j++) {
      auto f = cache->find(&j, sizeof(std::uint64_t));
      if (f.found()) {
        found++;
        ASSERT_NE(f.value(), nullptr);
        ASSERT_TRUE(f.value()->sameKey(&j, sizeof(std::uint64_t)));
      }
    }
    ASSERT_EQ(inserted, found);
  }

  // remove actual keys
  for (std::uint64_t i = 0; i < 1024; i++) {
    auto status = cache->remove(&i, sizeof(std::uint64_t));
    ASSERT_TRUE(status.ok());
    auto f = cache->find(&i, sizeof(std::uint64_t));
    ASSERT_FALSE(f.found());
  }

  manager.destroyCache(cache);
}

TEST(CacheTransactionalCacheTest, verify_banishing_works_as_expected) {
  std::uint64_t cacheLimit = 128 * 1024;
  auto postFn = [](std::function<void()>) -> bool { return false; };
  MockMetricsServer server;
  SharedPRNGFeature& sharedPRNG = server.getFeature<SharedPRNGFeature>();
  Manager manager(sharedPRNG, postFn, 4 * cacheLimit);
  auto cache = manager.createCache(CacheType::Transactional, false, cacheLimit);

  Transaction* tx = manager.beginTransaction(false);

  for (std::uint64_t i = 0; i < 1024; i++) {
    CachedValue* value =
        CachedValue::construct(&i, sizeof(std::uint64_t), &i, sizeof(std::uint64_t));
    TRI_ASSERT(value != nullptr);
    auto status = cache->insert(value);
    if (status.ok()) {
      auto f = cache->find(&i, sizeof(std::uint64_t));
      ASSERT_TRUE(f.found());
      ASSERT_NE(f.value(), nullptr);
      ASSERT_TRUE(f.value()->sameKey(&i, sizeof(std::uint64_t)));
    } else {
      delete value;
    }
  }

  for (std::uint64_t i = 512; i < 1024; i++) {
    auto status = cache->banish(&i, sizeof(std::uint64_t));
    ASSERT_TRUE(status.ok());
    auto f = cache->find(&i, sizeof(std::uint64_t));
    ASSERT_FALSE(f.found());
  }

  for (std::uint64_t i = 512; i < 1024; i++) {
    CachedValue* value =
        CachedValue::construct(&i, sizeof(std::uint64_t), &i, sizeof(std::uint64_t));
    TRI_ASSERT(value != nullptr);
    auto status = cache->insert(value);
    ASSERT_TRUE(status.fail());
    delete value;
    auto f = cache->find(&i, sizeof(std::uint64_t));
    ASSERT_FALSE(f.found());
  }

  manager.endTransaction(tx);
  tx = manager.beginTransaction(false);

  std::uint64_t reinserted = 0;
  for (std::uint64_t i = 512; i < 1024; i++) {
    CachedValue* value =
        CachedValue::construct(&i, sizeof(std::uint64_t), &i, sizeof(std::uint64_t));
    TRI_ASSERT(value != nullptr);
    auto status = cache->insert(value);
    if (status.ok()) {
      reinserted++;
      auto f = cache->find(&i, sizeof(std::uint64_t));
      ASSERT_TRUE(f.found());
    } else {
      delete value;
    }
  }
  ASSERT_TRUE(reinserted >= 256);

  manager.endTransaction(tx);
  manager.destroyCache(cache);
}

TEST(CacheTransactionalCacheTest, verify_cache_can_grow_correctly_when_it_runs_out_of_space_LongRunning) {
  MockScheduler scheduler(4);
  auto postFn = [&scheduler](std::function<void()> fn) -> bool {
    scheduler.post(fn);
    return true;
  };
  MockMetricsServer server;
  SharedPRNGFeature& sharedPRNG = server.getFeature<SharedPRNGFeature>();
  Manager manager(sharedPRNG, postFn, 1024 * 1024 * 1024);
  auto cache = manager.createCache(CacheType::Transactional);
  std::uint64_t minimumUsage = cache->usageLimit() * 2;

  for (std::uint64_t i = 0; i < 4 * 1024 * 1024; i++) {
    CachedValue* value =
        CachedValue::construct(&i, sizeof(std::uint64_t), &i, sizeof(std::uint64_t));
    TRI_ASSERT(value != nullptr);
    auto status = cache->insert(value);
    if (status.fail()) {
      delete value;
    }
  }

  EXPECT_GT(cache->usageLimit(), minimumUsage);
  EXPECT_GT(cache->usage(), minimumUsage);

  manager.destroyCache(cache);
}

TEST(CacheTransactionalCacheTest, test_behavior_under_mixed_load_LongRunning) {
  RandomGenerator::initialize(RandomGenerator::RandomType::MERSENNE);
  MockScheduler scheduler(4);
  auto postFn = [&scheduler](std::function<void()> fn) -> bool {
    scheduler.post(fn);
    return true;
  };
  MockMetricsServer server;
  SharedPRNGFeature& sharedPRNG = server.getFeature<SharedPRNGFeature>();
  Manager manager(sharedPRNG, postFn, 1024 * 1024 * 1024);
  std::size_t threadCount = 4;
  std::shared_ptr<Cache> cache = manager.createCache(CacheType::Transactional);

  std::uint64_t chunkSize = 16 * 1024 * 1024;
  std::uint64_t initialInserts = 4 * 1024 * 1024;
  std::uint64_t operationCount = 16 * 1024 * 1024;
  std::atomic<std::uint64_t> hitCount(0);
  std::atomic<std::uint64_t> missCount(0);
  auto worker = [&manager, &cache, initialInserts, operationCount, &hitCount,
                 &missCount](std::uint64_t lower, std::uint64_t upper) -> void {
    Transaction* tx = manager.beginTransaction(false);
    // fill with some initial data
    for (std::uint64_t i = 0; i < initialInserts; i++) {
      std::uint64_t item = lower + i;
      CachedValue* value = CachedValue::construct(&item, sizeof(std::uint64_t),
                                                  &item, sizeof(std::uint64_t));
      TRI_ASSERT(value != nullptr);
      auto status = cache->insert(value);
      if (status.fail()) {
        delete value;
      }
    }

    // initialize valid range for keys that *might* be in cache
    std::uint64_t validLower = lower;
    std::uint64_t validUpper = lower + initialInserts - 1;
    std::uint64_t banishUpper = validUpper;

    // commence mixed workload
    for (std::uint64_t i = 0; i < operationCount; i++) {
      std::uint32_t r = RandomGenerator::interval(static_cast<std::uint32_t>(99UL));

      if (r >= 99) {  // remove something
        if (validLower == validUpper) {
          continue;  // removed too much
        }

        std::uint64_t item = validLower++;

        cache->remove(&item, sizeof(std::uint64_t));
      } else if (r >= 90) {  // insert something
        if (validUpper == upper) {
          continue;  // already maxed out range
        }

        std::uint64_t item = ++validUpper;
        if (validUpper > banishUpper) {
          banishUpper = validUpper;
        }
        CachedValue* value = CachedValue::construct(&item, sizeof(std::uint64_t),
                                                    &item, sizeof(std::uint64_t));
        TRI_ASSERT(value != nullptr);
        auto status = cache->insert(value);
        if (status.fail()) {
          delete value;
        }
      } else if (r >= 80) {  // banish something
        if (banishUpper == upper) {
          continue;  // already maxed out range
        }

        std::uint64_t item = ++banishUpper;
        cache->banish(&item, sizeof(std::uint64_t));
      } else {  // lookup something
        std::uint64_t item =
            RandomGenerator::interval(static_cast<int64_t>(validLower),
                                      static_cast<int64_t>(validUpper));

        Finding f = cache->find(&item, sizeof(std::uint64_t));
        if (f.found()) {
          hitCount++;
          TRI_ASSERT(f.value() != nullptr);
          TRI_ASSERT(f.value()->sameKey(&item, sizeof(std::uint64_t)));
        } else {
          missCount++;
          TRI_ASSERT(f.value() == nullptr);
        }
      }
    }
    manager.endTransaction(tx);
  };

  std::vector<std::thread*> threads;
  // dispatch threads
  for (std::size_t i = 0; i < threadCount; i++) {
    std::uint64_t lower = i * chunkSize;
    std::uint64_t upper = ((i + 1) * chunkSize) - 1;
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
