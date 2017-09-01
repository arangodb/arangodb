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
#include "catch.hpp"

#include <stdint.h>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

using namespace arangodb;
using namespace arangodb::cache;

TEST_CASE("lock stress test", "[cache][!hide][longRunning]") {
  SECTION("test transactionality for mixed workload") {
    RandomGenerator::initialize(RandomGenerator::RandomType::MERSENNE);
    MockScheduler scheduler(4);
    Manager manager(scheduler.ioService(), 32 * 1024 * 1024);
    TransactionalStore store(&manager);
    uint64_t totalDocuments = 1000000;
    uint64_t readBatchSize = 10000;
    uint64_t numBatches = 250;
    size_t readerCount = 24;

    // initial fill
    for (uint64_t i = 1; i <= totalDocuments; i++) {
      store.insert(nullptr, TransactionalStore::Document(i));
    }

    auto readWorker = [&store, totalDocuments,
                       readBatchSize, numBatches]() -> void {
      for (uint64_t batch = 0; batch < numBatches; batch++) {
        auto tx = store.beginTransaction(true);
        for (uint64_t i = 0; i < readBatchSize; i++) {
          uint64_t choice = RandomGenerator::interval(totalDocuments);
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
    for (size_t i = 0; i < readerCount; i++) {
      threads.push_back(new std::thread(readWorker));
    }

    // join threads
    for (auto t : threads) {
      t->join();
      delete t;
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::cout << "time: " << static_cast<double>((end - start).count()) / 1000000000 << std::endl;

    RandomGenerator::shutdown();
  }
}
