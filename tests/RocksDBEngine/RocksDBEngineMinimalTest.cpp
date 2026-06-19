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

#include "gtest/gtest.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Metrics/ICollector.h"

// interface headers
#include "RocksDBEngine/ISortingPolicy.h"
#include "Cache/ICacheManagerProvider.h"
#include "Replication2/ReplicatedLog/IReplicatedLogProvider.h"
#include "RestServer/IDatabasePathProvider.h"
#include "RestServer/IDatabaseProvider.h"
#include "RestServer/IDumpLimitsProvider.h"
#include "RestServer/IFlushControl.h"
#include "RocksDBEngine/IIndexCacheRefill.h"
#include "VectorIndex/IVectorIndexProvider.h"

// concrete — both in arango_rocksdb
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBOptionsProvider.h"
#include "RocksDBEngine/RocksDBRecoveryManager.h"

using namespace arangodb;

// --- stubs ---

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

struct TestDatabasePathProvider final : IDatabasePathProvider {
  std::string const& directory() const override { return _dir; }
  std::string subdirectoryName(std::string const& sub) const override {
    return _dir + "/" + sub;
  }
  std::string _dir{"/tmp/test"};
};

struct TestVectorIndexProvider final : IVectorIndexProvider {
  bool isVectorIndexEnabled() const noexcept override { return false; }
};

struct TestFlushControl final : IFlushControl {
  bool isEnabled() const noexcept override { return false; }
  std::tuple<std::size_t, std::size_t, TRI_voc_tick_t> releaseUnusedTicks()
      override {
    return {0, 0, 0};
  }
};

struct TestDumpLimitsProvider final : IDumpLimitsProvider {
  DumpLimitsFeatureOptions const& limits() const noexcept override {
    return _limits;
  }
  DumpLimitsFeatureOptions _limits{};
};

struct TestDatabaseProvider final : IDatabaseProvider {
  VocbasePtr useDatabase(std::string_view) const override { return {}; }
  VocbasePtr useDatabase(TRI_voc_tick_t) const override { return {}; }
  void enumerateDatabases(std::function<void(TRI_vocbase_t&)> const&) override {
  }
  void inventory(
      velocypack::Builder&, TRI_voc_tick_t,
      std::function<bool(LogicalCollection const*)> const&) override {}
  replication::Version defaultReplicationVersion() const noexcept override {
    return {};
  }
  bool extendedNames() const noexcept override { return false; }
  void extendedNames(bool) noexcept override {}
};

struct TestCacheManagerProvider final : ICacheManagerProvider {
  cache::Manager* manager() override { return nullptr; }
};

struct TestSortingPolicy final : ISortingPolicy {
  bool useLegacySorting() const noexcept override { return false; }
};

struct TestIndexCacheRefill final : IIndexCacheRefill {
  void scheduleFullIndexRefill(std::string const&, std::string const&,
                               IndexId) override {}
  bool autoRefill() const noexcept override { return false; }
  bool autoRefillOnFollowers() const noexcept override { return false; }
  void waitForCatchup() override {}
};

struct TestMetricsCollector final : metrics::ICollector {
 protected:
  std::shared_ptr<metrics::Metric> doAdd(metrics::Builder& builder) override {
    auto metric = builder.build();
    _metrics.push_back(metric);
    return metric;
  }

 private:
  std::vector<std::shared_ptr<metrics::Metric>> _metrics;
};

// --- test ---

TEST(RocksDBEngineMinimal, CanConstruct) {
  TestMetricsCollector metricsCollector;
  application_features::ApplicationServer server{nullptr, nullptr};
  auto& recovery = server.addFeature<RocksDBRecoveryManager>();

  TestRocksDBOptionsProvider optionsProvider;
  TestDatabasePathProvider dbPath;
  TestVectorIndexProvider vectorIdx;
  TestFlushControl flush;
  TestDumpLimitsProvider dumpLimits;
  TestDatabaseProvider dbProvider;
  TestCacheManagerProvider cacheManager;
  TestSortingPolicy sortingPolicy;
  TestIndexCacheRefill indexCacheRefill;

  auto& engine = server.addFeature<RocksDBEngine>(
      optionsProvider, metricsCollector, dbPath, vectorIdx, flush, dumpLimits,
      nullptr /* IReplicatedLogProvider* */, recovery, dbProvider,
      indexCacheRefill, cacheManager, sortingPolicy);

  EXPECT_EQ(engine.kEngineName, "rocksdb");
}
