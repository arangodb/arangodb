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
#include <queue>
#include <string>
#include <thread>
#include <vector>

#include "Basics/ScopeGuard.h"
#include "Basics/debugging.h"
#include "Cache/BinaryKeyHasher.h"
#include "Cache/CacheManagerFeatureThreads.h"
#include "Cache/CacheOptionsProvider.h"
#include "Cache/Common.h"
#include "Cache/Manager.h"
#include "Cache/PlainCache.h"
#include "Cache/TransactionalCache.h"
#include "Random/RandomGenerator.h"
#include "RestServer/SharedPRNGFeature.h"

#include "Mocks/Servers.h"
#include "MockScheduler.h"

using namespace arangodb;
using namespace arangodb::cache;
using namespace arangodb::tests::mocks;

TEST(CacheManagerTest, test_memory_usage_for_cache_creation) {
  std::uint64_t requestLimit = 1024 * 1024;

  MockMetricsServer server;
  SharedPRNGFeature& sharedPRNG = server.getFeature<SharedPRNGFeature>();
  auto postFn = [](std::function<void()>) -> bool { return false; };
  CacheOptions co;
  co.cacheSize = requestLimit;
  co.maxSpareAllocation = 0;
  Manager manager(sharedPRNG, postFn, co);

  ASSERT_EQ(requestLimit, manager.globalLimit());

  ASSERT_TRUE(0ULL < manager.globalAllocation());
  ASSERT_TRUE(requestLimit > manager.globalAllocation());

  {
    auto beforeStats = manager.memoryStats(cache::Cache::triesGuarantee);
    ASSERT_EQ(0, beforeStats->activeTables);

    auto cache = manager.createCache<BinaryKeyHasher>(CacheType::Transactional);
    ASSERT_NE(nullptr, cache);

    auto afterStats = manager.memoryStats(cache::Cache::triesGuarantee);
    ASSERT_EQ(1, afterStats->activeTables);

    manager.destroyCache(std::move(cache));

    auto afterStats2 = manager.memoryStats(cache::Cache::triesGuarantee);
    ASSERT_EQ(0, afterStats2->activeTables);
    ASSERT_EQ(beforeStats->globalAllocation, afterStats2->globalAllocation);
  }
}

TEST(CacheManagerTest, test_memory_usage_for_cache_reusage) {
  std::uint64_t requestLimit = 1024 * 1024 * 256;

  MockMetricsServer server;
  SharedPRNGFeature& sharedPRNG = server.getFeature<SharedPRNGFeature>();
  auto postFn = [](std::function<void()>) -> bool { return false; };
  CacheOptions co;
  co.cacheSize = requestLimit;
  co.maxSpareAllocation = 256 * 1024 * 1024;
  Manager manager(sharedPRNG, postFn, co);

  ASSERT_EQ(requestLimit, manager.globalLimit());

  ASSERT_TRUE(0ULL < manager.globalAllocation());
  ASSERT_TRUE(requestLimit > manager.globalAllocation());

  {
    auto beforeStats = manager.memoryStats(cache::Cache::triesGuarantee);
    ASSERT_EQ(0, beforeStats->activeTables);

    auto cache = manager.createCache<BinaryKeyHasher>(CacheType::Transactional);
    ASSERT_NE(nullptr, cache);

    manager.destroyCache(std::move(cache));

    auto afterStats = manager.memoryStats(cache::Cache::triesGuarantee);
    ASSERT_EQ(0, afterStats->activeTables);
    ASSERT_EQ(1, afterStats->spareTables);
    ASSERT_LT(beforeStats->globalAllocation, afterStats->globalAllocation);

    cache = manager.createCache<BinaryKeyHasher>(CacheType::Transactional);

    auto afterStats2 = manager.memoryStats(cache::Cache::triesGuarantee);
    ASSERT_EQ(1, afterStats2->activeTables);
    ASSERT_EQ(0, afterStats2->spareTables);
    ASSERT_EQ(afterStats->globalAllocation,
              afterStats2->globalAllocation - Manager::kCacheRecordOverhead);

    manager.destroyCache(std::move(cache));

    auto afterStats3 = manager.memoryStats(cache::Cache::triesGuarantee);
    ASSERT_EQ(afterStats->globalAllocation, afterStats3->globalAllocation);

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
    manager.freeUnusedTablesForTesting();

    auto afterStats4 = manager.memoryStats(cache::Cache::triesGuarantee);
    ASSERT_EQ(0, afterStats4->activeTables);
    ASSERT_EQ(0, afterStats4->spareTables);
    ASSERT_EQ(beforeStats->globalAllocation, afterStats4->globalAllocation);
#endif
  }
}

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
TEST(CacheManagerTest,
     test_memory_usage_with_failure_during_allocation_with_reserve) {
  std::uint64_t requestLimit = 1024 * 1024;

  MockMetricsServer server;
  SharedPRNGFeature& sharedPRNG = server.getFeature<SharedPRNGFeature>();
  auto postFn = [](std::function<void()>) -> bool { return false; };
  CacheOptions co;
  co.cacheSize = requestLimit;
  co.maxSpareAllocation = 256 * 1024 * 1024;
  Manager manager(sharedPRNG, postFn, co);

  ASSERT_EQ(requestLimit, manager.globalLimit());

  ASSERT_TRUE(0ULL < manager.globalAllocation());
  ASSERT_TRUE(requestLimit > manager.globalAllocation());

  auto guard = scopeGuard([]() noexcept { TRI_ClearFailurePointsDebugging(); });

  {
    TRI_ClearFailurePointsDebugging();
    auto beforeStats = manager.memoryStats(cache::Cache::triesGuarantee);

    TRI_AddFailurePointDebugging("CacheAllocation::fail1");
    auto cache = manager.createCache<BinaryKeyHasher>(CacheType::Transactional);
    ASSERT_EQ(nullptr, cache);

    TRI_ClearFailurePointsDebugging();
    cache = manager.createCache<BinaryKeyHasher>(CacheType::Transactional);
    ASSERT_NE(nullptr, cache);

    manager.destroyCache(std::move(cache));

    manager.freeUnusedTablesForTesting();

    auto afterStats = manager.memoryStats(cache::Cache::triesGuarantee);
    ASSERT_EQ(beforeStats->globalAllocation, afterStats->globalAllocation);
  }

  {
    TRI_ClearFailurePointsDebugging();
    auto beforeStats = manager.memoryStats(cache::Cache::triesGuarantee);

    TRI_AddFailurePointDebugging("CacheAllocation::fail2");
    ASSERT_ANY_THROW(
        manager.createCache<BinaryKeyHasher>(CacheType::Transactional));

    manager.freeUnusedTablesForTesting();

    auto afterStats = manager.memoryStats(cache::Cache::triesGuarantee);
    ASSERT_EQ(beforeStats->globalAllocation, afterStats->globalAllocation);
  }

  {
    TRI_ClearFailurePointsDebugging();
    auto beforeStats = manager.memoryStats(cache::Cache::triesGuarantee);

    TRI_AddFailurePointDebugging("CacheAllocation::fail3");
    ASSERT_ANY_THROW(
        manager.createCache<BinaryKeyHasher>(CacheType::Transactional));

    manager.freeUnusedTablesForTesting();

    auto afterStats = manager.memoryStats(cache::Cache::triesGuarantee);
    ASSERT_EQ(beforeStats->globalAllocation, afterStats->globalAllocation);
  }
}
#endif

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
TEST(CacheManagerTest,
     test_memory_usage_with_failure_during_allocation_no_reserve) {
  std::uint64_t requestLimit = 1024 * 1024;

  MockMetricsServer server;
  SharedPRNGFeature& sharedPRNG = server.getFeature<SharedPRNGFeature>();
  auto postFn = [](std::function<void()>) -> bool { return false; };
  CacheOptions co;
  co.cacheSize = requestLimit;
  co.maxSpareAllocation = 0;
  Manager manager(sharedPRNG, postFn, co);

  ASSERT_EQ(requestLimit, manager.globalLimit());

  ASSERT_TRUE(0ULL < manager.globalAllocation());
  ASSERT_TRUE(requestLimit > manager.globalAllocation());

  auto guard = scopeGuard([]() noexcept { TRI_ClearFailurePointsDebugging(); });

  {
    TRI_ClearFailurePointsDebugging();
    auto beforeStats = manager.memoryStats(cache::Cache::triesGuarantee);

    TRI_AddFailurePointDebugging("CacheAllocation::fail1");
    auto cache = manager.createCache<BinaryKeyHasher>(CacheType::Transactional);
    ASSERT_EQ(nullptr, cache);

    auto afterStats = manager.memoryStats(cache::Cache::triesGuarantee);
    ASSERT_EQ(beforeStats->globalAllocation, afterStats->globalAllocation);
    ASSERT_EQ(beforeStats->activeTables, afterStats->activeTables);
  }

  {
    TRI_ClearFailurePointsDebugging();
    auto beforeStats = manager.memoryStats(cache::Cache::triesGuarantee);

    TRI_AddFailurePointDebugging("CacheAllocation::fail2");
    ASSERT_ANY_THROW(
        manager.createCache<BinaryKeyHasher>(CacheType::Transactional));

    auto afterStats = manager.memoryStats(cache::Cache::triesGuarantee);
    ASSERT_EQ(beforeStats->globalAllocation, afterStats->globalAllocation);
    ASSERT_EQ(beforeStats->activeTables, afterStats->activeTables);
  }

  {
    TRI_ClearFailurePointsDebugging();
    auto beforeStats = manager.memoryStats(cache::Cache::triesGuarantee);

    TRI_AddFailurePointDebugging("CacheAllocation::fail3");
    ASSERT_ANY_THROW(
        manager.createCache<BinaryKeyHasher>(CacheType::Transactional));

    auto afterStats = manager.memoryStats(cache::Cache::triesGuarantee);
    ASSERT_EQ(beforeStats->globalAllocation, afterStats->globalAllocation);
    ASSERT_EQ(beforeStats->activeTables, afterStats->activeTables);
  }
}
#endif

TEST(CacheManagerTest, test_create_and_destroy_caches) {
  std::uint64_t requestLimit = 1024 * 1024;

  MockMetricsServer server;
  SharedPRNGFeature& sharedPRNG = server.getFeature<SharedPRNGFeature>();
  auto postFn = [](std::function<void()>) -> bool { return false; };
  CacheOptions co;
  co.cacheSize = requestLimit;
  Manager manager(sharedPRNG, postFn, co);

  ASSERT_EQ(requestLimit, manager.globalLimit());

  ASSERT_TRUE(0ULL < manager.globalAllocation());
  ASSERT_TRUE(requestLimit > manager.globalAllocation());

  std::vector<std::shared_ptr<Cache>> caches;

  for (size_t i = 0; i < 8; ++i) {
    auto beforeStats = manager.memoryStats(cache::Cache::triesGuarantee);
    ASSERT_EQ(i, beforeStats->activeTables);

    auto cache = manager.createCache<BinaryKeyHasher>(CacheType::Transactional);
    ASSERT_NE(nullptr, cache);
    ASSERT_GT(cache->size(),
              40 * 1024);  // size of each cache is about 40kb without stats

    auto afterStats = manager.memoryStats(cache::Cache::triesGuarantee);
    ASSERT_LT(beforeStats->globalAllocation, afterStats->globalAllocation);
    ASSERT_EQ(i + 1, afterStats->activeTables);

    ASSERT_EQ(0, afterStats->spareAllocation);
    ASSERT_EQ(0, afterStats->spareTables);

    caches.emplace_back(std::move(cache));
  }

  std::uint64_t spareTables = 0;
  while (!caches.empty()) {
    auto beforeStats = manager.memoryStats(cache::Cache::triesGuarantee);
    ASSERT_EQ(spareTables, beforeStats->spareTables);

    auto cache = caches.back();
    std::uint64_t size = cache->size();
    ASSERT_GT(size, 40 * 1024);  // size of each cache is about 40kb
    manager.destroyCache(std::move(cache));

    auto afterStats = manager.memoryStats(cache::Cache::triesGuarantee);
    if (afterStats->spareTables == beforeStats->spareTables) {
      // table deleted
      ASSERT_GT(beforeStats->globalAllocation, afterStats->globalAllocation);
      ASSERT_EQ(spareTables, afterStats->spareTables);
    } else {
      // table recycled
      ++spareTables;
      ASSERT_GT(afterStats->spareAllocation, spareTables * 16384);
      ASSERT_EQ(spareTables, afterStats->spareTables);
    }
    ASSERT_EQ(caches.size() - 1, afterStats->activeTables);

    caches.pop_back();
  }
}

TEST(CacheManagerTest, test_basic_constructor_function) {
  std::uint64_t requestLimit = 1024 * 1024;

  MockMetricsServer server;
  SharedPRNGFeature& sharedPRNG = server.getFeature<SharedPRNGFeature>();
  auto postFn = [](std::function<void()>) -> bool { return false; };
  CacheOptions co;
  co.cacheSize = requestLimit;
  Manager manager(sharedPRNG, postFn, co);

  ASSERT_EQ(requestLimit, manager.globalLimit());

  ASSERT_TRUE(0ULL < manager.globalAllocation());
  ASSERT_TRUE(requestLimit > manager.globalAllocation());

  std::uint64_t bigRequestLimit = 4ULL * 1024ULL * 1024ULL * 1024ULL;
  CacheOptions co2;
  co2.cacheSize = bigRequestLimit;
  Manager bigManager(sharedPRNG, nullptr, co2);

  ASSERT_EQ(bigRequestLimit, bigManager.globalLimit());

  ASSERT_TRUE(1024ULL * 1024ULL < bigManager.globalAllocation());
  ASSERT_TRUE(bigRequestLimit > bigManager.globalAllocation());
}

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
TEST(CacheManagerTest, test_memory_usage_for_data) {
  std::uint64_t requestLimit = 128 * 1024 * 1024;

  MockMetricsServer server;
  SharedPRNGFeature& sharedPRNG = server.getFeature<SharedPRNGFeature>();
  auto postFn = [](std::function<void()>) -> bool { return false; };
  CacheOptions co;
  co.cacheSize = requestLimit;
  co.maxSpareAllocation = 0;
  Manager manager(sharedPRNG, postFn, co);

  ASSERT_EQ(requestLimit, manager.globalLimit());

  ASSERT_TRUE(0ULL < manager.globalAllocation());
  ASSERT_TRUE(requestLimit > manager.globalAllocation());

  constexpr std::size_t n = 10'000;

  auto beforeStats = manager.memoryStats(cache::Cache::triesGuarantee);

  // create an initially large table
  TRI_AddFailurePointDebugging("Cache::createTable.large");

  auto guard = scopeGuard([]() noexcept { TRI_ClearFailurePointsDebugging(); });

  auto cache = manager.createCache<BinaryKeyHasher>(CacheType::Transactional);

  // clear failure point
  guard.fire();

  auto afterStats = manager.memoryStats(cache::Cache::triesGuarantee);
  ASSERT_LT(beforeStats->globalAllocation, afterStats->globalAllocation);

  std::string key;
  std::string value;
  std::size_t totalSize = 0;
  for (std::size_t i = 0; i < n; ++i) {
    key.clear();
    key.append("testkey").append(std::to_string(i));
    totalSize += key.size();

    value.clear();
    value.append("testvalue").append(std::to_string(i));
    totalSize += value.size();

    CachedValue* cv = CachedValue::construct(key.data(), key.size(),
                                             value.data(), value.size());
    TRI_ASSERT(cv != nullptr);
    do {
      auto status = cache->insert(cv);
      if (status == TRI_ERROR_NO_ERROR) {
        break;
      }
    } while (true);

    // add overhead:
    // - uint32 for padding
    // - atomic uint32 for ref count
    // - uint32 for key size
    // - uint32 for value size
    totalSize += 4 + 4 + 4 + 4;
  }

  auto afterStats2 = manager.memoryStats(cache::Cache::triesGuarantee);
  ASSERT_LT(beforeStats->globalAllocation + totalSize,
            afterStats2->globalAllocation);

  manager.destroyCache(std::move(cache));

  manager.freeUnusedTablesForTesting();

  auto afterStats3 = manager.memoryStats(cache::Cache::triesGuarantee);
  ASSERT_EQ(0, afterStats3->activeTables);
  ASSERT_EQ(0, afterStats3->spareTables);
  ASSERT_EQ(beforeStats->globalAllocation, afterStats3->globalAllocation);
}
#endif

TEST(CacheManagerTest, test_mixed_cache_types_under_mixed_load_LongRunning) {
  RandomGenerator::initialize(RandomGenerator::RandomType::MERSENNE);
  MockScheduler scheduler(4);
  auto postFn = [&scheduler](std::function<void()> fn) -> bool {
    scheduler.post(fn);
    return true;
  };

  MockMetricsServer server;
  SharedPRNGFeature& sharedPRNG = server.getFeature<SharedPRNGFeature>();
  CacheOptions co;
  co.cacheSize = 1024ULL * 1024ULL * 1024ULL;
  Manager manager(sharedPRNG, postFn, co);
  std::size_t cacheCount = 4;
  std::size_t threadCount = 4;
  std::vector<std::shared_ptr<Cache>> caches;
  for (std::size_t i = 0; i < cacheCount; i++) {
    auto res = manager.createCache<BinaryKeyHasher>(
        (i % 2 == 0) ? CacheType::Plain : CacheType::Transactional);
    TRI_ASSERT(res);
    caches.emplace_back(res);
  }

  std::uint64_t chunkSize = 4 * 1024 * 1024;
  std::uint64_t initialInserts = 1 * 1024 * 1024;
  std::uint64_t operationCount = 4 * 1024 * 1024;
  std::atomic<std::uint64_t> hitCount(0);
  std::atomic<std::uint64_t> missCount(0);
  auto worker = [&caches, cacheCount, initialInserts, operationCount, &hitCount,
                 &missCount](std::uint64_t lower, std::uint64_t upper) -> void {
    // fill with some initial data
    for (std::uint64_t i = 0; i < initialInserts; i++) {
      std::uint64_t item = lower + i;
      std::size_t cacheIndex = item % cacheCount;
      CachedValue* value = CachedValue::construct(&item, sizeof(std::uint64_t),
                                                  &item, sizeof(std::uint64_t));
      TRI_ASSERT(value != nullptr);
      auto status = caches[cacheIndex]->insert(value);
      if (status != TRI_ERROR_NO_ERROR) {
        delete value;
      }
    }

    // initialize valid range for keys that *might* be in cache
    std::uint64_t validLower = lower;
    std::uint64_t validUpper = lower + initialInserts - 1;

    // commence mixed workload
    for (std::uint64_t i = 0; i < operationCount; i++) {
      std::uint32_t r =
          RandomGenerator::interval(static_cast<std::uint32_t>(99));

      if (r >= 99) {  // remove something
        if (validLower == validUpper) {
          continue;  // removed too much
        }

        std::uint64_t item = validLower++;
        std::size_t cacheIndex = item % cacheCount;

        caches[cacheIndex]->remove(&item, sizeof(std::uint64_t));
      } else if (r >= 95) {  // insert something
        if (validUpper == upper) {
          continue;  // already maxed out range
        }

        std::uint64_t item = ++validUpper;
        std::size_t cacheIndex = item % cacheCount;
        CachedValue* value = CachedValue::construct(
            &item, sizeof(std::uint64_t), &item, sizeof(std::uint64_t));
        TRI_ASSERT(value != nullptr);
        auto status = caches[cacheIndex]->insert(value);
        if (status != TRI_ERROR_NO_ERROR) {
          delete value;
        }
      } else {  // lookup something
        std::uint64_t item = RandomGenerator::interval(
            static_cast<int64_t>(validLower), static_cast<int64_t>(validUpper));
        std::size_t cacheIndex = item % cacheCount;

        auto f = caches[cacheIndex]->find(&item, sizeof(std::uint64_t));
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

  for (auto cache : caches) {
    manager.destroyCache(std::move(cache));
  }
  caches.clear();

  RandomGenerator::shutdown();
}

TEST(CacheManagerTest, test_manager_under_cache_lifecycle_chaos_LongRunning) {
  RandomGenerator::initialize(RandomGenerator::RandomType::MERSENNE);
  MockScheduler scheduler(4);
  auto postFn = [&scheduler](std::function<void()> fn) -> bool {
    scheduler.post(fn);
    return true;
  };

  MockMetricsServer server;
  SharedPRNGFeature& sharedPRNG = server.getFeature<SharedPRNGFeature>();
  CacheOptions co;
  co.cacheSize = 1024ULL * 1024ULL * 1024ULL;
  Manager manager(sharedPRNG, postFn, co);
  std::size_t threadCount = 4;
  std::uint64_t operationCount = 4ULL * 1024ULL;

  auto worker = [&manager, operationCount]() -> void {
    std::queue<std::shared_ptr<Cache>> caches;

    for (std::uint64_t i = 0; i < operationCount; i++) {
      std::uint32_t r =
          RandomGenerator::interval(static_cast<std::uint32_t>(1));
      switch (r) {
        case 0: {
          auto res = manager.createCache<BinaryKeyHasher>(
              (i % 2 == 0) ? CacheType::Plain : CacheType::Transactional);
          if (res) {
            caches.emplace(res);
          }
        }
        // intentionally falls through
        case 1:
        default: {
          if (!caches.empty()) {
            auto cache = caches.front();
            caches.pop();
            manager.destroyCache(std::move(cache));
          }
        }
      }
    }
  };

  std::vector<std::thread*> threads;
  // dispatch threads
  for (std::size_t i = 0; i < threadCount; i++) {
    threads.push_back(new std::thread(worker));
  }

  // join threads
  for (auto t : threads) {
    t->join();
    delete t;
  }

  RandomGenerator::shutdown();
}
