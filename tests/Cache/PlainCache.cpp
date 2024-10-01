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
/// @author Dan Larkin-York
/// @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include <stdint.h>
#include <string>
#include <thread>
#include <vector>

#include "Basics/xoroshiro128plus.h"
#include "Cache/BinaryKeyHasher.h"
#include "Cache/CacheOptionsProvider.h"
#include "Cache/Common.h"
#include "Cache/Manager.h"
#include "Cache/PlainCache.h"
#include "Random/RandomGenerator.h"
#include "RestServer/SharedPRNGFeature.h"

#include "Mocks/Servers.h"
#include "MockScheduler.h"

using namespace arangodb;
using namespace arangodb::cache;
using namespace arangodb::tests::mocks;

struct CachePlainCacheTest : testing::Test {
  CachePlainCacheTest() : sharedPRNG(server.getFeature<SharedPRNGFeature>()) {}

  MockMetricsServer server;
  SharedPRNGFeature& sharedPRNG;
};

TEST_F(CachePlainCacheTest, test_basic_cache_creation) {
  auto postFn = [](std::function<void()>) -> bool { return false; };
  CacheOptions co;
  co.cacheSize = 1024 * 1024;
  Manager manager(sharedPRNG, postFn, co);
  auto cache1 =
      manager.createCache<BinaryKeyHasher>(CacheType::Plain, false, 256 * 1024);
  auto cache2 =
      manager.createCache<BinaryKeyHasher>(CacheType::Plain, false, 512 * 1024);

  ASSERT_EQ(0, cache1->usage());
  ASSERT_TRUE(256 * 1024 >= cache1->size());
  ASSERT_EQ(0, cache2->usage());
  ASSERT_TRUE(512 * 1024 >= cache2->size());

  manager.destroyCache(std::move(cache1));
  manager.destroyCache(std::move(cache2));
}

TEST_F(CachePlainCacheTest, check_that_insertion_works_as_expected) {
  std::uint64_t cacheLimit = 128 * 1024;
  auto postFn = [](std::function<void()>) -> bool { return false; };
  CacheOptions co;
  co.cacheSize = 4 * cacheLimit;
  Manager manager(sharedPRNG, postFn, co);
  auto cache =
      manager.createCache<BinaryKeyHasher>(CacheType::Plain, false, cacheLimit);

  for (std::uint64_t i = 0; i < 1024; i++) {
    CachedValue* value = CachedValue::construct(&i, sizeof(std::uint64_t), &i,
                                                sizeof(std::uint64_t));
    TRI_ASSERT(value != nullptr);
    auto status = cache->insert(value);
    if (status == TRI_ERROR_NO_ERROR) {
      auto f = cache->find(&i, sizeof(std::uint64_t));
      ASSERT_TRUE(f.found());
    } else {
      delete value;
    }
  }

  for (std::uint64_t i = 0; i < 1024; i++) {
    std::uint64_t j = 2 * i;
    CachedValue* value = CachedValue::construct(&i, sizeof(std::uint64_t), &j,
                                                sizeof(std::uint64_t));
    TRI_ASSERT(value != nullptr);
    auto status = cache->insert(value);
    if (status == TRI_ERROR_NO_ERROR) {
      auto f = cache->find(&i, sizeof(std::uint64_t));
      ASSERT_TRUE(f.found());
      ASSERT_EQ(0, memcmp(f.value()->value(), &j, sizeof(std::uint64_t)));
    } else {
      delete value;
    }
  }

  for (std::uint64_t i = 1024; i < 128 * 1024; i++) {
    CachedValue* value = CachedValue::construct(&i, sizeof(std::uint64_t), &i,
                                                sizeof(std::uint64_t));
    TRI_ASSERT(value != nullptr);
    auto status = cache->insert(value);
    if (status == TRI_ERROR_NO_ERROR) {
      auto f = cache->find(&i, sizeof(std::uint64_t));
      ASSERT_TRUE(f.found());
    } else {
      delete value;
    }
  }
  ASSERT_TRUE(cache->size() <= 128 * 1024);

  manager.destroyCache(std::move(cache));
}

TEST_F(CachePlainCacheTest, test_that_removal_works_as_expected) {
  std::uint64_t cacheLimit = 128 * 1024;
  auto postFn = [](std::function<void()>) -> bool { return false; };
  CacheOptions co;
  co.cacheSize = 4 * cacheLimit;
  Manager manager(sharedPRNG, postFn, co);
  auto cache =
      manager.createCache<BinaryKeyHasher>(CacheType::Plain, false, cacheLimit);

  for (std::uint64_t i = 0; i < 1024; i++) {
    CachedValue* value = CachedValue::construct(&i, sizeof(std::uint64_t), &i,
                                                sizeof(std::uint64_t));
    TRI_ASSERT(value != nullptr);
    auto status = cache->insert(value);
    if (status == TRI_ERROR_NO_ERROR) {
      auto f = cache->find(&i, sizeof(std::uint64_t));
      ASSERT_TRUE(f.found());
      ASSERT_NE(f.value(), nullptr);
      ASSERT_TRUE(BinaryKeyHasher::sameKey(
          f.value()->key(), f.value()->keySize(), &i, sizeof(std::uint64_t)));
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
      ASSERT_TRUE(BinaryKeyHasher::sameKey(
          f.value()->key(), f.value()->keySize(), &j, sizeof(std::uint64_t)));
    }
  }

  // test removal of bogus keys
  for (std::uint64_t i = 1024; i < 1088; i++) {
    auto status = cache->remove(&i, sizeof(std::uint64_t));
    ASSERT_EQ(TRI_ERROR_NO_ERROR, status);
    // ensure existing keys not removed
    std::uint64_t found = 0;
    for (std::uint64_t j = 0; j < 1024; j++) {
      auto f = cache->find(&j, sizeof(std::uint64_t));
      if (f.found()) {
        found++;
        ASSERT_NE(f.value(), nullptr);
        ASSERT_TRUE(BinaryKeyHasher::sameKey(
            f.value()->key(), f.value()->keySize(), &j, sizeof(std::uint64_t)));
      }
    }
    ASSERT_EQ(inserted, found);
  }

  // remove actual keys
  for (std::uint64_t i = 0; i < 1024; i++) {
    auto status = cache->remove(&i, sizeof(std::uint64_t));
    ASSERT_EQ(TRI_ERROR_NO_ERROR, status);
    auto f = cache->find(&i, sizeof(std::uint64_t));
    ASSERT_FALSE(f.found());
  }

  manager.destroyCache(std::move(cache));
}

TEST_F(
    CachePlainCacheTest,
    verify_that_cache_can_indeed_grow_when_it_runs_out_of_space_LongRunning) {
  MockScheduler scheduler(4);
  auto postFn = [&scheduler](std::function<void()> fn) -> bool {
    scheduler.post(fn);
    return true;
  };

  CacheOptions co;
  co.cacheSize = 1024 * 1024 * 1024;
  Manager manager(sharedPRNG, postFn, co);
  auto cache = manager.createCache<BinaryKeyHasher>(CacheType::Plain);
  std::uint64_t minimumUsage = cache->usageLimit() * 2;

  for (std::uint64_t i = 0; i < 4 * 1024 * 1024; i++) {
    CachedValue* value = CachedValue::construct(&i, sizeof(std::uint64_t), &i,
                                                sizeof(std::uint64_t));
    TRI_ASSERT(value != nullptr);
    auto status = cache->insert(value);
    if (status != TRI_ERROR_NO_ERROR) {
      delete value;
    }
  }

  EXPECT_GT(cache->usageLimit(), minimumUsage);
  EXPECT_GT(cache->usage(), minimumUsage);

  manager.destroyCache(std::move(cache));
}

TEST_F(CachePlainCacheTest, test_behavior_under_mixed_load_LongRunning) {
  RandomGenerator::initialize(RandomGenerator::RandomType::MERSENNE);
  MockScheduler scheduler(4);
  auto postFn = [&scheduler](std::function<void()> fn) -> bool {
    scheduler.post(fn);
    return true;
  };

  CacheOptions co;
  co.cacheSize = 1024 * 1024 * 1024;
  Manager manager(sharedPRNG, postFn, co);
  std::size_t threadCount = 4;
  std::shared_ptr<Cache> cache =
      manager.createCache<BinaryKeyHasher>(CacheType::Plain);

  std::uint64_t chunkSize = 16 * 1024 * 1024;
  std::uint64_t initialInserts = 4 * 1024 * 1024;
  std::uint64_t operationCount = 16 * 1024 * 1024;
  std::atomic<std::uint64_t> hitCount(0);
  std::atomic<std::uint64_t> missCount(0);
  auto worker = [&cache, initialInserts, operationCount, &hitCount, &missCount](
                    std::uint64_t lower, std::uint64_t upper) -> void {
    // fill with some initial data
    for (std::uint64_t i = 0; i < initialInserts; i++) {
      std::uint64_t item = lower + i;
      CachedValue* value = CachedValue::construct(&item, sizeof(std::uint64_t),
                                                  &item, sizeof(std::uint64_t));
      TRI_ASSERT(value != nullptr);
      auto status = cache->insert(value);
      if (status != TRI_ERROR_NO_ERROR) {
        delete value;
      }
    }

    // initialize valid range for keys that *might* be in cache
    std::uint64_t validLower = lower;
    std::uint64_t validUpper = lower + initialInserts - 1;

    basics::xoroshiro128plus prng;
    prng.seed(RandomGenerator::interval(UINT64_MAX),
              RandomGenerator::interval(UINT64_MAX));

    // commence mixed workload
    for (std::uint64_t i = 0; i < operationCount; i++) {
      std::uint64_t r = prng.next() % 99;

      if (r >= 99) {  // remove something
        if (validLower == validUpper) {
          continue;  // removed too much
        }

        std::uint64_t item = validLower++;

        cache->remove(&item, sizeof(std::uint64_t));
      } else if (r >= 95) {  // insert something
        if (validUpper == upper) {
          continue;  // already maxed out range
        }

        std::uint64_t item = ++validUpper;
        CachedValue* value = CachedValue::construct(
            &item, sizeof(std::uint64_t), &item, sizeof(std::uint64_t));
        TRI_ASSERT(value != nullptr);
        auto status = cache->insert(value);
        if (status != TRI_ERROR_NO_ERROR) {
          delete value;
        }
      } else {  // lookup something
        std::uint64_t item =
            (prng.next() % (validUpper + 1 - validLower)) + validLower;

        Finding f = cache->find(&item, sizeof(std::uint64_t));
        if (f.found()) {
          hitCount++;
          TRI_ASSERT(f.value() != nullptr);
          TRI_ASSERT(BinaryKeyHasher::sameKey(f.value()->key(),
                                              f.value()->keySize(), &item,
                                              sizeof(std::uint64_t)));
        } else {
          missCount++;
          TRI_ASSERT(f.value() == nullptr);
        }
      }
    }
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

  manager.destroyCache(std::move(cache));
  RandomGenerator::shutdown();
}

TEST_F(CachePlainCacheTest, test_hit_rate_statistics_reporting) {
  std::uint64_t cacheLimit = 256 * 1024;
  auto postFn = [](std::function<void()>) -> bool { return false; };

  CacheOptions co;
  co.cacheSize = 4 * cacheLimit;
  Manager manager(sharedPRNG, postFn, co);
  auto cacheMiss =
      manager.createCache<BinaryKeyHasher>(CacheType::Plain, true, cacheLimit);
  auto cacheHit =
      manager.createCache<BinaryKeyHasher>(CacheType::Plain, true, cacheLimit);
  auto cacheMixed =
      manager.createCache<BinaryKeyHasher>(CacheType::Plain, true, cacheLimit);

  for (std::uint64_t i = 0; i < 1024; i++) {
    CachedValue* value = CachedValue::construct(&i, sizeof(std::uint64_t), &i,
                                                sizeof(std::uint64_t));
    TRI_ASSERT(value != nullptr);
    auto status = cacheHit->insert(value);
    if (status != TRI_ERROR_NO_ERROR) {
      delete value;
    }

    value = CachedValue::construct(&i, sizeof(std::uint64_t), &i,
                                   sizeof(std::uint64_t));
    TRI_ASSERT(value != nullptr);
    status = cacheMiss->insert(value);
    if (status != TRI_ERROR_NO_ERROR) {
      delete value;
    }

    value = CachedValue::construct(&i, sizeof(std::uint64_t), &i,
                                   sizeof(std::uint64_t));
    TRI_ASSERT(value != nullptr);
    status = cacheMixed->insert(value);
    if (status != TRI_ERROR_NO_ERROR) {
      delete value;
    }
  }

  for (std::uint64_t i = 0; i < 1024; i++) {
    auto f = cacheHit->find(&i, sizeof(std::uint64_t));
  }
  {
    auto cacheStats = cacheHit->hitRates();
    auto managerStats = manager.globalHitRates();
    EXPECT_GE(cacheStats.first, 40.0);
    EXPECT_GE(cacheStats.second, 40.0);
    EXPECT_GE(managerStats.first, 40.0);
    EXPECT_GE(managerStats.second, 40.0);
  }

  for (std::uint64_t i = 1024; i < 2048; i++) {
    auto f = cacheMiss->find(&i, sizeof(std::uint64_t));
  }
  {
    auto cacheStats = cacheMiss->hitRates();
    auto managerStats = manager.globalHitRates();
    EXPECT_DOUBLE_EQ(cacheStats.first, 0.0);
    EXPECT_DOUBLE_EQ(cacheStats.second, 0.0);
    EXPECT_GT(managerStats.first, 10.0);
    EXPECT_LT(managerStats.first, 60.0);
    EXPECT_GT(managerStats.second, 10.0);
    EXPECT_LT(managerStats.second, 60.0);
  }

  for (std::uint64_t i = 0; i < 1024; i++) {
    auto f = cacheMixed->find(&i, sizeof(std::uint64_t));
  }
  for (std::uint64_t i = 1024; i < 2048; i++) {
    auto f = cacheMixed->find(&i, sizeof(std::uint64_t));
  }
  {
    auto cacheStats = cacheMixed->hitRates();
    auto managerStats = manager.globalHitRates();
    // the tracking of hits and misses in the cache is only
    // approximate. thus we cannot guarantee exact values here
    // and have to use ranges for checking.
    EXPECT_GT(cacheStats.first, 10.0);
    EXPECT_LT(cacheStats.first, 75.0);
    EXPECT_GT(cacheStats.second, 10.0);
    EXPECT_LT(cacheStats.second, 75.0);
    EXPECT_GT(managerStats.first, 10.0);
    EXPECT_LT(managerStats.first, 75.0);
    EXPECT_GT(managerStats.second, 10.0);
    EXPECT_LT(managerStats.second, 75.0);
  }

  manager.destroyCache(std::move(cacheHit));
  manager.destroyCache(std::move(cacheMiss));
  manager.destroyCache(std::move(cacheMixed));
}
