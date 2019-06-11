////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2018 ArangoDB GmbH, Cologne, Germany
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
#include "Basics/ReadWriteLock.h"
#include "RocksDBEngine/RocksDBTypes.h"
#include "StorageEngine/StorageEngine.h"
#include "VocBase/AccessMode.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/RocksDBEngine/RocksDBEngineEE.h"
#endif

#include <rocksdb/options.h>
#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

namespace rocksdb {

class TransactionDB;
}

namespace arangodb {

class PhysicalCollection;
class PhysicalView;
class RocksDBBackgroundThread;
class RocksDBEventListener;
class RocksDBKey;
class RocksDBLogValue;
class RocksDBRecoveryHelper;
class RocksDBReplicationManager;
class RocksDBSettingsManager;
class RocksDBSyncThread;
class RocksDBThrottle;  // breaks tons if RocksDBThrottle.h included here
class RocksDBVPackComparator;
class RocksDBWalAccess;
class TransactionCollection;
class TransactionState;

namespace rest {
class RestHandlerFactory;
}

namespace transaction {
class ContextData;
struct Options;
}  // namespace transaction

class RocksDBEngine; // forward

/// @brief helper class to make file-purging thread-safe
/// while there is an object of this type around, it will prevent
/// purging of maybe-needed WAL files via holding a lock in the
/// RocksDB engine. if there is no object of this type around,
/// purging is allowed to happen
class RocksDBFilePurgePreventer {
 public:
  RocksDBFilePurgePreventer(RocksDBFilePurgePreventer const&) = delete;
  RocksDBFilePurgePreventer& operator=(RocksDBFilePurgePreventer const&) = delete;
  RocksDBFilePurgePreventer& operator=(RocksDBFilePurgePreventer&&) = delete;

  explicit RocksDBFilePurgePreventer(RocksDBEngine*);
  RocksDBFilePurgePreventer(RocksDBFilePurgePreventer&&);
  ~RocksDBFilePurgePreventer();

 private:
  RocksDBEngine* _engine;
};

/// @brief helper class to make file-purging thread-safe
/// creating an object of this type will try to acquire a lock that rules
/// out all WAL iteration/WAL tailing while the lock is held. While this
/// is the case, we are allowed to purge any WAL file, because no other
/// thread is able to access it. Note that it is still safe to delete
/// unneeded WAL files, as they will not be accessed by any other thread.
/// however, without this object it would be unsafe to delete WAL files
/// that may still be accessed by WAL tailing etc.
class RocksDBFilePurgeEnabler {
 public:
  RocksDBFilePurgeEnabler(RocksDBFilePurgePreventer const&) = delete;
  RocksDBFilePurgeEnabler& operator=(RocksDBFilePurgeEnabler const&) = delete;
  RocksDBFilePurgeEnabler& operator=(RocksDBFilePurgeEnabler&&) = delete;

  explicit RocksDBFilePurgeEnabler(RocksDBEngine*);
  RocksDBFilePurgeEnabler(RocksDBFilePurgeEnabler&&);
  ~RocksDBFilePurgeEnabler();

  /// @brief returns true if purging any type of WAL file is currently allowed
  bool canPurge() const { return _engine != nullptr; }

 private:
  RocksDBEngine* _engine;
};

class RocksDBEngine final : public StorageEngine {
  friend class RocksDBFilePurgePreventer;
  friend class RocksDBFilePurgeEnabler;

 public:
  // create the storage engine
  explicit RocksDBEngine(application_features::ApplicationServer& server);
  ~RocksDBEngine();

  // inherited from ApplicationFeature
  // ---------------------------------

  // add the storage engine's specific options to the global list of options
  void collectOptions(std::shared_ptr<options::ProgramOptions>) override;
  // validate the storage engine's specific options
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override;

  // preparation phase for storage engine. can be used for internal setup.
  // the storage engine must not start any threads here or write any files
  void prepare() override;
  void start() override;
  void beginShutdown() override;
  void stop() override;
  void unprepare() override;

  // minimum timeout for the synchronous replication
  double minimumSyncReplicationTimeout() const override { return 1.0; }

  bool supportsDfdb() const override { return false; }
  bool useRawDocumentPointers() override { return false; }

  std::unique_ptr<TransactionManager> createTransactionManager() override;
  std::unique_ptr<transaction::ContextData> createTransactionContextData() override;
  std::unique_ptr<TransactionState> createTransactionState(TRI_vocbase_t& vocbase,
                                                           transaction::Options const& options) override;
  std::unique_ptr<TransactionCollection> createTransactionCollection(
      TransactionState& state, TRI_voc_cid_t cid, AccessMode::Type accessType,
      int nestingLevel) override;

  // create storage-engine specific collection
  std::unique_ptr<PhysicalCollection> createPhysicalCollection(
      LogicalCollection& collection, velocypack::Slice const& info) override;

  void getStatistics(velocypack::Builder& builder) const override;

  // inventory functionality
  // -----------------------

  void getDatabases(arangodb::velocypack::Builder& result) override;

  void getCollectionInfo(TRI_vocbase_t& vocbase, TRI_voc_cid_t cid,
                         arangodb::velocypack::Builder& result,
                         bool includeIndexes, TRI_voc_tick_t maxTick) override;

  int getCollectionsAndIndexes(TRI_vocbase_t& vocbase, arangodb::velocypack::Builder& result,
                               bool wasCleanShutdown, bool isUpgrade) override;

  int getViews(TRI_vocbase_t& vocbase, arangodb::velocypack::Builder& result) override;

  std::string versionFilename(TRI_voc_tick_t id) const override;
  std::string databasePath(TRI_vocbase_t const* /*vocbase*/) const override {
    return _basePath;
  }
  std::string collectionPath(TRI_vocbase_t const& /*vocbase*/, TRI_voc_cid_t /*id*/
                             ) const override {
    return std::string();  // no path to be returned here
  }

  void cleanupReplicationContexts() override;

  velocypack::Builder getReplicationApplierConfiguration(TRI_vocbase_t& vocbase,
                                                         int& status) override;
  velocypack::Builder getReplicationApplierConfiguration(int& status) override;
  int removeReplicationApplierConfiguration(TRI_vocbase_t& vocbase) override;
  int removeReplicationApplierConfiguration() override;
  int saveReplicationApplierConfiguration(TRI_vocbase_t& vocbase,
                                          velocypack::Slice slice, bool doSync) override;
  int saveReplicationApplierConfiguration(arangodb::velocypack::Slice slice, bool doSync) override;
  // TODO worker-safety
  Result handleSyncKeys(DatabaseInitialSyncer& syncer, LogicalCollection& col,
                        std::string const& keysId) override;
  Result createLoggerState(TRI_vocbase_t* vocbase, velocypack::Builder& builder) override;
  Result createTickRanges(velocypack::Builder& builder) override;
  Result firstTick(uint64_t& tick) override;
  Result lastLogger(TRI_vocbase_t& vocbase,
                    std::shared_ptr<transaction::Context> transactionContext,
                    uint64_t tickStart, uint64_t tickEnd,
                    std::shared_ptr<velocypack::Builder>& builderSPtr) override;
  WalAccess const* walAccess() const override;

  // database, collection and index management
  // -----------------------------------------

  // intentionally empty, not useful for this type of engine
  void waitForSyncTick(TRI_voc_tick_t) override {}

  /// @brief return a list of the currently open WAL files
  std::vector<std::string> currentWalFiles() const override;

  Result flushWal(bool waitForSync, bool waitForCollector, bool writeShutdownFile) override;
  void waitForEstimatorSync(std::chrono::milliseconds maxWaitTime) override;

  virtual std::unique_ptr<TRI_vocbase_t> openDatabase(velocypack::Slice const& args,
                                                      bool isUpgrade, int& status) override;
  std::unique_ptr<TRI_vocbase_t> createDatabase(TRI_voc_tick_t id,
                                                velocypack::Slice const& args,
                                                int& status) override;
  int writeCreateDatabaseMarker(TRI_voc_tick_t id, velocypack::Slice const& slice) override;
  void prepareDropDatabase(TRI_vocbase_t& vocbase, bool useWriteMarker, int& status) override;
  Result dropDatabase(TRI_vocbase_t& database) override;
  void waitUntilDeletion(TRI_voc_tick_t id, bool force, int& status) override;

  // wal in recovery
  bool inRecovery() override;

  // start compactor thread and delete files form collections marked as deleted
  void recoveryDone(TRI_vocbase_t& vocbase) override;

 public:
  /// @brief disallow purging of WAL files even if the archive gets too big
  /// removing WAL files does not seem to be thread-safe, so we have to track
  /// usage of WAL files ourselves
  RocksDBFilePurgePreventer disallowPurging() noexcept;

  /// @brief whether or not purging of WAL files is currently allowed
  RocksDBFilePurgeEnabler startPurging() noexcept;

  std::string createCollection(TRI_vocbase_t& vocbase, TRI_voc_cid_t id,
                               LogicalCollection const& collection) override;

  arangodb::Result persistCollection(TRI_vocbase_t& vocbase,
                                     LogicalCollection const& collection) override;

  arangodb::Result dropCollection(TRI_vocbase_t& vocbase, LogicalCollection& collection) override;

  void destroyCollection(TRI_vocbase_t& vocbase, LogicalCollection& collection) override;

  void changeCollection(TRI_vocbase_t& vocbase, TRI_voc_cid_t id,
                        LogicalCollection const& collection, bool doSync) override;

  arangodb::Result renameCollection(TRI_vocbase_t& vocbase, LogicalCollection const& collection,
                                    std::string const& oldName) override;

  void unloadCollection(TRI_vocbase_t& vocbase, LogicalCollection& collection) override;

  arangodb::Result changeView(TRI_vocbase_t& vocbase,
                              arangodb::LogicalView const& view, bool doSync) override;

  arangodb::Result createView(TRI_vocbase_t& vocbase, TRI_voc_cid_t id,
                              arangodb::LogicalView const& view) override;

  virtual void getViewProperties(TRI_vocbase_t& /*vocbase*/,
                                 LogicalView const& /*view*/, velocypack::Builder& /*builder*/
                                 ) override {
    // does nothing
  }

  arangodb::Result dropView(TRI_vocbase_t& vocbase, LogicalView& view) override;

  void destroyView(TRI_vocbase_t& vocbase, LogicalView& view) noexcept override;

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

  rocksdb::TransactionDB* db() const { return _db; }

  Result writeDatabaseMarker(TRI_voc_tick_t id, velocypack::Slice const& slice,
                             RocksDBLogValue&& logValue);
  int writeCreateCollectionMarker(TRI_voc_tick_t databaseId, TRI_voc_cid_t id,
                                  velocypack::Slice const& slice,
                                  RocksDBLogValue&& logValue);

  void addCollectionMapping(uint64_t, TRI_voc_tick_t, TRI_voc_cid_t);
  std::vector<std::pair<TRI_voc_tick_t, TRI_voc_cid_t>> collectionMappings() const;
  void addIndexMapping(uint64_t objectId, TRI_voc_tick_t, TRI_voc_cid_t, TRI_idx_iid_t);
  void removeIndexMapping(uint64_t);

  // Identifies a collection
  typedef std::pair<TRI_voc_tick_t, TRI_voc_cid_t> CollectionPair;
  typedef std::tuple<TRI_voc_tick_t, TRI_voc_cid_t, TRI_idx_iid_t> IndexTriple;
  CollectionPair mapObjectToCollection(uint64_t) const;
  IndexTriple mapObjectToIndex(uint64_t) const;

  void determinePrunableWalFiles(TRI_voc_tick_t minTickToKeep);
  void pruneWalFiles();

  double pruneWaitTimeInitial() const { return _pruneWaitTimeInitial; }

  // management methods for synchronizing with external persistent stores
  virtual TRI_voc_tick_t currentTick() const override;
  virtual TRI_voc_tick_t releasedTick() const override;
  virtual void releaseTick(TRI_voc_tick_t) override;

 private:
  void shutdownRocksDBInstance() noexcept;
  velocypack::Builder getReplicationApplierConfiguration(RocksDBKey const& key, int& status);
  int removeReplicationApplierConfiguration(RocksDBKey const& key);
  int saveReplicationApplierConfiguration(RocksDBKey const& key,
                                          arangodb::velocypack::Slice slice, bool doSync);
  Result dropDatabase(TRI_voc_tick_t);
  bool systemDatabaseExists();
  void addSystemDatabase();
  /// @brief open an existing database. internal function
  std::unique_ptr<TRI_vocbase_t> openExistingDatabase(TRI_voc_tick_t id,
                                                      std::string const& name,
                                                      bool wasCleanShutdown, bool isUpgrade);

  std::string getCompressionSupport() const;

#ifdef USE_ENTERPRISE
  void collectEnterpriseOptions(std::shared_ptr<options::ProgramOptions>);
  void validateEnterpriseOptions(std::shared_ptr<options::ProgramOptions>);
  void prepareEnterprise();
  void startEnterprise();
  void configureEnterpriseRocksDBOptions(rocksdb::Options& options);

  enterprise::RocksDBEngineEEData _eeData;

public:
  std::string const& getEncryptionKey();
#endif
private:
  // activate generation of SHA256 files to parallel .sst files
  bool _createShaFiles;

public:
  // returns whether sha files are created or not
  bool getCreateShaFiles() { return _createShaFiles; }
  // enabled or disable sha file creation. Requires feature not be started.
  void setCreateShaFiles(bool create) { _createShaFiles = create; }

 public:
  static std::string const EngineName;
  static std::string const FeatureName;

  bool canUseRangeDeleteInWal() const;

  rocksdb::Options const& rocksDBOptions() const { return _options; }

  /// @brief recovery manager
  RocksDBSettingsManager* settingsManager() const {
    TRI_ASSERT(_settingsManager);
    return _settingsManager.get();
  }

  /// @brief manages the ongoing dump clients
  RocksDBReplicationManager* replicationManager() const {
    TRI_ASSERT(_replicationManager);
    return _replicationManager.get();
  }

  /// @brief returns a pointer to the sync thread
  /// note: returns a nullptr if automatic syncing is turned off!
  RocksDBSyncThread* syncThread() const { return _syncThread.get(); }

  static arangodb::Result registerRecoveryHelper(std::shared_ptr<RocksDBRecoveryHelper> helper);
  static std::vector<std::shared_ptr<RocksDBRecoveryHelper>> const& recoveryHelpers();

 private:
  /// single rocksdb database used in this storage engine
  rocksdb::TransactionDB* _db;
  /// default read options
  rocksdb::Options _options;
  /// arangodb comparator - requried because of vpack in keys
  std::unique_ptr<RocksDBVPackComparator> _vpackCmp;
  /// path used by rocksdb (inside _basePath)
  std::string _path;
  /// path to arangodb data dir
  std::string _basePath;

  /// @brief repository for replication contexts
  std::unique_ptr<RocksDBReplicationManager> _replicationManager;
  /// @brief tracks the count of documents in collections
  std::unique_ptr<RocksDBSettingsManager> _settingsManager;
  /// @brief Local wal access abstraction
  std::unique_ptr<RocksDBWalAccess> _walAccess;

  /// Background thread handling garbage collection etc
  std::unique_ptr<RocksDBBackgroundThread> _backgroundThread;
  uint64_t _maxTransactionSize;       // maximum allowed size for a transaction
  uint64_t _intermediateCommitSize;   // maximum size for a
                                      // transaction before an
                                      // intermediate commit is performed
  uint64_t _intermediateCommitCount;  // limit of transaction count
                                      // for intermediate commit

  // hook-ins for recovery process
  static std::vector<std::shared_ptr<RocksDBRecoveryHelper>> _recoveryHelpers;

  mutable basics::ReadWriteLock _mapLock;
  std::unordered_map<uint64_t, CollectionPair> _collectionMap;
  std::unordered_map<uint64_t, IndexTriple> _indexMap;

  /// @brief protects _prunableWalFiles
  mutable basics::ReadWriteLock _walFileLock;

  /// @brief which WAL files can be pruned when
  /// an expiration time of <= 0.0 means the file does not have expired, but
  /// still should be purged because the WAL files archive outgrew its max
  /// configured size
  std::unordered_map<std::string, double> _prunableWalFiles;

  // number of seconds to wait before an obsolete WAL file is actually pruned
  double _pruneWaitTime;

  // number of seconds to wait initially after server start before WAL file
  // deletion kicks in
  double _pruneWaitTimeInitial;

  /// @brief maximum total size (in bytes) of archived WAL files
  uint64_t _maxWalArchiveSizeLimit;

  // do not release walfiles containing writes later than this
  TRI_voc_tick_t _releasedTick;

  /// Background thread handling WAL syncing
  /// note: this is a nullptr if automatic syncing is turned off!
  std::unique_ptr<RocksDBSyncThread> _syncThread;

  // WAL sync interval, specified in milliseconds by end user, but uses microseconds internally
  uint64_t _syncInterval;

  // use write-throttling
  bool _useThrottle;
  
  /// @brief whether or not to use _releasedTick when determining the WAL files to prune
  bool _useReleasedTick;

  // activate rocksdb's debug logging
  bool _debugLogging;

  // code to pace ingest rate of writes to reduce chances of compactions getting
  // too far behind and blocking incoming writes
  // (will only be set if _useThrottle is true)
  std::shared_ptr<RocksDBThrottle> _listener;

  // optional code to notice when rocksdb creates or deletes .ssh files.  Currently
  //  uses that input to create or delete parallel sha256 files
  std::shared_ptr<RocksDBEventListener> _shaListener;

  arangodb::basics::ReadWriteLock _purgeLock;
};

}  // namespace arangodb

#endif
