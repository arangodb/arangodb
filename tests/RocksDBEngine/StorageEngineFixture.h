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
#include "RestServer/IRecoveryCallback.h"
#include "RocksDBEngine/Mocks.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBOptionsProvider.h"
#include "RocksDBEngine/RocksDBRecoveryManager.h"
#include "RocksDBEngine/TempDatabasePathProvider.h"
#include "Scheduler/ISchedulerProvider.h"

#include <memory>

namespace arangodb::tests {

struct TestRocksDBOptionsProvider final : RocksDBOptionsProvider {
  explicit TestRocksDBOptionsProvider(bool timeTravel = false)
      : _timeTravel(timeTravel) {}

  rocksdb::TransactionDBOptions getTransactionDBOptions() const override {
    return {};
  }
  uint64_t maxTotalWalSize() const noexcept override { return 0; }
  uint32_t numThreadsHigh() const noexcept override { return 2; }
  uint32_t numThreadsLow() const noexcept override { return 2; }
  uint64_t periodicCompactionTtl() const noexcept override { return 0; }
  bool timeTravelEnabled() const noexcept override { return _timeTravel; }

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

 private:
  bool _timeTravel;
};

struct NullRecoveryCallback final : IRecoveryCallback {
  void recoveryDone() override {}
};

struct TestSchedulerProvider final : ISchedulerProvider {
  explicit TestSchedulerProvider(Scheduler& scheduler)
      : _scheduler(scheduler) {}
  Scheduler* scheduler() const noexcept override { return &_scheduler; }
  Scheduler& _scheduler;
};

struct StorageEngineFixtureSuite {
  // `timeTravel` enables the PrimaryIndex_TT column family on the engine; off
  // by default so existing suites keep exercising the non-time-travel paths.
  explicit StorageEngineFixtureSuite(bool timeTravel = false)
      : optionsProvider(timeTravel) {}
  ~StorageEngineFixtureSuite();

  metrics::FakeRegistry metricsRegistry;
  application_features::ApplicationServer server{nullptr, nullptr};
  ScopedServerStateReset serverStateReset;
  ServerState serverState{server};
  TempDatabasePathProvider dbPath;
  TestRocksDBOptionsProvider optionsProvider;
  DumpLimitsFeatureOptions limitsOptions{};
  std::shared_ptr<replication2::ReplicatedLogGlobalSettings const> logSettings =
      std::make_shared<replication2::ReplicatedLogGlobalSettings>();

  ::testing::NiceMock<MockFlushControl> flush;
  ::testing::NiceMock<MockDumpLimitsProvider> dumpLimits;
  ::testing::NiceMock<MockDatabaseProvider> dbProvider;
  ::testing::NiceMock<MockCacheManagerProvider> cacheManager;
  ::testing::NiceMock<MockSortingPolicy> sortingPolicy;
  ::testing::NiceMock<MockIndexCacheRefill> indexCacheRefill;
  ::testing::NiceMock<MockReplicatedLogProvider> logProvider;

  NullRecoveryCallback nullCallback;
  RocksDBRecoveryManager recoveryManager{server, dbProvider, nullCallback};

  FakeScheduler scheduler{server};
  TestSchedulerProvider schedulerProvider{scheduler};

  RocksDBEngine engine{
      server,          optionsProvider, metricsRegistry,  dbPath,
      flush,           dumpLimits,      &logProvider,     schedulerProvider,
      recoveryManager, dbProvider,      indexCacheRefill, cacheManager,
      sortingPolicy};
};

// Build a suite, wire up the collaborator mocks the engine needs at startup,
// and start the engine. Shared by the plain and time-travel fixtures.
std::unique_ptr<StorageEngineFixtureSuite> makeStartedSuite(
    bool timeTravel = false);

// Shut down and destroy a suite created by makeStartedSuite().
void stopSuite(std::unique_ptr<StorageEngineFixtureSuite>& suite);

// Fixture providing a preconfigured RocksDBEngine backed by a temporary
// directory. The engine is started once in SetUpTestSuite() and ready to use
// for all tests in the suite. Each test gets a fresh collection via SetUp().
// All collaborators are owned by the fixture suite and torn down (including
// the on-disk data) when all tests in the class finish.
template<class Derived>
class BasicStorageEngineFixture : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    _suite = makeStartedSuite(Derived::timeTravelEnabled);
  }

  static void TearDownTestSuite() { stopSuite(_suite); }

  RocksDBEngine& engine() noexcept { return _suite->engine; }

  inline static std::unique_ptr<StorageEngineFixtureSuite> _suite;
};

struct StorageEngineFixture : BasicStorageEngineFixture<StorageEngineFixture> {
  static constexpr bool timeTravelEnabled = false;
};

// Like StorageEngineFixture, but with the time-travel feature enabled
struct TimeTravelStorageEngineFixture
    : BasicStorageEngineFixture<TimeTravelStorageEngineFixture> {
  static constexpr bool timeTravelEnabled = true;
};

}  // namespace arangodb::tests
