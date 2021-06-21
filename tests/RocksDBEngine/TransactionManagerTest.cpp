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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"

#include "gtest/gtest.h"
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "ApplicationFeatures/ApplicationServer.h"
#include "RestServer/MetricsFeature.h"
#include "Transaction/Manager.h"
#include "Transaction/ManagerFeature.h"

using namespace arangodb;

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

/// @brief test RocksDBTransactionManager

/// @brief simple non-overlapping
TEST(RocksDBTransactionManager, test_non_overlapping) {
  application_features::ApplicationServer server{nullptr, nullptr};
  server.addFeature<MetricsFeature>();
  transaction::ManagerFeature feature(server);
  transaction::Manager tm(feature);

  EXPECT_EQ(tm.getActiveTransactionCount(), 0);
  EXPECT_TRUE(tm.holdTransactions(500) );
  tm.releaseTransactions();

  tm.registerTransaction(static_cast<TransactionId>(1), false, false);
  EXPECT_EQ(tm.getActiveTransactionCount(), 1);
  tm.unregisterTransaction(static_cast<TransactionId>(1), false, false);
  EXPECT_EQ(tm.getActiveTransactionCount(), 0);

  EXPECT_TRUE(tm.holdTransactions(500) );
  tm.releaseTransactions();
}

/// @brief simple non-overlapping
TEST(RocksDBTransactionManager, test_overlapping) {
  application_features::ApplicationServer server{nullptr, nullptr};
  server.addFeature<MetricsFeature>();
  transaction::ManagerFeature feature(server);
  transaction::Manager tm(feature);

  std::chrono::milliseconds five(5);
  std::mutex mu;
  std::condition_variable cv;

  EXPECT_EQ(tm.getActiveTransactionCount(), 0);
  EXPECT_TRUE(tm.holdTransactions(500) );

  std::unique_lock<std::mutex> lock(mu);

  auto getReadLock = [&] () -> void {
    {
      std::unique_lock<std::mutex> innerLock(mu);
      cv.notify_all();
    }

    tm.registerTransaction(static_cast<TransactionId>(1), false, false);
    EXPECT_EQ(tm.getActiveTransactionCount(), 1);
  };

  std::thread reader(getReadLock);

  cv.wait(lock);
  EXPECT_EQ(tm.getActiveTransactionCount(), 0);
  std::this_thread::sleep_for(five);
  EXPECT_EQ(tm.getActiveTransactionCount(), 0);
  tm.releaseTransactions();

  reader.join();
  EXPECT_EQ(tm.getActiveTransactionCount(), 1);
  tm.unregisterTransaction(static_cast<TransactionId>(1), false, false);
  EXPECT_EQ(tm.getActiveTransactionCount(), 0);
}
