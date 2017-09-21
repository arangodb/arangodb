////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for arangodb::cache::Rebalancer
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

#include "Cache/Rebalancer.h"
#include "Basics/Common.h"
#include "Cache/Common.h"
#include "Cache/Manager.h"
#include "Cache/PlainCache.h"
#include "Cache/Transaction.h"
#include "Cache/TransactionalCache.h"
#include "Random/RandomGenerator.h"

#include "MockScheduler.h"
#include "catch.hpp"

#include <stdint.h>
#include <queue>
#include <string>
#include <thread>
#include <vector>

using namespace arangodb;
using namespace arangodb::cache;

TEST_CASE("cache::Rebalancer", "[cache][!hide][longRunning]") {
  SECTION("test rebalancing with PlainCache") {
    RandomGenerator::initialize(RandomGenerator::RandomType::MERSENNE);
    MockScheduler scheduler(4);
    auto postFn = [&scheduler](std::function<void()> fn) -> bool {
      scheduler.post(fn);
      return true;
    };
    Manager manager(postFn, 128 * 1024 * 1024);
    Rebalancer rebalancer(&manager);

    size_t cacheCount = 4;
    size_t threadCount = 4;
    std::vector<std::shared_ptr<Cache>> caches;
    for (size_t i = 0; i < cacheCount; i++) {
      caches.emplace_back(manager.createCache(CacheType::Plain));
    }

    bool doneRebalancing = false;
    auto rebalanceWorker = [&rebalancer, &doneRebalancing]() -> void {
      while (!doneRebalancing) {
        int status = rebalancer.rebalance();
        if (status != TRI_ERROR_ARANGO_BUSY) {
          usleep(500 * 1000);
        } else {
          usleep(10 * 1000);
        }
      }
    };
    auto rebalancerThread = new std::thread(rebalanceWorker);

    uint64_t chunkSize = 4 * 1024 * 1024;
    uint64_t initialInserts = 1 * 1024 * 1024;
    uint64_t operationCount = 4 * 1024 * 1024;
    std::atomic<uint64_t> hitCount(0);
    std::atomic<uint64_t> missCount(0);
    auto worker = [&manager, &caches, cacheCount, initialInserts,
                   operationCount, &hitCount,
                   &missCount](uint64_t lower, uint64_t upper) -> void {
      // fill with some initial data
      for (uint64_t i = 0; i < initialInserts; i++) {
        uint64_t item = lower + i;
        size_t cacheIndex = item % cacheCount;
        CachedValue* value = CachedValue::construct(&item, sizeof(uint64_t),
                                                    &item, sizeof(uint64_t));
        TRI_ASSERT(value != nullptr);
        auto status = caches[cacheIndex]->insert(value);
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
          size_t cacheIndex = item % cacheCount;

          caches[cacheIndex]->remove(&item, sizeof(uint64_t));
        } else if (r >= 95) {  // insert something
          if (validUpper == upper) {
            continue;  // already maxed out range
          }

          uint64_t item = ++validUpper;
          size_t cacheIndex = item % cacheCount;
          CachedValue* value = CachedValue::construct(&item, sizeof(uint64_t),
                                                      &item, sizeof(uint64_t));
          TRI_ASSERT(value != nullptr);
          auto status = caches[cacheIndex]->insert(value);
          if (status.fail()) {
            delete value;
          }
        } else {  // lookup something
          uint64_t item =
              RandomGenerator::interval(static_cast<int64_t>(validLower),
                                        static_cast<int64_t>(validUpper));
          size_t cacheIndex = item % cacheCount;

          Finding f = caches[cacheIndex]->find(&item, sizeof(uint64_t));
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

    doneRebalancing = true;
    rebalancerThread->join();
    delete rebalancerThread;

    for (auto cache : caches) {
      manager.destroyCache(cache);
    }

    RandomGenerator::shutdown();
  }

  SECTION("test rebalancing with TransactionalCache") {
    RandomGenerator::initialize(RandomGenerator::RandomType::MERSENNE);
    MockScheduler scheduler(4);
    auto postFn = [&scheduler](std::function<void()> fn) -> bool {
      scheduler.post(fn);
      return true;
    };
    Manager manager(postFn, 128 * 1024 * 1024);
    Rebalancer rebalancer(&manager);

    size_t cacheCount = 4;
    size_t threadCount = 4;
    std::vector<std::shared_ptr<Cache>> caches;
    for (size_t i = 0; i < cacheCount; i++) {
      caches.emplace_back(manager.createCache(CacheType::Transactional));
    }

    bool doneRebalancing = false;
    auto rebalanceWorker = [&rebalancer, &doneRebalancing]() -> void {
      while (!doneRebalancing) {
        int status = rebalancer.rebalance();
        if (status != TRI_ERROR_ARANGO_BUSY) {
          usleep(500 * 1000);
        } else {
          usleep(10 * 1000);
        }
      }
    };
    auto rebalancerThread = new std::thread(rebalanceWorker);

    uint64_t chunkSize = 4 * 1024 * 1024;
    uint64_t initialInserts = 1 * 1024 * 1024;
    uint64_t operationCount = 4 * 1024 * 1024;
    std::atomic<uint64_t> hitCount(0);
    std::atomic<uint64_t> missCount(0);
    auto worker = [&manager, &caches, cacheCount, initialInserts,
                   operationCount, &hitCount,
                   &missCount](uint64_t lower, uint64_t upper) -> void {
      Transaction* tx = manager.beginTransaction(false);
      // fill with some initial data
      for (uint64_t i = 0; i < initialInserts; i++) {
        uint64_t item = lower + i;
        size_t cacheIndex = item % cacheCount;
        CachedValue* value = CachedValue::construct(&item, sizeof(uint64_t),
                                                    &item, sizeof(uint64_t));
        TRI_ASSERT(value != nullptr);
        auto status = caches[cacheIndex]->insert(value);
        if (status.fail()) {
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
          size_t cacheIndex = item % cacheCount;

          caches[cacheIndex]->remove(&item, sizeof(uint64_t));
        } else if (r >= 90) {  // insert something
          if (validUpper == upper) {
            continue;  // already maxed out range
          }

          uint64_t item = ++validUpper;
          if (validUpper > blacklistUpper) {
            blacklistUpper = validUpper;
          }
          size_t cacheIndex = item % cacheCount;
          CachedValue* value = CachedValue::construct(&item, sizeof(uint64_t),
                                                      &item, sizeof(uint64_t));
          TRI_ASSERT(value != nullptr);
          auto status = caches[cacheIndex]->insert(value);
          if (status.fail()) {
            delete value;
          }
        } else if (r >= 80) {  // blacklist something
          if (blacklistUpper == upper) {
            continue;  // already maxed out range
          }

          uint64_t item = ++blacklistUpper;
          size_t cacheIndex = item % cacheCount;
          caches[cacheIndex]->blacklist(&item, sizeof(uint64_t));
        } else {  // lookup something
          uint64_t item =
              RandomGenerator::interval(static_cast<int64_t>(validLower),
                                        static_cast<int64_t>(validUpper));
          size_t cacheIndex = item % cacheCount;

          Finding f = caches[cacheIndex]->find(&item, sizeof(uint64_t));
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

    doneRebalancing = true;
    rebalancerThread->join();
    delete rebalancerThread;

    for (auto cache : caches) {
      manager.destroyCache(cache);
    }

    RandomGenerator::shutdown();
  }
}
