////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for arangodb::cache::TransactionalCache
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2017 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Daniel H. Larkin
/// @author Copyright 2017, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"
#include "Random/RandomGenerator.h"

#include "catch.hpp"

#include "Cache/Manager.h"
#include "Cache/Transaction.h"
#include "Cache/TransactionalCache.h"

#include "MockScheduler.h"

#include <stdint.h>
#include <string>
#include <thread>
#include <vector>

#include <iostream>

using namespace arangodb;
using namespace arangodb::cache;

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("CCacheTransactionalCacheTest",
                         "[cache][!hide][longRunning]") {

////////////////////////////////////////////////////////////////////////////////
/// @brief test construction (single-threaded)
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_st_construction") {
  Manager manager(nullptr, 1024ULL * 1024ULL);
  auto cache1 = manager.createCache(Manager::CacheType::Transactional,
                                    256ULL * 1024ULL, false);
  auto cache2 = manager.createCache(Manager::CacheType::Transactional,
                                    512ULL * 1024ULL, false);

  CHECK(0ULL == cache1->usage());
  CHECK(256ULL * 1024ULL == cache1->limit());
  CHECK(0ULL == cache2->usage());
  CHECK(512ULL * 1024ULL > cache2->limit());

  manager.destroyCache(cache1);
  manager.destroyCache(cache2);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test insertion (single-threaded)
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_st_insertion") {
  uint64_t cacheLimit = 256ULL * 1024ULL;
  Manager manager(nullptr, 4ULL * cacheLimit);
  auto cache =
      manager.createCache(Manager::CacheType::Transactional, cacheLimit, false);

  for (uint64_t i = 0; i < 1024; i++) {
    CachedValue* value =
        CachedValue::construct(&i, sizeof(uint64_t), &i, sizeof(uint64_t));
    bool success = cache->insert(value);
    CHECK(success);
    auto f = cache->find(&i, sizeof(uint64_t));
    CHECK(f.found());
  }

  for (uint64_t i = 0; i < 1024; i++) {
    uint64_t j = 2 * i;
    CachedValue* value =
        CachedValue::construct(&i, sizeof(uint64_t), &j, sizeof(uint64_t));
    bool success = cache->insert(value);
    CHECK(success);
    auto f = cache->find(&i, sizeof(uint64_t));
    CHECK(f.found());
    CHECK(0 == memcmp(f.value()->value(), &j, sizeof(uint64_t)));
  }

  uint64_t notInserted = 0;
  for (uint64_t i = 1024; i < 128 * 1024; i++) {
    CachedValue* value =
        CachedValue::construct(&i, sizeof(uint64_t), &i, sizeof(uint64_t));
    bool success = cache->insert(value);
    if (success) {
      auto f = cache->find(&i, sizeof(uint64_t));
      CHECK(f.found());
    } else {
      delete value;
      notInserted++;
    }
  }
  CHECK(notInserted > 0);

  manager.destroyCache(cache);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test removal (single-threaded)
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_st_removal") {
  uint64_t cacheLimit = 256ULL * 1024ULL;
  Manager manager(nullptr, 4ULL * cacheLimit);
  auto cache =
      manager.createCache(Manager::CacheType::Transactional, cacheLimit, false);

  for (uint64_t i = 0; i < 1024; i++) {
    CachedValue* value =
        CachedValue::construct(&i, sizeof(uint64_t), &i, sizeof(uint64_t));
    bool success = cache->insert(value);
    CHECK(success);
    auto f = cache->find(&i, sizeof(uint64_t));
    CHECK(f.found());
    CHECK(f.value() != nullptr);
    CHECK(f.value()->sameKey(&i, sizeof(uint64_t)));
  }

  // test removal of bogus keys
  for (uint64_t i = 1024; i < 2048; i++) {
    bool removed = cache->remove(&i, sizeof(uint64_t));
    REQUIRE(removed);
    // ensure existing keys not removed
    for (uint64_t j = 0; j < 1024; j++) {
      auto f = cache->find(&j, sizeof(uint64_t));
      CHECK(f.found());
      CHECK(f.value() != nullptr);
      CHECK(f.value()->sameKey(&j, sizeof(uint64_t)));
    }
  }

  // remove actual keys
  for (uint64_t i = 0; i < 1024; i++) {
    bool removed = cache->remove(&i, sizeof(uint64_t));
    CHECK(removed);
    auto f = cache->find(&i, sizeof(uint64_t));
    CHECK(!f.found());
  }

  manager.destroyCache(cache);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test blacklisting (single-threaded)
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_st_blacklist") {
  uint64_t cacheLimit = 256ULL * 1024ULL;
  Manager manager(nullptr, 4ULL * cacheLimit);
  auto cache =
      manager.createCache(Manager::CacheType::Transactional, cacheLimit, false);

  Transaction* tx = manager.beginTransaction(false);

  for (uint64_t i = 0; i < 1024; i++) {
    CachedValue* value =
        CachedValue::construct(&i, sizeof(uint64_t), &i, sizeof(uint64_t));
    bool success = cache->insert(value);
    CHECK(success);
    auto f = cache->find(&i, sizeof(uint64_t));
    CHECK(f.found());
    CHECK(f.value() != nullptr);
    CHECK(f.value()->sameKey(&i, sizeof(uint64_t)));
  }

  for (uint64_t i = 512; i < 1024; i++) {
    bool success = cache->blacklist(&i, sizeof(uint64_t));
    CHECK(success);
    auto f = cache->find(&i, sizeof(uint64_t));
    CHECK(!f.found());
  }

  for (uint64_t i = 512; i < 1024; i++) {
    CachedValue* value =
        CachedValue::construct(&i, sizeof(uint64_t), &i, sizeof(uint64_t));
    bool success = cache->insert(value);
    CHECK(!success);
    delete value;
    auto f = cache->find(&i, sizeof(uint64_t));
    CHECK(!f.found());
  }

  manager.endTransaction(tx);
  tx = manager.beginTransaction(false);

  for (uint64_t i = 512; i < 1024; i++) {
    CachedValue* value =
        CachedValue::construct(&i, sizeof(uint64_t), &i, sizeof(uint64_t));
    bool success = cache->insert(value);
    CHECK(success);
    auto f = cache->find(&i, sizeof(uint64_t));
    CHECK(f.found());
  }

  manager.endTransaction(tx);
  manager.destroyCache(cache);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test growth behavior (single-threaded)
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_st_growth") {
  uint64_t initialSize = 16ULL * 1024ULL;
  uint64_t minimumSize = 64ULL * initialSize;
  MockScheduler scheduler(4);
  Manager manager(scheduler.ioService(), 1024ULL * 1024ULL * 1024ULL);
  auto cache =
      manager.createCache(Manager::CacheType::Transactional, initialSize, true);

  for (uint64_t i = 0; i < 4ULL * 1024ULL * 1024ULL; i++) {
    CachedValue* value =
        CachedValue::construct(&i, sizeof(uint64_t), &i, sizeof(uint64_t));
    bool success = cache->insert(value);
    if (!success) {
      delete value;
    }
  }

  CHECK(cache->usage() > minimumSize);

  manager.destroyCache(cache);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test shrink behavior (single-threaded)
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_st_shrink") {
  uint64_t initialSize = 16ULL * 1024ULL;
  RandomGenerator::initialize(RandomGenerator::RandomType::MERSENNE);
  MockScheduler scheduler(4);
  Manager manager(scheduler.ioService(), 1024ULL * 1024ULL * 1024ULL);
  auto cache =
      manager.createCache(Manager::CacheType::Transactional, initialSize, true);

  for (uint64_t i = 0; i < 16ULL * 1024ULL * 1024ULL; i++) {
    CachedValue* value =
        CachedValue::construct(&i, sizeof(uint64_t), &i, sizeof(uint64_t));
    bool success = cache->insert(value);
    if (!success) {
      delete value;
    }
  }

  cache->disableGrowth();
  uint64_t target = cache->usage() / 2;
  while (!cache->resize(target)) {
  };

  for (uint64_t i = 0; i < 16ULL * 1024ULL * 1024ULL; i++) {
    CachedValue* value =
        CachedValue::construct(&i, sizeof(uint64_t), &i, sizeof(uint64_t));
    bool success = cache->insert(value);
    if (!success) {
      delete value;
    }
  }

  while (cache->isResizing()) {
  }
  CHECK(cache->usage() <= target);

  manager.destroyCache(cache);
  RandomGenerator::shutdown();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test mixed load behavior (multi-threaded)
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_mt_mixed_load") {
  uint64_t initialSize = 16ULL * 1024ULL;
  RandomGenerator::initialize(RandomGenerator::RandomType::MERSENNE);
  MockScheduler scheduler(4);
  Manager manager(scheduler.ioService(), 1024ULL * 1024ULL * 1024ULL);
  size_t threadCount = 4;
  std::shared_ptr<Cache> cache =
      manager.createCache(Manager::CacheType::Transactional, initialSize, true);

  uint64_t chunkSize = 16 * 1024 * 1024;
  uint64_t initialInserts = 4 * 1024 * 1024;
  uint64_t operationCount = 16 * 1024 * 1024;
  std::atomic<uint64_t> hitCount(0);
  std::atomic<uint64_t> missCount(0);
  auto worker = [&manager, &cache, initialInserts, operationCount, &hitCount,
                 &missCount](uint64_t lower, uint64_t upper) -> void {
    Transaction* tx = manager.beginTransaction(false);
    // fill with some initial data
    for (uint64_t i = 0; i < initialInserts; i++) {
      uint64_t item = lower + i;
      CachedValue* value = CachedValue::construct(&item, sizeof(uint64_t),
                                                  &item, sizeof(uint64_t));
      bool ok = cache->insert(value);
      if (!ok) {
        delete value;
      }
    }

    // initialize valid range for keys that *might* be in cache
    uint64_t validLower = lower;
    uint64_t validUpper = lower + initialInserts - 1;
    uint64_t blacklistUpper = validUpper;

    // commence mixed workload
    for (uint64_t i = 0; i < operationCount; i++) {
      uint32_t r = RandomGenerator::interval(static_cast<uint32_t>(99UL));

      if (r >= 99) {  // remove something
        if (validLower == validUpper) {
          continue;  // removed too much
        }

        uint64_t item = validLower++;

        cache->remove(&item, sizeof(uint64_t));
      } else if (r >= 90) {  // insert something
        if (validUpper == upper) {
          continue;  // already maxed out range
        }

        uint64_t item = ++validUpper;
        if (validUpper > blacklistUpper) {
          blacklistUpper = validUpper;
        }
        CachedValue* value = CachedValue::construct(&item, sizeof(uint64_t),
                                                    &item, sizeof(uint64_t));
        bool ok = cache->insert(value);
        if (!ok) {
          delete value;
        }
      } else if (r >= 80) {  // blacklist something
        if (blacklistUpper == upper) {
          continue;  // already maxed out range
        }

        uint64_t item = ++blacklistUpper;
        cache->blacklist(&item, sizeof(uint64_t));
      } else {  // lookup something
        uint64_t item = RandomGenerator::interval(
            static_cast<int64_t>(validLower), static_cast<int64_t>(validUpper));

        Cache::Finding f = cache->find(&item, sizeof(uint64_t));
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
    manager.endTransaction(tx);
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

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|//
// --SECTION--\\|/// @\\}\\)"
// End:
