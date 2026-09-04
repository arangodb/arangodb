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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "ClusterEngine/Common.h"
#include "Indexes/IndexDefinitionRegistry.h"
#include "Metrics/IRegistry.h"
#include "StorageEngine/StorageEngine.h"
#include "VectorIndex/IVectorIndexProvider.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

namespace arangodb {

class ClusterFeature;
class DatabaseFeature;

class ClusterEngine final : public StorageEngine {
 public:
  static constexpr std::string_view name() noexcept { return "ClusterEngine"; }

  // create the storage engine
  explicit ClusterEngine(application_features::ApplicationServer& server,
                         ClusterFeature& clusterFeature,
                         DatabaseFeature& database, metrics::IRegistry& metrics,
                         IVectorIndexProvider const& vectorIndexProvider);
  ~ClusterEngine();

  ClusterEngineType engineType() const;

  // equal()/normalize() only, no RocksDB dependency
  IndexDefinitionRegistry const& indexDefinitions() const {
    return *_indexDefinitions;
  }

  // storage engine overrides
  // ------------------------

  std::string_view typeName() const override;

  // inherited from ApplicationFeature
  // ---------------------------------

  // preparation phase for storage engine. can be used for internal setup.
  // the storage engine must not start any threads here or write any files
  void prepare() override;
  void start() override;

  HealthData healthCheck() override;

  std::shared_ptr<TransactionState> createTransactionState(
      TRI_vocbase_t& vocbase, TransactionId tid,
      transaction::Options const& options,
      transaction::OperationOrigin operationOrigin) override;

  // create storage-engine specific collection
  std::unique_ptr<PhysicalCollection> createPhysicalCollection(
      LogicalCollection& collection, velocypack::Slice info) override;

  void getStatistics(velocypack::Builder& builder) const override;

  // inventory functionality
  // -----------------------

  void getDatabases(arangodb::velocypack::Builder& result) override;

  void getCollectionInfo(TRI_vocbase_t& vocbase, DataSourceId cid,
                         arangodb::velocypack::Builder& result,
                         bool includeIndexes, TRI_voc_tick_t maxTick) override;

  ErrorCode getCollectionsAndIndexes(TRI_vocbase_t& vocbase,
                                     arangodb::velocypack::Builder& result,
                                     bool wasCleanShutdown,
                                     bool isUpgrade) override;

  ErrorCode getViews(TRI_vocbase_t& vocbase,
                     arangodb::velocypack::Builder& result) override;

  std::string versionFilename(TRI_voc_tick_t id) const override {
    // the cluster engine does not have any versioning information
    return std::string();
  }

  void cleanupReplicationContexts() override {}

  Result handleSyncKeys(DatabaseInitialSyncer& syncer, LogicalCollection& col,
                        std::string const& keysId) override {
    return {TRI_ERROR_NOT_IMPLEMENTED};
  }
  Result createLoggerState(TRI_vocbase_t* vocbase,
                           velocypack::Builder& builder) override {
    return {TRI_ERROR_NOT_IMPLEMENTED};
  }

  WalAccess const* walAccess() const override {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
    return nullptr;
  }

  // database, collection and index management
  // -----------------------------------------

  /// @brief return a list of the currently open WAL files
  std::vector<std::string> currentWalFiles() const override {
    return std::vector<std::string>();
  }

  Result flushWal(bool /*waitForSync*/ = false,
                  bool /*flushColumnFamilies*/ = false) override {
    return {};
  }

  void waitForEstimatorSync() override;

  virtual std::unique_ptr<TRI_vocbase_t> openDatabase(
      arangodb::CreateDatabaseInfo&& info, bool isUpgrade) override;
  Result dropDatabase(TRI_vocbase_t& database) override;

  // current recovery state
  RecoveryState recoveryState() override;
  // current recovery tick
  TRI_voc_tick_t recoveryTick() override;

  void createCollection(TRI_vocbase_t& vocbase,
                        LogicalCollection const& collection) override;

  arangodb::Result dropCollection(TRI_vocbase_t& vocbase,
                                  LogicalCollection& collection) override;

  void changeCollection(TRI_vocbase_t& vocbase,
                        LogicalCollection const& collection) override;

  arangodb::Result renameCollection(TRI_vocbase_t& vocbase,
                                    LogicalCollection const& collection,
                                    std::string const& oldName) override;

  Result changeView(LogicalView const& view, velocypack::Slice update) final;

  arangodb::Result createView(TRI_vocbase_t& vocbase, DataSourceId id,
                              arangodb::LogicalView const& view) override;

  arangodb::Result dropView(TRI_vocbase_t const& vocbase,
                            LogicalView const& view) override;

  arangodb::Result compactAll(bool changeLevel,
                              bool compactBottomMostLevel) override;

  auto dropReplicatedState(
      TRI_vocbase_t& vocbase,
      std::unique_ptr<replication2::storage::IStorageEngineMethods>& ptr)
      -> Result override;
  auto createReplicatedState(
      TRI_vocbase_t& vocbase, arangodb::replication2::LogId id,
      const replication2::storage::PersistedStateInfo& info)
      -> ResultT<std::unique_ptr<
          replication2::storage::IStorageEngineMethods>> override;

#ifdef USE_V8
  /// @brief Add engine-specific V8 functions
  void addV8Functions() override;
#endif

  void addParametersForNewCollection(arangodb::velocypack::Builder& builder,
                                     arangodb::velocypack::Slice info) override;

  // management methods for synchronizing with external persistent stores
  TRI_voc_tick_t currentTick() const override { return 0; }
  TRI_voc_tick_t releasedTick() const override { return 0; }
  void releaseTick(TRI_voc_tick_t) override {
    // noop
  }

  bool autoRefillIndexCaches() const override { return false; }
  bool autoRefillIndexCachesOnFollowers() const override { return false; }

  std::shared_ptr<StorageSnapshot> currentSnapshot() final { return nullptr; }

 public:
  static std::string const EngineName;
  static std::string const FeatureName;

  // mock mode
#ifdef ARANGODB_USE_GOOGLE_TESTS
  static bool Mocking;
#else
  static constexpr bool Mocking = false;
#endif

 private:
  ClusterFeature& _clusterFeature;
  metrics::IRegistry& _metrics;
  /// path to arangodb data dir
  std::string _basePath;
  std::unique_ptr<IndexDefinitionRegistry> _indexDefinitions;
};

}  // namespace arangodb
