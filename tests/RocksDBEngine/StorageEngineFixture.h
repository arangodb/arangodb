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

// Fixture providing a preconfigured RocksDBEngine backed by a temporary
// directory. The engine is started in SetUp() and ready to use; all
// collaborators are owned by the fixture and torn down (including the on-disk
// data) when the test ends.
// Adapts a Scheduler to the ISchedulerProvider port the engine expects.
struct TestSchedulerProvider final : ISchedulerProvider {
  explicit TestSchedulerProvider(Scheduler& scheduler)
      : _scheduler(scheduler) {}
  Scheduler* scheduler() const noexcept override { return &_scheduler; }
  Scheduler& _scheduler;
};

class StorageEngineFixture : public ::testing::Test {
 protected:
  void SetUp() override {
    _serverState.setRole(ServerState::ROLE_SINGLE);

    using ::testing::Return;
    using ::testing::ReturnRef;
    ON_CALL(_dumpLimits, limits()).WillByDefault(ReturnRef(_limitsOptions));
    // single-server/DB-server engines require the released-tick mechanism
    ON_CALL(_flush, isEnabled()).WillByDefault(Return(true));
    ON_CALL(_logProvider, options()).WillByDefault(Return(_logSettings));
    ON_CALL(_dbProvider, defaultReplicationVersion())
        .WillByDefault(Return(replication::Version::ONE));

    _engine.start();
  }

  void TearDown() override {
    // The engine's background threads assert that the server is stopping when
    // they are shut down, so move the server into a stopping state first.
    _server.beginShutdown();
    // Then run the ApplicationFeature shutdown lifecycle so the background
    // threads are stopped before the RocksDB instance (and the engine) are
    // destroyed.
    _engine.beginShutdown();
    _engine.stop();
    _engine.unprepare();
  }

  RocksDBEngine& engine() noexcept { return _engine; }

  metrics::FakeRegistry _metricsRegistry;
  application_features::ApplicationServer _server{nullptr, nullptr};
  // ServerState is a process-wide singleton. The reset guard detaches any
  // existing instance (e.g. the one installed by tests/main.cpp in the combined
  // arangodbtests binary) so this fixture can own its own ServerState bound to
  // _server. Declaration order matters: the guard must precede _serverState.
  ScopedServerStateReset _serverStateReset;
  ServerState _serverState{_server};
  TempDatabasePathProvider _dbPath;
  TestRocksDBOptionsProvider _optionsProvider;
  DumpLimitsFeatureOptions _limitsOptions{};
  std::shared_ptr<replication2::ReplicatedLogGlobalSettings const>
      _logSettings =
          std::make_shared<replication2::ReplicatedLogGlobalSettings>();

  ::testing::NiceMock<MockVectorIndexProvider> _vectorIdx;
  ::testing::NiceMock<MockFlushControl> _flush;
  ::testing::NiceMock<MockDumpLimitsProvider> _dumpLimits;
  ::testing::NiceMock<MockDatabaseProvider> _dbProvider;
  ::testing::NiceMock<MockCacheManagerProvider> _cacheManager;
  ::testing::NiceMock<MockSortingPolicy> _sortingPolicy;
  ::testing::NiceMock<MockIndexCacheRefill> _indexCacheRefill;
  ::testing::NiceMock<MockReplicatedLogProvider> _logProvider;

  FakeScheduler _scheduler{_server};
  TestSchedulerProvider _schedulerProvider{_scheduler};

  RocksDBEngine _engine{_server,       _optionsProvider,  _metricsRegistry,
                        _dbPath,       _vectorIdx,        _flush,
                        _dumpLimits,   &_logProvider,     _schedulerProvider,
                        _dbProvider,   _indexCacheRefill, _cacheManager,
                        _sortingPolicy};
};

}  // namespace arangodb::tests
