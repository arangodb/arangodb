////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_CLUSTER_ENGINE_CLUSTER_ENGINE_H
#define ARANGOD_CLUSTER_ENGINE_CLUSTER_ENGINE_H 1

#include "Basics/Common.h"
#include "Basics/Mutex.h"
#include "Basics/StaticStrings.h"
#include "ClusterEngine/Common.h"
#include "StorageEngine/StorageEngine.h"
#include "VocBase/AccessMode.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

namespace arangodb {
class PhysicalCollection;
class PhysicalView;
class TransactionCollection;
class TransactionState;

namespace rest {
class RestHandlerFactory;
}

namespace transaction {
class ContextData;
struct Options;
}

class ClusterEngine final : public StorageEngine {
 public:
  // create the storage engine
  explicit ClusterEngine(application_features::ApplicationServer& server);
  ~ClusterEngine();

  void setActualEngine(StorageEngine* e) { _actualEngine = e; }
  StorageEngine* actualEngine() const { return _actualEngine; }
  bool isRocksDB() const;
  bool isMMFiles() const;
  bool isMock() const;
  ClusterEngineType engineType() const;

  // storage engine overrides
  // ------------------------

  std::string const& typeName() const override {
    return _actualEngine ? _actualEngine->typeName() : StaticStrings::Empty;
  }

  // inherited from ApplicationFeature
  // ---------------------------------

  // preparation phase for storage engine. can be used for internal setup.
  // the storage engine must not start any threads here or write any files
  void prepare() override;
  void start() override;

  // minimum timeout for the synchronous replication
  double minimumSyncReplicationTimeout() const override { return 1.0; }

  bool supportsDfdb() const override { return false; }
  bool useRawDocumentPointers() override { return false; }

  std::unique_ptr<TransactionManager> createTransactionManager() override;
  std::unique_ptr<transaction::ContextData> createTransactionContextData() override;
  std::unique_ptr<TransactionState> createTransactionState(
    TRI_vocbase_t& vocbase,
    transaction::Options const& options
  ) override;
  std::unique_ptr<TransactionCollection> createTransactionCollection(
    TransactionState& state,
    TRI_voc_cid_t cid,
    AccessMode::Type accessType,
    int nestingLevel
  ) override;

  // create storage-engine specific collection
  std::unique_ptr<PhysicalCollection> createPhysicalCollection(
    LogicalCollection& collection,
    velocypack::Slice const& info
  ) override;

  void getStatistics(velocypack::Builder& builder) const override;

  // inventory functionality
  // -----------------------

  void getDatabases(arangodb::velocypack::Builder& result) override;

  void getCollectionInfo(
    TRI_vocbase_t& vocbase,
    TRI_voc_cid_t cid,
    arangodb::velocypack::Builder& result,
    bool includeIndexes,
    TRI_voc_tick_t maxTick
  ) override;

  int getCollectionsAndIndexes(
    TRI_vocbase_t& vocbase,
    arangodb::velocypack::Builder& result,
    bool wasCleanShutdown,
    bool isUpgrade
  ) override;

  int getViews(
    TRI_vocbase_t& vocbase,
    arangodb::velocypack::Builder& result
  ) override;

  std::string versionFilename(TRI_voc_tick_t id) const override {
    // the cluster engine does not have any versioning information
    return std::string();
  }
  std::string databasePath(TRI_vocbase_t const* vocbase) const override {
    // the cluster engine does not have any database path
    return std::string();
  }
  std::string collectionPath(
      TRI_vocbase_t const& vocbase,
      TRI_voc_cid_t id
  ) const override {
    return std::string(); // no path to be returned here
  }

  velocypack::Builder getReplicationApplierConfiguration(
    TRI_vocbase_t& vocbase,
    int& status
  ) override;
  velocypack::Builder getReplicationApplierConfiguration(int& status) override;
  int removeReplicationApplierConfiguration(TRI_vocbase_t& vocbase) override {
    return TRI_ERROR_NOT_IMPLEMENTED;
  }
  int removeReplicationApplierConfiguration() override {
    return TRI_ERROR_NOT_IMPLEMENTED;
  }
  int saveReplicationApplierConfiguration(
      TRI_vocbase_t& vocbase,
      velocypack::Slice slice,
      bool doSync
  ) override {
    return TRI_ERROR_NOT_IMPLEMENTED;
  }
  int saveReplicationApplierConfiguration(arangodb::velocypack::Slice slice,
                                          bool doSync) override {
    return TRI_ERROR_NOT_IMPLEMENTED;
  }
  Result handleSyncKeys(
      DatabaseInitialSyncer& syncer,
      LogicalCollection& col,
      std::string const& keysId
  ) override {
    return TRI_ERROR_NOT_IMPLEMENTED;
  }
  Result createLoggerState(TRI_vocbase_t* vocbase,
                           velocypack::Builder& builder) override {
    return TRI_ERROR_NOT_IMPLEMENTED;
  }
  Result createTickRanges(velocypack::Builder& builder) override {
    return TRI_ERROR_NOT_IMPLEMENTED;
  }
  Result firstTick(uint64_t& tick) override {
    return TRI_ERROR_NOT_IMPLEMENTED;
  }
  Result lastLogger(
      TRI_vocbase_t& vocbase,
      std::shared_ptr<transaction::Context> transactionContext,
      uint64_t tickStart,
      uint64_t tickEnd,
      std::shared_ptr<velocypack::Builder>& builderSPtr
  ) override {
    return TRI_ERROR_NOT_IMPLEMENTED;
  }
  WalAccess const* walAccess() const override {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
    return nullptr;
  }

  // database, collection and index management
  // -----------------------------------------

  // intentionally empty, not useful for this type of engine
  void waitForSyncTick(TRI_voc_tick_t) override {}
  
  /// @brief return a list of the currently open WAL files
  std::vector<std::string> currentWalFiles() const override { return std::vector<std::string>(); }

  Result flushWal(bool waitForSync, bool waitForCollector,
                  bool writeShutdownFile) override {
    return TRI_ERROR_NO_ERROR;
  }
  void waitForEstimatorSync(std::chrono::milliseconds maxWaitTime) override;

  virtual std::unique_ptr<TRI_vocbase_t> openDatabase(
    velocypack::Slice const& parameters,
    bool isUpgrade,
    int& status
  ) override;
  std::unique_ptr<TRI_vocbase_t> createDatabase(
    TRI_voc_tick_t id,
    velocypack::Slice const& args,
    int& status
  ) override;
  int writeCreateDatabaseMarker(TRI_voc_tick_t id,
                                velocypack::Slice const& slice) override;
  void prepareDropDatabase(
    TRI_vocbase_t& vocbase,
    bool useWriteMarker,
    int& status
  ) override;
  Result dropDatabase(TRI_vocbase_t& database) override;
  void waitUntilDeletion(TRI_voc_tick_t id, bool force, int& status) override;

  // wal in recovery
  bool inRecovery() override;
  // start compactor thread and delete files form collections marked as deleted
  void recoveryDone(TRI_vocbase_t& vocbase) override;

 public:
  std::string createCollection(
    TRI_vocbase_t& vocbase,
    TRI_voc_cid_t id,
    LogicalCollection const& collection
  ) override;

  arangodb::Result persistCollection(
    TRI_vocbase_t& vocbase,
    LogicalCollection const& collection
  ) override;

  arangodb::Result dropCollection(
    TRI_vocbase_t& vocbase,
    LogicalCollection& collection
  ) override;

  void destroyCollection(
    TRI_vocbase_t& vocbase,
    LogicalCollection& collection
  ) override;

  void changeCollection(
    TRI_vocbase_t& vocbase,
    TRI_voc_cid_t id,
    LogicalCollection const& collection,
    bool doSync
  ) override;

  arangodb::Result renameCollection(
    TRI_vocbase_t& vocbase,
    LogicalCollection const& collection,
    std::string const& oldName
  ) override;

  void unloadCollection(
    TRI_vocbase_t& vocbase,
    LogicalCollection& collection
  ) override;

  arangodb::Result changeView(
    TRI_vocbase_t& vocbase,
    arangodb::LogicalView const& view,
    bool doSync
  ) override;

  arangodb::Result createView(
    TRI_vocbase_t& vocbase,
    TRI_voc_cid_t id,
    arangodb::LogicalView const& view
  ) override;

  virtual void getViewProperties(
     TRI_vocbase_t& /*vocbase*/,
     LogicalView const& /*view*/,
     VPackBuilder& /*builder*/
  ) override {
    // does nothing
  }

  arangodb::Result dropView(
    TRI_vocbase_t& vocbase,
    LogicalView& view
  ) override;

  void destroyView(
    TRI_vocbase_t& vocbase,
    LogicalView& view
  ) noexcept override;

  void signalCleanup(TRI_vocbase_t& vocbase) override;

  int shutdownDatabase(TRI_vocbase_t& vocbase) override;

  /// @brief Add engine-specific optimizer rules
  void addOptimizerRules() override;

  /// @brief Add engine-specific V8 functions
  void addV8Functions() override;

  /// @brief Add engine-specific REST handlers
  void addRestHandlers(rest::RestHandlerFactory& handlerFactory) override;

  void addParametersForNewCollection(arangodb::velocypack::Builder& builder,
                                     arangodb::velocypack::Slice info) override;

  // management methods for synchronizing with external persistent stores
  TRI_voc_tick_t currentTick() const override {
    return 0;
  }
  TRI_voc_tick_t releasedTick() const override {
    return 0;
  }
  void releaseTick(TRI_voc_tick_t) override {
    // noop
  }

 private:

  /// @brief open an existing database. internal function
  std::unique_ptr<TRI_vocbase_t> openExistingDatabase(
    TRI_voc_tick_t id,
    std::string const& name,
    bool wasCleanShutdown,
    bool isUpgrade
  );

 public:
  static std::string const EngineName;
  static std::string const FeatureName;

  // mock mode
  static bool Mocking;

 private:
  /// path to arangodb data dir
  std::string _basePath;
  StorageEngine* _actualEngine;
};

}  // namespace arangodb

#endif
