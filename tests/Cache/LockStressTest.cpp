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
/// @author Dan Larkin-York
/// @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>
#include <vector>

#include "Cache/Manager.h"
#include "Cache/Rebalancer.h"
#include "Random/RandomGenerator.h"

#include "MockScheduler.h"
#include "TransactionalStore.h"

using namespace arangodb;
using namespace arangodb::cache;

// long-running

TEST(CacheLockStressTest, test_transactionality_for_mixed_load) {
  RandomGenerator::initialize(RandomGenerator::RandomType::MERSENNE);
  MockScheduler scheduler(4);
  Manager manager(scheduler.ioService(), 32 * 1024 * 1024);
  TransactionalStore store(&manager);
  std::uint64_t totalDocuments = 1000000;
  std::uint64_t readBatchSize = 10000;
  std::uint64_t numBatches = 250;
  std::size_t readerCount = 24;

  // initial fill
  for (std::uint64_t i = 1; i <= totalDocuments; i++) {
    store.insert(nullptr, TransactionalStore::Document(i));
  }

  auto readWorker = [&store, totalDocuments, readBatchSize, numBatches]() -> void {
    for (std::uint64_t batch = 0; batch < numBatches; batch++) {
      auto tx = store.beginTransaction(true);
      for (std::uint64_t i = 0; i < readBatchSize; i++) {
        std::uint64_t choice = RandomGenerator::interval(totalDocuments);
        if (choice == 0) {
          choice = 1;
        }

        auto d = store.lookup(tx, choice);
        TRI_ASSERT(!d.empty());
      }
      bool ok = store.commit(tx);
      TRI_ASSERT(ok);
    }
  };

  auto start = std::chrono::high_resolution_clock::now();

  std::vector<std::thread*> threads;
  // dispatch reader threads
  for (std::size_t i = 0; i < readerCount; i++) {
    threads.push_back(new std::thread(readWorker));
  }

  // join threads
  for (auto t : threads) {
    t->join();
    delete t;
  }

  auto end = std::chrono::high_resolution_clock::now();
  std::cout << "time: " << static_cast<double>((end - start).count()) / 1000000000
            << std::endl;

  RandomGenerator::shutdown();
}
