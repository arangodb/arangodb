////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for arangodb::cache::TransactionalCache with backing store
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
#include "Cache/Manager.h"
#include "Cache/Rebalancer.h"
#include "Random/RandomGenerator.h"

#include "MockScheduler.h"
#include "TransactionalStore.h"
#include "gtest/gtest.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

using namespace arangodb;
using namespace arangodb::cache;

struct ThreadGuard {
  ThreadGuard(ThreadGuard&& other) noexcept = default;
  ThreadGuard& operator=(ThreadGuard&& other) noexcept = default;

  ThreadGuard(std::unique_ptr<std::thread> thread)
      : thread(std::move(thread)) {}
  ~ThreadGuard() {
    join();
  }

  void join() {
    if (thread != nullptr) {
      thread->join();
      thread.reset();
    }
  }

  std::unique_ptr<std::thread> thread;
};

// long-running

/*
Planned Tests
=============

All tests begin by filling the database with a set number of documents. After
that, all writes consist of updates via the Document::advance() API to both keep
things simple and to provide a reliable way to test what version of a document a
reader gets.

  1) Single store; Read-only; hotset access pattern
    - Test for hit rate

  2) Single store; Simultaneous read, write threads, part 1
    - Have writers sleep a while between transactions
    - Have readers read single documents with only internal transactions
    - Test for hit rate

  3) Single store; Simultaneous read, write threads, part 2
    - Have writers sleep a while between transactions
    - Have readers read a set of documents within a transaction
    - Test for snapshot isolation to the extent possible
    - Test for hit rate

  4) Multiple stores with rebalancing; Simultaneous read, write threads
    - Use small global limit to provide memory pressure
    - Vary store-access bias over time to check that rebalancing helps
    - Have writers sleep a while between transactions
    - Have readers read a set of documents within a transaction
*/
TEST(CacheWithBackingStoreTest, test_hit_rate_for_readonly_hotset_workload_LongRunning) {
  RandomGenerator::initialize(RandomGenerator::RandomType::MERSENNE);
  MockScheduler scheduler(4);
  auto postFn = [&scheduler](std::function<void()> fn) -> bool {
    scheduler.post(fn);
    return true;
  };
  Manager manager(postFn, 16 * 1024 * 1024);
  TransactionalStore store(&manager);
  uint64_t totalDocuments = 1000000;
  uint64_t hotsetSize = 50000;
  size_t threadCount = 4;
  uint64_t lookupsPerThread = 1000000;

  // initial fill
  for (uint64_t i = 1; i <= totalDocuments; i++) {
    store.insert(nullptr, TransactionalStore::Document(i));
  }

  auto worker = [&store, hotsetSize, totalDocuments, lookupsPerThread]() -> void {
    for (uint64_t i = 0; i < lookupsPerThread; i++) {
      uint32_t r = RandomGenerator::interval(static_cast<uint32_t>(99));
      uint64_t choice = (r >= 90) ? RandomGenerator::interval(totalDocuments)
                                  : RandomGenerator::interval(hotsetSize);
      if (choice == 0) {
        choice = 1;
      }

      auto d = store.lookup(nullptr, choice);
      TRI_ASSERT(!d.empty());
    }
  };

  std::vector<ThreadGuard> threads;
  // dispatch threads
  for (size_t i = 0; i < threadCount; i++) {
    threads.emplace_back(std::make_unique<std::thread>(worker));
  }

  // join threads
  threads.clear();

  auto hitRates = manager.globalHitRates();
  EXPECT_TRUE(hitRates.first >= 15.0);
  EXPECT_TRUE(hitRates.second >= 30.0);

  RandomGenerator::shutdown();
}

TEST(CacheWithBackingStoreTest, test_hit_rate_for_mixed_workload_LongRunning) {
  RandomGenerator::initialize(RandomGenerator::RandomType::MERSENNE);
  MockScheduler scheduler(4);
  auto postFn = [&scheduler](std::function<void()> fn) -> bool {
    scheduler.post(fn);
    return true;
  };
  Manager manager(postFn, 256 * 1024 * 1024);
  TransactionalStore store(&manager);
  uint64_t totalDocuments = 1000000;
  uint64_t batchSize = 1000;
  size_t readerCount = 4;
  size_t writerCount = 2;
  std::atomic<size_t> writersDone(0);
  auto writeWaitInterval = std::chrono::milliseconds(10);

  // initial fill
  for (uint64_t i = 1; i <= totalDocuments; i++) {
    store.insert(nullptr, TransactionalStore::Document(i));
  }

  auto readWorker = [&store, &writersDone, writerCount, totalDocuments]() -> void {
    while (writersDone.load() < writerCount) {
      uint64_t choice = RandomGenerator::interval(totalDocuments);
      if (choice == 0) {
        choice = 1;
      }

      auto d = store.lookup(nullptr, choice);
      TRI_ASSERT(!d.empty());
    }
  };

  auto writeWorker = [&store, &writersDone, batchSize,
                      &writeWaitInterval](uint64_t lower, uint64_t upper) -> void {
    uint64_t batches = (upper + 1 - lower) / batchSize;
    uint64_t choice = lower;
    for (uint64_t batch = 0; batch < batches; batch++) {
      auto tx = store.beginTransaction(false);
      for (uint64_t i = 0; i < batchSize; i++) {
        auto d = store.lookup(tx, choice);
        TRI_ASSERT(!d.empty());
        d.advance();
        bool ok = store.update(tx, d);
        TRI_ASSERT(ok);
        choice++;
      }
      bool ok = store.commit(tx);
      TRI_ASSERT(ok);
      std::this_thread::sleep_for(writeWaitInterval);
    }
    writersDone++;
  };

  std::vector<ThreadGuard> threads;
  // dispatch reader threads
  for (size_t i = 0; i < readerCount; i++) {
    threads.emplace_back(std::make_unique<std::thread>(readWorker));
  }
  // dispatch writer threads
  uint64_t chunkSize = totalDocuments / writerCount;
  for (size_t i = 0; i < writerCount; i++) {
    uint64_t lower = (i * chunkSize) + 1;
    uint64_t upper = ((i + 1) * chunkSize);
    threads.emplace_back(std::make_unique<std::thread>(writeWorker, lower, upper));
  }

  // join threads
  threads.clear();

  auto hitRates = manager.globalHitRates();
  EXPECT_TRUE(hitRates.first >= 0.1);
  EXPECT_TRUE(hitRates.second >= 2.5);

  RandomGenerator::shutdown();
}

TEST(CacheWithBackingStoreTest, test_transactionality_for_mixed_workload_LongRunning) {
  RandomGenerator::initialize(RandomGenerator::RandomType::MERSENNE);
  MockScheduler scheduler(4);
  auto postFn = [&scheduler](std::function<void()> fn) -> bool {
    scheduler.post(fn);
    return true;
  };
  Manager manager(postFn, 256 * 1024 * 1024);
  TransactionalStore store(&manager);
  uint64_t totalDocuments = 1000000;
  uint64_t writeBatchSize = 1000;
  uint64_t readBatchSize = 10000;
  size_t readerCount = 4;
  size_t writerCount = 2;
  std::atomic<size_t> writersDone(0);
  auto writeWaitInterval = std::chrono::milliseconds(10);

  // initial fill
  for (uint64_t i = 1; i <= totalDocuments; i++) {
    store.insert(nullptr, TransactionalStore::Document(i));
  }

  auto readWorker = [&store, &writersDone, writerCount, totalDocuments,
                     readBatchSize]() -> void {
    while (writersDone.load() < writerCount) {
      auto tx = store.beginTransaction(true);
      uint64_t start = static_cast<uint64_t>(
          std::chrono::steady_clock::now().time_since_epoch().count());
      for (uint64_t i = 0; i < readBatchSize; i++) {
        uint64_t choice = RandomGenerator::interval(totalDocuments);
        if (choice == 0) {
          choice = 1;
        }

        auto d = store.lookup(tx, choice);
        TRI_ASSERT(!d.empty());
        TRI_ASSERT(d.timestamp <= start);  // transactionality
      }
      bool ok = store.commit(tx);
      TRI_ASSERT(ok);
    }
  };

  auto writeWorker = [&store, &writersDone, writeBatchSize,
                      &writeWaitInterval](uint64_t lower, uint64_t upper) -> void {
    uint64_t batches = (upper + 1 - lower) / writeBatchSize;
    uint64_t choice = lower;
    for (uint64_t batch = 0; batch < batches; batch++) {
      auto tx = store.beginTransaction(false);
      for (uint64_t i = 0; i < writeBatchSize; i++) {
        auto d = store.lookup(tx, choice);
        TRI_ASSERT(!d.empty());
        d.advance();
        bool ok = store.update(tx, d);
        TRI_ASSERT(ok);
        choice++;
      }
      bool ok = store.commit(tx);
      TRI_ASSERT(ok);
      std::this_thread::sleep_for(writeWaitInterval);
    }
    writersDone++;
  };

  std::vector<ThreadGuard> threads;
  // dispatch reader threads
  for (size_t i = 0; i < readerCount; i++) {
    threads.emplace_back(std::make_unique<std::thread>(readWorker));
  }
  // dispatch writer threads
  uint64_t chunkSize = totalDocuments / writerCount;
  for (size_t i = 0; i < writerCount; i++) {
    uint64_t lower = (i * chunkSize) + 1;
    uint64_t upper = ((i + 1) * chunkSize);
    threads.emplace_back(std::make_unique<std::thread>(writeWorker, lower, upper));
  }

  // join threads
  threads.clear();

  RandomGenerator::shutdown();
}

TEST(CacheWithBackingStoreTest, test_rebalancing_in_the_wild_LongRunning) {
  RandomGenerator::initialize(RandomGenerator::RandomType::MERSENNE);
  MockScheduler scheduler(4);
  auto postFn = [&scheduler](std::function<void()> fn) -> bool {
    scheduler.post(fn);
    return true;
  };
  Manager manager(postFn, 16 * 1024 * 1024);
  Rebalancer rebalancer(&manager);
  TransactionalStore store1(&manager);
  TransactionalStore store2(&manager);
  uint64_t totalDocuments = 1000000;
  uint64_t writeBatchSize = 1000;
  uint64_t readBatchSize = 100;
  size_t readerCount = 4;
  size_t writerCount = 2;
  std::atomic<size_t> writersDone(0);
  auto writeWaitInterval = std::chrono::milliseconds(50);
  uint32_t storeBias;

  bool doneRebalancing = false;
  auto rebalanceWorker = [&rebalancer, &doneRebalancing]() -> void {
    while (!doneRebalancing) {
      int status = rebalancer.rebalance();
      if (status != TRI_ERROR_ARANGO_BUSY) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
      } else {
        std::this_thread::sleep_for(std::chrono::microseconds(100));
      }
    }
  };

  ThreadGuard rebalancerThread(std::make_unique<std::thread>(rebalanceWorker));

  // initial fill
  for (uint64_t i = 1; i <= totalDocuments; i++) {
    store1.insert(nullptr, TransactionalStore::Document(i));
    store2.insert(nullptr, TransactionalStore::Document(i));
  }

  auto readWorker = [&store1, &store2, &storeBias, &writersDone, writerCount,
                     totalDocuments, readBatchSize]() -> void {
    while (writersDone.load() < writerCount) {
      uint32_t r = RandomGenerator::interval(static_cast<uint32_t>(99UL));
      TransactionalStore* store = (r <= storeBias) ? &store1 : &store2;
      auto tx = store->beginTransaction(true);
      uint64_t start = static_cast<uint64_t>(
          std::chrono::steady_clock::now().time_since_epoch().count());
      for (uint64_t i = 0; i < readBatchSize; i++) {
        uint64_t choice = RandomGenerator::interval(totalDocuments);
        if (choice == 0) {
          choice = 1;
        }

        auto d = store->lookup(tx, choice);
        TRI_ASSERT(!d.empty());
        TRI_ASSERT(d.timestamp <= start);  // transactionality
      }
      bool ok = store->commit(tx);
      TRI_ASSERT(ok);
    }
  };

  auto writeWorker = [&store1, &store2, &storeBias, &writersDone, writeBatchSize,
                      &writeWaitInterval](uint64_t lower, uint64_t upper) -> void {
    uint64_t batches = (upper + 1 - lower) / writeBatchSize;
    uint64_t choice = lower;
    for (uint64_t batch = 0; batch < batches; batch++) {
      uint32_t r = RandomGenerator::interval(static_cast<uint32_t>(99UL));
      TransactionalStore* store = (r <= storeBias) ? &store1 : &store2;
      auto tx = store->beginTransaction(false);
      for (uint64_t i = 0; i < writeBatchSize; i++) {
        auto d = store->lookup(tx, choice);
        TRI_ASSERT(!d.empty());
        d.advance();
        bool ok = store->update(tx, d);
        TRI_ASSERT(ok);
        choice++;
      }
      bool ok = store->commit(tx);
      TRI_ASSERT(ok);
      std::this_thread::sleep_for(writeWaitInterval);
    }
    writersDone++;
  };


  // bias toward first store
  storeBias = 80;

  // dispatch reader threads
  std::vector<ThreadGuard> threads;
  for (size_t i = 0; i < readerCount; i++) {
    threads.emplace_back(std::make_unique<std::thread>(readWorker));
  }
  // dispatch writer threads
  uint64_t chunkSize = totalDocuments / writerCount;
  for (size_t i = 0; i < writerCount; i++) {
    uint64_t lower = (i * chunkSize) + 1;
    uint64_t upper = ((i + 1) * chunkSize);
    threads.emplace_back(std::make_unique<std::thread>(writeWorker, lower, upper));
  }

  // join threads
  threads.clear();

  while (store1.cache()->isResizing() || store2.cache()->isResizing()) {
    std::this_thread::yield();
  }

  // bias toward second store
  storeBias = 20;

  // dispatch reader threads
  for (size_t i = 0; i < readerCount; i++) {
    threads.emplace_back(std::make_unique<std::thread>(readWorker));
  }
  // dispatch writer threads
  for (size_t i = 0; i < writerCount; i++) {
    uint64_t lower = (i * chunkSize) + 1;
    uint64_t upper = ((i + 1) * chunkSize);
    threads.emplace_back(std::make_unique<std::thread>(writeWorker, lower, upper));
  }

  // join threads
  threads.clear();

  while (store1.cache()->isResizing() || store2.cache()->isResizing()) {
    std::this_thread::yield();
  }
  doneRebalancing = true;
  rebalancerThread.join();

  RandomGenerator::shutdown();
}
