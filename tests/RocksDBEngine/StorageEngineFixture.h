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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Cluster/ServerState.h"
#include "Mocks/FakeRegistry.h"
#include "Mocks/FakeScheduler.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/Version.h"
#include "RocksDBEngine/Mocks.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBOptionsProvider.h"
#include "RocksDBEngine/TempDatabasePathProvider.h"
#include "Scheduler/ISchedulerProvider.h"

#include <memory>

namespace arangodb::tests {

struct TestRocksDBOptionsProvider final : RocksDBOptionsProvider {
  rocksdb::TransactionDBOptions getTransactionDBOptions() const override {
    return {};
  }
  uint64_t maxTotalWalSize() const noexcept override { return 0; }
  uint32_t numThreadsHigh() const noexcept override { return 2; }
  uint32_t numThreadsLow() const noexcept override { return 2; }
  uint64_t periodicCompactionTtl() const noexcept override { return 0; }

 protected:
  rocksdb::Options doGetOptions() const override {
    rocksdb::Options options;
    options.create_if_missing = true;
    options.create_missing_column_families = true;
    return options;
  }
  rocksdb::BlockBasedTableOptions doGetTableOptions() const override {
    return {};
  }
};

struct TestSchedulerProvider final : ISchedulerProvider {
  explicit TestSchedulerProvider(Scheduler& scheduler)
      : _scheduler(scheduler) {}
  Scheduler* scheduler() const noexcept override { return &_scheduler; }
  Scheduler& _scheduler;
};

struct StorageEngineFixtureSuite {
  metrics::FakeRegistry metricsRegistry;
  application_features::ApplicationServer server{nullptr, nullptr};
  ScopedServerStateReset serverStateReset;
  ServerState serverState{server};
  TempDatabasePathProvider dbPath;
  TestRocksDBOptionsProvider optionsProvider;
  DumpLimitsFeatureOptions limitsOptions{};
  std::shared_ptr<replication2::ReplicatedLogGlobalSettings const> logSettings =
      std::make_shared<replication2::ReplicatedLogGlobalSettings>();

  ::testing::NiceMock<MockVectorIndexProvider> vectorIdx;
  ::testing::NiceMock<MockFlushControl> flush;
  ::testing::NiceMock<MockDumpLimitsProvider> dumpLimits;
  ::testing::NiceMock<MockDatabaseProvider> dbProvider;
  ::testing::NiceMock<MockCacheManagerProvider> cacheManager;
  ::testing::NiceMock<MockSortingPolicy> sortingPolicy;
  ::testing::NiceMock<MockIndexCacheRefill> indexCacheRefill;
  ::testing::NiceMock<MockReplicatedLogProvider> logProvider;

  FakeScheduler scheduler{server};
  TestSchedulerProvider schedulerProvider{scheduler};

  RocksDBEngine engine{server,       optionsProvider,  metricsRegistry,
                       dbPath,       vectorIdx,        flush,
                       dumpLimits,   &logProvider,     schedulerProvider,
                       dbProvider,   indexCacheRefill, cacheManager,
                       sortingPolicy};
};

// Fixture providing a preconfigured RocksDBEngine backed by a temporary
// directory. The engine is started once in SetUpTestSuite() and ready to use
// for all tests in the suite. Each test gets a fresh collection via SetUp().
// All collaborators are owned by the fixture suite and torn down (including
// the on-disk data) when all tests in the class finish.
class StorageEngineFixture : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    _suite = std::make_unique<StorageEngineFixtureSuite>();
    _suite->serverState.setRole(ServerState::ROLE_SINGLE);

    using ::testing::Return;
    using ::testing::ReturnRef;
    ON_CALL(_suite->dumpLimits, limits())
        .WillByDefault(ReturnRef(_suite->limitsOptions));
    ON_CALL(_suite->flush, isEnabled()).WillByDefault(Return(true));
    ON_CALL(_suite->logProvider, options())
        .WillByDefault(Return(_suite->logSettings));
    ON_CALL(_suite->dbProvider, defaultReplicationVersion())
        .WillByDefault(Return(replication::Version::ONE));

    _suite->engine.start();
  }

  static void TearDownTestSuite() {
    _suite->server.beginShutdown();
    _suite->engine.beginShutdown();
    _suite->engine.stop();
    _suite->engine.unprepare();
    _suite.reset();
  }

  RocksDBEngine& engine() noexcept { return _suite->engine; }

  static std::unique_ptr<StorageEngineFixtureSuite> _suite;
};

}  // namespace arangodb::tests
