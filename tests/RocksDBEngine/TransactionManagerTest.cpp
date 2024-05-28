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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Metrics/MetricsFeature.h"
#include "Transaction/Manager.h"
#include "Transaction/ManagerFeature.h"
#include "Cluster/ClusterFeature.h"
#include "Metrics/ClusterMetricsFeature.h"
#include "Statistics/StatisticsFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"

using namespace arangodb;

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

/// @brief test RocksDBTransactionManager

/// @brief simple non-overlapping
TEST(RocksDBTransactionManager, test_non_overlapping) {
  ArangodServer server{nullptr, nullptr};
  server.addFeature<metrics::MetricsFeature>(
      LazyApplicationFeatureReference<QueryRegistryFeature>(nullptr),
      LazyApplicationFeatureReference<StatisticsFeature>(nullptr),
      LazyApplicationFeatureReference<EngineSelectorFeature>(nullptr),
      LazyApplicationFeatureReference<metrics::ClusterMetricsFeature>(nullptr),
      LazyApplicationFeatureReference<ClusterFeature>(nullptr));
  transaction::ManagerFeature feature(server);
  transaction::Manager tm(feature);

  EXPECT_EQ(tm.getActiveTransactionCount(), 0);
  EXPECT_TRUE(tm.holdTransactions(500));
  tm.releaseTransactions();

  auto guard =
      tm.registerTransaction(static_cast<TransactionId>(1), false, false);
  EXPECT_EQ(tm.getActiveTransactionCount(), 1);
  guard.reset();
  EXPECT_EQ(tm.getActiveTransactionCount(), 0);

  EXPECT_TRUE(tm.holdTransactions(500));
  tm.releaseTransactions();
}

/// @brief simple non-overlapping
TEST(RocksDBTransactionManager, test_overlapping) {
  auto trxId = static_cast<TransactionId>(1);
  ArangodServer server{nullptr, nullptr};
  server.addFeature<metrics::MetricsFeature>(
      LazyApplicationFeatureReference<QueryRegistryFeature>(nullptr),
      LazyApplicationFeatureReference<StatisticsFeature>(nullptr),
      LazyApplicationFeatureReference<EngineSelectorFeature>(nullptr),
      LazyApplicationFeatureReference<metrics::ClusterMetricsFeature>(nullptr),
      LazyApplicationFeatureReference<ClusterFeature>(nullptr));
  transaction::ManagerFeature feature(server);
  transaction::Manager tm(feature);

  std::chrono::milliseconds five(5);

  EXPECT_EQ(tm.getActiveTransactionCount(), 0);
  EXPECT_TRUE(tm.holdTransactions(500));

  auto guard = tm.registerTransaction(trxId, false, false);
  EXPECT_EQ(tm.getActiveTransactionCount(), 1);

  std::atomic<bool> done;

  auto getReadLock = [&]() -> void {
    tm.commitManagedTrx(trxId, "foo").get();
    done = true;
  };

  std::thread reader(getReadLock);

  EXPECT_EQ(tm.getActiveTransactionCount(), 1);
  std::this_thread::sleep_for(five);
  EXPECT_FALSE(done);

  tm.releaseTransactions();

  reader.join();

  EXPECT_EQ(tm.getActiveTransactionCount(), 1);
  guard.reset();
  EXPECT_EQ(tm.getActiveTransactionCount(), 0);
}
