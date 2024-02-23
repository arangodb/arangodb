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

#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>
#include <vector>

#include "Basics/ThreadGuard.h"
#include "Cache/Manager.h"
#include "Cache/Rebalancer.h"
#include "Random/RandomGenerator.h"

#include "Mocks/Servers.h"

#include "MockScheduler.h"
#include "TransactionalStore.h"

using namespace arangodb;
using namespace arangodb::cache;
using namespace arangodb::tests::mocks;

// long-running

TEST(CacheLockStressTest, test_transactionality_for_mixed_load) {
  RandomGenerator::initialize(RandomGenerator::RandomType::MERSENNE);
  MockScheduler scheduler(4);
  auto postFn = [&scheduler](std::function<void()> fn) -> bool {
    scheduler.post(fn);
    return true;
  };
  MockMetricsServer server;
  SharedPRNGFeature& sharedPRNG = server.getFeature<SharedPRNGFeature>();
  Manager manager(sharedPRNG, postFn, 16 * 1024 * 1024);
  TransactionalStore store(&manager);
  std::uint64_t totalDocuments = 1000000;
  std::uint64_t readBatchSize = 10000;
  std::uint64_t numBatches = 250;
  std::size_t readerCount = 24;

  // initial fill
  for (std::uint64_t i = 1; i <= totalDocuments; i++) {
    store.insert(nullptr, TransactionalStore::Document(i));
  }

  auto readWorker = [&store, totalDocuments, readBatchSize,
                     numBatches]() -> void {
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

  auto threads = ThreadGuard(readerCount);

  // dispatch reader threads
  for (std::size_t i = 0; i < readerCount; i++) {
    threads.emplace(readWorker);
  }

  threads.joinAll();

  auto end = std::chrono::high_resolution_clock::now();
  std::cout << "time: "
            << static_cast<double>((end - start).count()) / 1000000000
            << std::endl;

  RandomGenerator::shutdown();
}
