////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
/// @author Julia Puget
////////////////////////////////////////////////////////////////////////////////

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Mocks.h"

// concrete — both in arango_rocksdb
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBOptionsProvider.h"
#include "RocksDBEngine/RocksDBRecoveryManager.h"

using namespace arangodb;
using namespace arangodb::tests;
using ::testing::_;
using ::testing::ReturnRef;

struct TestRocksDBOptionsProvider final : RocksDBOptionsProvider {
  rocksdb::TransactionDBOptions getTransactionDBOptions() const override {
    return {};
  }
  uint64_t maxTotalWalSize() const noexcept override { return 0; }
  uint32_t numThreadsHigh() const noexcept override { return 2; }
  uint32_t numThreadsLow() const noexcept override { return 2; }
  uint64_t periodicCompactionTtl() const noexcept override { return 0; }

 protected:
  rocksdb::Options doGetOptions() const override { return {}; }
  rocksdb::BlockBasedTableOptions doGetTableOptions() const override {
    return {};
  }
};

TEST(RocksDBEngineMinimal, CanConstruct) {
  MockMetricsCollector metricsCollector;
  ON_CALL(metricsCollector, doAdd(_))
      .WillByDefault([](metrics::Builder& builder) { return builder.build(); });

  std::string dir{"/tmp/test"};
  MockDatabasePathProvider dbPath;
  ON_CALL(dbPath, directory()).WillByDefault(ReturnRef(dir));
  ON_CALL(dbPath, subdirectoryName(_))
      .WillByDefault(
          [&dir](std::string const& sub) { return dir + "/" + sub; });

  DumpLimitsFeatureOptions limitsOptions{};
  MockDumpLimitsProvider dumpLimits;
  ON_CALL(dumpLimits, limits()).WillByDefault(ReturnRef(limitsOptions));

  application_features::ApplicationServer server{nullptr, nullptr};
  auto& recovery = server.addFeature<RocksDBRecoveryManager>();

  TestRocksDBOptionsProvider optionsProvider;
  MockVectorIndexProvider vectorIdx;
  MockFlushControl flush;
  MockDatabaseProvider dbProvider;
  MockCacheManagerProvider cacheManager;
  MockSortingPolicy sortingPolicy;
  MockIndexCacheRefill indexCacheRefill;

  auto& engine = server.addFeature<RocksDBEngine>(
      optionsProvider, metricsCollector, dbPath, vectorIdx, flush, dumpLimits,
      nullptr /* IReplicatedLogProvider* */, recovery, dbProvider,
      indexCacheRefill, cacheManager, sortingPolicy);

  EXPECT_EQ(engine.kEngineName, "rocksdb");
}
