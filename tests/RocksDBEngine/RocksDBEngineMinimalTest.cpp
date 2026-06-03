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

#include "Agency/AgencyFeature.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/SharedPRNG.h"
#include "Cache/CacheManagerFeature.h"
#include "Cache/CacheOptionsProvider.h"
#include "Metrics/MetricsFeature.h"

// LazyApplicationFeatureReference works with incomplete types
namespace arangodb {
class ClusterFeature;
namespace metrics {
class ClusterMetricsFeature;
}
}  // namespace arangodb
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/DumpLimitsFeature.h"
#include "RestServer/FlushFeature.h"
#include "RestServer/arangod.h"

namespace arangodb {
class QueryRegistryFeature;
}  // namespace arangodb
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBIndexCacheRefillFeature.h"
#include "RocksDBEngine/RocksDBOptionsProvider.h"
#include "RocksDBEngine/RocksDBRecoveryManager.h"
#include "Scheduler/SchedulerFeature.h"
#include "VectorIndex/VectorIndexFeature.h"

namespace arangodb {
class StatisticsFeature;
}  // namespace arangodb

using namespace arangodb;

// inline impl avoids pulling in any extra library for this
struct TestCacheOptionsProvider final : CacheOptionsProvider {
  CacheOptions getOptions() const override { return {}; }
};

// RocksDBOptionsProvider is abstract; this stub returns all defaults
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
  ArangodServer server{nullptr, nullptr};

  // addFeature<T>(args...) injects server as the first ctor arg automatically
  auto& metrics = server.addFeature<metrics::MetricsFeature>(
      LazyApplicationFeatureReference<QueryRegistryFeature>(nullptr),
      LazyApplicationFeatureReference<StatisticsFeature>(nullptr),
      LazyApplicationFeatureReference<DatabaseFeature>(nullptr),
      LazyApplicationFeatureReference<metrics::ClusterMetricsFeature>(nullptr),
      LazyApplicationFeatureReference<ClusterFeature>(nullptr));

  auto& dbPath = server.addFeature<DatabasePathFeature>();
  auto& dbFeature = server.addFeature<DatabaseFeature>();
  auto& dumpLimits = server.addFeature<DumpLimitsFeature>();
  auto& recovery = server.addFeature<RocksDBRecoveryManager>();
  auto& agency = server.addFeature<AgencyFeature>();
  auto& flush = server.addFeature<FlushFeature>(metrics);
  auto& vectorIdx = server.addFeature<VectorIndexFeature>(dbFeature);

  basics::SharedPRNG prng;
  auto& scheduler = server.addFeature<SchedulerFeature>(metrics, prng);

  TestCacheOptionsProvider cacheProvider;
  auto& cache = server.addFeature<CacheManagerFeature>(cacheProvider, prng);

  auto& indexRefill = server.addFeature<RocksDBIndexCacheRefillFeature>(
      dbFeature, nullptr /* ClusterFeature* */, metrics);

  TestRocksDBOptionsProvider optionsProvider;

  auto& engine = server.addFeature<RocksDBEngine>(
      optionsProvider, metrics, dbPath, vectorIdx, flush, dumpLimits, scheduler,
      nullptr /* ReplicatedLogFeature* */, recovery, dbFeature, indexRefill,
      cache, agency);

  EXPECT_EQ(engine.kEngineName, "rocksdb");
}
