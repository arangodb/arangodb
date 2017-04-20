////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_ROCKSDB_ENGINE_ROCKSDB_ENGINE_H
#define ARANGOD_ROCKSDB_ENGINE_ROCKSDB_ENGINE_H 1

#include "Basics/Common.h"
#include "Basics/Mutex.h"
#include "RocksDBEngine/RocksDBTypes.h"
#include "StorageEngine/StorageEngine.h"
#include "VocBase/AccessMode.h"

#include <rocksdb/db.h>
#include <rocksdb/utilities/transaction_db.h>
#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

namespace arangodb {
class PhysicalCollection;
class PhysicalView;
class RocksDBComparator;
class RocksDBCounterManager;
class RocksDBReplicationManager;
class TransactionCollection;
class TransactionState;

namespace rest {
class RestHandlerFactory;
}

namespace transaction {
class ContextData;
}

class RocksDBEngine final : public StorageEngine {
 public:
  // create the storage engine
  explicit RocksDBEngine(application_features::ApplicationServer*);
  ~RocksDBEngine();

  // inherited from ApplicationFeature
  // ---------------------------------

  // add the storage engine's specifc options to the global list of options
  void collectOptions(std::shared_ptr<options::ProgramOptions>) override;
  // validate the storage engine's specific options
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override;

  void start() override;
  void stop() override;
  // preparation phase for storage engine. can be used for internal setup.
  // the storage engine must not start any threads here or write any files
  void prepare() override;
  void unprepare() override;

  transaction::ContextData* createTransactionContextData() override;
  TransactionState* createTransactionState(TRI_vocbase_t*) override;
  TransactionCollection* createTransactionCollection(
      TransactionState* state, TRI_voc_cid_t cid, AccessMode::Type accessType,
      int nestingLevel) override;

  // create storage-engine specific collection
  PhysicalCollection* createPhysicalCollection(LogicalCollection*,
                                               VPackSlice const&) override;

  // create storage-engine specific view
  PhysicalView* createPhysicalView(LogicalView*, VPackSlice const&) override;

  // inventory functionality
  // -----------------------

  void getDatabases(arangodb::velocypack::Builder& result) override;
  void getCollectionInfo(TRI_vocbase_t* vocbase, TRI_voc_cid_t cid,
                         arangodb::velocypack::Builder& result,
                         bool includeIndexes, TRI_voc_tick_t maxTick) override;
  int getCollectionsAndIndexes(TRI_vocbase_t* vocbase,
                               arangodb::velocypack::Builder& result,
                               bool wasCleanShutdown, bool isUpgrade) override;

  int getViews(TRI_vocbase_t* vocbase,
               arangodb::velocypack::Builder& result) override;

  std::string versionFilename(TRI_voc_tick_t id) const override;
  std::string databasePath(TRI_vocbase_t const* vocbase) const override;
  std::string collectionPath(TRI_vocbase_t const* vocbase,
                             TRI_voc_cid_t id) const override;

  std::shared_ptr<arangodb::velocypack::Builder>
  getReplicationApplierConfiguration(TRI_vocbase_t* vocbase,
                                     int& status) override;
  int removeReplicationApplierConfiguration(TRI_vocbase_t* vocbase) override;
  int saveReplicationApplierConfiguration(TRI_vocbase_t* vocbase,
                                          arangodb::velocypack::Slice slice,
                                          bool doSync) override;

  // database, collection and index management
  // -----------------------------------------

  void waitForSync(TRI_voc_tick_t tick) override;

  virtual TRI_vocbase_t* openDatabase(
      arangodb::velocypack::Slice const& parameters, bool isUpgrade,
      int&) override;
  Database* createDatabase(TRI_voc_tick_t id,
                           arangodb::velocypack::Slice const& args,
                           int& status) override;
  int writeCreateDatabaseMarker(TRI_voc_tick_t id,
                                VPackSlice const& slice) override;
  void prepareDropDatabase(TRI_vocbase_t* vocbase, bool useWriteMarker,
                           int& status) override;
  Result dropDatabase(Database* database) override;
  void waitUntilDeletion(TRI_voc_tick_t id, bool force, int& status) override;

  // wal in recovery
  bool inRecovery() override;
  // start compactor thread and delete files form collections marked as deleted
  void recoveryDone(TRI_vocbase_t* vocbase) override;

 public:
  std::string createCollection(TRI_vocbase_t* vocbase, TRI_voc_cid_t id,
                               arangodb::LogicalCollection const*) override;

  arangodb::Result persistCollection(
      TRI_vocbase_t* vocbase, arangodb::LogicalCollection const*) override;
  arangodb::Result dropCollection(TRI_vocbase_t* vocbase,
                                  arangodb::LogicalCollection*) override;
  void destroyCollection(TRI_vocbase_t* vocbase,
                         arangodb::LogicalCollection*) override;
  void changeCollection(TRI_vocbase_t* vocbase, TRI_voc_cid_t id,
                        arangodb::LogicalCollection const*,
                        bool doSync) override;
  arangodb::Result renameCollection(TRI_vocbase_t* vocbase,
                                    arangodb::LogicalCollection const*,
                                    std::string const& oldName) override;
  void createIndex(TRI_vocbase_t* vocbase, TRI_voc_cid_t collectionId,
                   TRI_idx_iid_t id,
                   arangodb::velocypack::Slice const& data) override;
  void dropIndex(TRI_vocbase_t* vocbase, TRI_voc_cid_t collectionId,
                 TRI_idx_iid_t id) override;
  void dropIndexWalMarker(TRI_vocbase_t* vocbase, TRI_voc_cid_t collectionId,
                          arangodb::velocypack::Slice const& data,
                          bool writeMarker, int&) override;
  void unloadCollection(TRI_vocbase_t* vocbase,
                        arangodb::LogicalCollection* collection) override;
  void createView(TRI_vocbase_t* vocbase, TRI_voc_cid_t id,
                  arangodb::LogicalView const*) override;

  arangodb::Result persistView(TRI_vocbase_t* vocbase,
                               arangodb::LogicalView const*) override;

  arangodb::Result dropView(TRI_vocbase_t* vocbase,
                            arangodb::LogicalView*) override;
  void destroyView(TRI_vocbase_t* vocbase, arangodb::LogicalView*) override;
  void changeView(TRI_vocbase_t* vocbase, TRI_voc_cid_t id,
                  arangodb::LogicalView const*, bool doSync) override;
  void signalCleanup(TRI_vocbase_t* vocbase) override;

  // document operations
  // -------------------
  void iterateDocuments(
      TRI_voc_tick_t databaseId, TRI_voc_cid_t collectionId,
      std::function<void(arangodb::velocypack::Slice const&)> const& cb)
      override;
  void addDocumentRevision(
      TRI_voc_tick_t databaseId, TRI_voc_cid_t collectionId,
      arangodb::velocypack::Slice const& document) override;
  void removeDocumentRevision(
      TRI_voc_tick_t databaseId, TRI_voc_cid_t collectionId,
      arangodb::velocypack::Slice const& document) override;

  /// @brief remove data of expired compaction blockers
  bool cleanupCompactionBlockers(TRI_vocbase_t* vocbase) override;

  /// @brief insert a compaction blocker
  int insertCompactionBlocker(TRI_vocbase_t* vocbase, double ttl,
                              TRI_voc_tick_t& id) override;

  /// @brief touch an existing compaction blocker
  int extendCompactionBlocker(TRI_vocbase_t* vocbase, TRI_voc_tick_t id,
                              double ttl) override;

  /// @brief remove an existing compaction blocker
  int removeCompactionBlocker(TRI_vocbase_t* vocbase,
                              TRI_voc_tick_t id) override;

  /// @brief a callback function that is run while it is guaranteed that there
  /// is no compaction ongoing
  void preventCompaction(
      TRI_vocbase_t* vocbase,
      std::function<void(TRI_vocbase_t*)> const& callback) override;

  /// @brief a callback function that is run there is no compaction ongoing
  bool tryPreventCompaction(TRI_vocbase_t* vocbase,
                            std::function<void(TRI_vocbase_t*)> const& callback,
                            bool checkForActiveBlockers) override;

  int shutdownDatabase(TRI_vocbase_t* vocbase) override;

  int openCollection(TRI_vocbase_t* vocbase, LogicalCollection* collection,
                     bool ignoreErrors) override;

  /// @brief Add engine-specific AQL functions.
  void addAqlFunctions() override;

  /// @brief Add engine-specific optimizer rules
  void addOptimizerRules() override;

  /// @brief Add engine-specific V8 functions
  void addV8Functions() override;

  /// @brief Add engine-specific REST handlers
  void addRestHandlers(rest::RestHandlerFactory*) override;

  void addParametersForNewCollection(arangodb::velocypack::Builder& builder,
                                     arangodb::velocypack::Slice info) override;
  void addParametersForNewIndex(arangodb::velocypack::Builder& builder,
                                arangodb::velocypack::Slice info) override;

  rocksdb::TransactionDB* db() const { return _db; }

  RocksDBComparator* cmp() const { return _cmp.get(); }

  int writeCreateCollectionMarker(TRI_voc_tick_t databaseId, TRI_voc_cid_t id,
                                  VPackSlice const& slice);

  void addCollectionMapping(uint64_t, TRI_voc_tick_t, TRI_voc_cid_t);
  std::pair<TRI_voc_tick_t, TRI_voc_cid_t> mapObjectToCollection(uint64_t);

 private:
  Result dropDatabase(TRI_voc_tick_t);
  bool systemDatabaseExists();
  void addSystemDatabase();
  /// @brief open an existing database. internal function
  TRI_vocbase_t* openExistingDatabase(TRI_voc_tick_t id,
                                      std::string const& name,
                                      bool wasCleanShutdown, bool isUpgrade);

 public:
  static std::string const EngineName;
  static std::string const FeatureName;
  RocksDBCounterManager* counterManager();
  RocksDBReplicationManager* replicationManager();

 private:
  rocksdb::TransactionDB*
      _db;  // single rocksdb database used in this storage engine
  rocksdb::Options _options;  // default read options
  RocksDBReplicationManager* _replicationManager;
  std::unique_ptr<RocksDBComparator>
      _cmp;           // arangodb comparator - requried because of vpack in keys
  std::string _path;  // path used by rocksdb (inside _basePath)
  std::string _basePath;  // path to arangodb data dir

  std::unique_ptr<RocksDBCounterManager>
      _counterManager;           // tracks the count of documents in collections
  uint64_t _maxTransactionSize;  // maximum allowed size for a transaction
  uint64_t _intermediateTransactionCommitSize;   // maximum size for a
                                                 // transaction before a
                                                 // intermediate commit will be
                                                 // tried
  uint64_t _intermediateTransactionCommitCount;  // limit of transaction count
                                                 // for intermediate commit
  bool _intermediateTransactionCommitEnabled;    // allow usage of intermediate
                                                 // commits

  std::unordered_map<uint64_t, std::pair<TRI_voc_tick_t, TRI_voc_cid_t>>
      _collectionMap;
};
}
#endif
