////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#pragma once

#include "Basics/Common.h"
#include "Basics/Mutex.h"
#include "Basics/ReadWriteLock.h"
#include "RocksDBEngine/RocksDBKeyBounds.h"
#include "RocksDBEngine/RocksDBTypes.h"
#include "StorageEngine/StorageEngine.h"
#include "VocBase/AccessMode.h"
#include "VocBase/Identifiers/DataSourceId.h"
#include "VocBase/Identifiers/IndexId.h"
#include "Metrics/Fwd.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/RocksDBEngine/RocksDBEngineEE.h"
#endif

#include <rocksdb/options.h>
#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

namespace rocksdb {

class TransactionDB;
class EncryptionProvider;
}

namespace arangodb {

struct RocksDBLogPersistor;
class PhysicalCollection;
class RocksDBBackgroundErrorListener;
class RocksDBBackgroundThread;
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

  HealthData healthCheck() override;

  std::unique_ptr<transaction::Manager> createTransactionManager(transaction::ManagerFeature&) override;
  std::shared_ptr<TransactionState> createTransactionState(
      TRI_vocbase_t& vocbase, TransactionId, transaction::Options const& options) override;

  // create storage-engine specific collection
  std::unique_ptr<PhysicalCollection> createPhysicalCollection(
      LogicalCollection& collection, velocypack::Slice const& info) override;

  void getStatistics(velocypack::Builder& builder, bool v2) const override;
  void getStatistics(std::string& result, bool v2) const override;

  // inventory functionality
  // -----------------------

  void getDatabases(arangodb::velocypack::Builder& result) override;

  void getCollectionInfo(TRI_vocbase_t& vocbase, DataSourceId cid,
                         arangodb::velocypack::Builder& result,
                         bool includeIndexes, TRI_voc_tick_t maxTick) override;

  ErrorCode getCollectionsAndIndexes(TRI_vocbase_t& vocbase,
                                     arangodb::velocypack::Builder& result,
                                     bool wasCleanShutdown, bool isUpgrade) override;

  void getReplicatedLogs(TRI_vocbase_t& vocbase, arangodb::velocypack::Builder& result);
  ErrorCode getViews(TRI_vocbase_t& vocbase, arangodb::velocypack::Builder& result) override;

  std::string versionFilename(TRI_voc_tick_t id) const override;
  std::string dataPath() const override {
    return _basePath + TRI_DIR_SEPARATOR_STR + "engine-rocksdb";
  }
  std::string databasePath(TRI_vocbase_t const* /*vocbase*/) const override {
    return _basePath;
  }
  void cleanupReplicationContexts() override;

  velocypack::Builder getReplicationApplierConfiguration(TRI_vocbase_t& vocbase,
                                                         ErrorCode& status) override;
  velocypack::Builder getReplicationApplierConfiguration(ErrorCode& status) override;
  ErrorCode removeReplicationApplierConfiguration(TRI_vocbase_t& vocbase) override;
  ErrorCode removeReplicationApplierConfiguration() override;
  ErrorCode saveReplicationApplierConfiguration(TRI_vocbase_t& vocbase,
                                                velocypack::Slice slice, bool doSync) override;
  ErrorCode saveReplicationApplierConfiguration(arangodb::velocypack::Slice slice,
                                                bool doSync) override;
  // TODO worker-safety
  Result handleSyncKeys(DatabaseInitialSyncer& syncer, LogicalCollection& col,
                        std::string const& keysId) override;
  Result createLoggerState(TRI_vocbase_t* vocbase, velocypack::Builder& builder) override;
  Result createTickRanges(velocypack::Builder& builder) override;
  Result firstTick(uint64_t& tick) override;
  Result lastLogger(TRI_vocbase_t& vocbase,
                    uint64_t tickStart, uint64_t tickEnd,
                    velocypack::Builder& builder) override;
  WalAccess const* walAccess() const override;

  // database, collection and index management
  // -----------------------------------------

  /// @brief return a list of the currently open WAL files
  std::vector<std::string> currentWalFiles() const override;

  /// @brief flushes the RocksDB WAL. 
  /// the optional parameter "waitForSync" is currently only used when the
  /// "waitForCollector" parameter is also set to true. If "waitForCollector"
  /// is true, all the RocksDB column family memtables are flushed, and, if
  /// "waitForSync" is set, additionally synced to disk. The only call site
  /// that uses "waitForCollector" currently is hot backup.
  /// The function parameter name are a remainder from MMFiles times, when
  /// they made more sense. This can be refactored at any point, so that
  /// flushing column families becomes a separate API.
  Result flushWal(bool waitForSync, bool waitForCollector) override;
  void waitForEstimatorSync(std::chrono::milliseconds maxWaitTime) override;

  virtual std::unique_ptr<TRI_vocbase_t> openDatabase(arangodb::CreateDatabaseInfo&& info,
                                                      bool isUpgrade) override;
  std::unique_ptr<TRI_vocbase_t> createDatabase(arangodb::CreateDatabaseInfo&&,
                                                ErrorCode& status) override;
  Result writeCreateDatabaseMarker(TRI_voc_tick_t id, velocypack::Slice const& slice) override;
  Result prepareDropDatabase(TRI_vocbase_t& vocbase) override;
  Result dropDatabase(TRI_vocbase_t& database) override;

  // wal in recovery
  RecoveryState recoveryState() noexcept override;

  /// @brief current recovery tick
  TRI_voc_tick_t recoveryTick() noexcept override;

  /// @brief disallow purging of WAL files even if the archive gets too big
  /// removing WAL files does not seem to be thread-safe, so we have to track
  /// usage of WAL files ourselves
  RocksDBFilePurgePreventer disallowPurging() noexcept;

  /// @brief whether or not purging of WAL files is currently allowed
  RocksDBFilePurgeEnabler startPurging() noexcept;

  void scheduleTreeRebuild(TRI_voc_tick_t database, std::string const& collection);
  void processTreeRebuilds();

  void compactRange(RocksDBKeyBounds bounds);
  void processCompactions();

  virtual auto createReplicatedLog(TRI_vocbase_t&, arangodb::replication2::LogId)
      -> ResultT<std::shared_ptr<arangodb::replication2::replicated_log::PersistedLog>> override;
  virtual auto dropReplicatedLog(TRI_vocbase_t&,
                                 std::shared_ptr<arangodb::replication2::replicated_log::PersistedLog> const&)
      -> Result override;

  void createCollection(TRI_vocbase_t& vocbase,
                        LogicalCollection const& collection) override;

  void prepareDropCollection(TRI_vocbase_t& vocbase, LogicalCollection& collection) override;
  arangodb::Result dropCollection(TRI_vocbase_t& vocbase, LogicalCollection& collection) override;

  void changeCollection(TRI_vocbase_t& vocbase,
                        LogicalCollection const& collection, bool doSync) override;

  arangodb::Result renameCollection(TRI_vocbase_t& vocbase, LogicalCollection const& collection,
                                    std::string const& oldName) override;

  arangodb::Result changeView(TRI_vocbase_t& vocbase,
                              arangodb::LogicalView const& view, bool doSync) override;

  arangodb::Result createView(TRI_vocbase_t& vocbase, DataSourceId id,
                              arangodb::LogicalView const& view) override;

  arangodb::Result dropView(TRI_vocbase_t const& vocbase, LogicalView const& view) override;
  
  arangodb::Result compactAll(bool changeLevel, bool compactBottomMostLevel) override;

  /// @brief Add engine-specific optimizer rules
  void addOptimizerRules(aql::OptimizerRulesFeature& feature) override;

  /// @brief Add engine-specific V8 functions
  void addV8Functions() override;

  /// @brief Add engine-specific REST handlers
  void addRestHandlers(rest::RestHandlerFactory& handlerFactory) override;

  void addParametersForNewCollection(arangodb::velocypack::Builder& builder,
                                     arangodb::velocypack::Slice info) override;

  rocksdb::TransactionDB* db() const { return _db; }

  Result writeDatabaseMarker(TRI_voc_tick_t id, velocypack::Slice const& slice,
                             RocksDBLogValue&& logValue);
  Result writeCreateCollectionMarker(TRI_voc_tick_t databaseId, DataSourceId id,
                                     velocypack::Slice const& slice,
                                     RocksDBLogValue&& logValue);

  void addCollectionMapping(uint64_t, TRI_voc_tick_t, DataSourceId);
  std::vector<std::pair<TRI_voc_tick_t, DataSourceId>> collectionMappings() const;
  void addIndexMapping(uint64_t objectId, TRI_voc_tick_t, DataSourceId, IndexId);
  void removeIndexMapping(uint64_t);

  // Identifies a collection
  typedef std::pair<TRI_voc_tick_t, DataSourceId> CollectionPair;
  typedef std::tuple<TRI_voc_tick_t, DataSourceId, IndexId> IndexTriple;
  CollectionPair mapObjectToCollection(uint64_t) const;
  IndexTriple mapObjectToIndex(uint64_t) const;

  /// @brief determine how many archived WAL files are available. this is called
  /// during the first few minutes after the instance start, when we don't want
  /// to prune any WAL files yet.
  /// this also updates the metrics for the number of available WAL files.
  void determineWalFilesInitial();

  /// @brief determine which archived WAL files are prunable. as a side-effect,
  /// this updates the metrics for the number of available and prunable WAL
  /// files.
  void determinePrunableWalFiles(TRI_voc_tick_t minTickToKeep);
  void pruneWalFiles();

  double pruneWaitTimeInitial() const { return _pruneWaitTimeInitial; }
  bool useEdgeCache() const { return _useEdgeCache; }

  // management methods for synchronizing with external persistent stores
  virtual TRI_voc_tick_t currentTick() const override;
  virtual TRI_voc_tick_t releasedTick() const override;
  virtual void releaseTick(TRI_voc_tick_t) override;

  /// @brief whether or not the database existed at startup. this function
  /// provides a valid answer only after start() has successfully finished, 
  /// so don't call it from other features during their start() if they are
  /// earlier in the startup sequence
  bool dbExisted() const noexcept { return _dbExisted; }

  void trackRevisionTreeHibernation() noexcept;
  void trackRevisionTreeResurrection() noexcept;

  void trackRevisionTreeMemoryIncrease(std::uint64_t value) noexcept;
  void trackRevisionTreeMemoryDecrease(std::uint64_t value) noexcept;

#ifdef USE_ENTERPRISE
  bool encryptionKeyRotationEnabled() const;

  bool isEncryptionEnabled() const;
  
  std::string const& getEncryptionKey();
  
  std::string getEncryptionTypeFile() const;
  
  std::string getKeyStoreFolder() const;
  
  std::vector<enterprise::EncryptionSecret> userEncryptionSecrets() const;
  
  /// rotate user-provided keys, writes out the internal key files
  Result rotateUserEncryptionKeys();
  
  /// load encryption at rest key from specified keystore
  Result decryptInternalKeystore(std::string const& keystorePath,
                                 std::vector<enterprise::EncryptionSecret>& userKeys,
                                 std::string& encryptionKey) const;
#endif
 
  // returns whether sha files are created or not
  bool getCreateShaFiles() const { return _createShaFiles; }

  // enabled or disable sha file creation. Requires feature not be started.
  void setCreateShaFiles(bool create) { _createShaFiles = create; }

  rocksdb::EncryptionProvider* encryptionProvider() const noexcept {
#ifdef USE_ENTERPRISE
    return _eeData._encryptionProvider;
#else
    return nullptr;
#endif
  }

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

  bool hasBackgroundError() const;

  static arangodb::Result registerRecoveryHelper(std::shared_ptr<RocksDBRecoveryHelper> helper);
  static std::vector<std::shared_ptr<RocksDBRecoveryHelper>> const& recoveryHelpers();

 private:
  void shutdownRocksDBInstance() noexcept;
  void waitForCompactionJobsToFinish();
  velocypack::Builder getReplicationApplierConfiguration(RocksDBKey const& key,
                                                         ErrorCode& status);
  ErrorCode removeReplicationApplierConfiguration(RocksDBKey const& key);
  ErrorCode saveReplicationApplierConfiguration(RocksDBKey const& key,
                                                arangodb::velocypack::Slice slice,
                                                bool doSync);
  Result dropDatabase(TRI_voc_tick_t);
  bool systemDatabaseExists();
  void addSystemDatabase();
  /// @brief open an existing database. internal function
  std::unique_ptr<TRI_vocbase_t> openExistingDatabase(arangodb::CreateDatabaseInfo&& info,
                                                      bool wasCleanShutdown, bool isUpgrade);

  std::string getCompressionSupport() const;

#ifdef USE_ENTERPRISE
  void collectEnterpriseOptions(std::shared_ptr<options::ProgramOptions>);
  void validateEnterpriseOptions(std::shared_ptr<options::ProgramOptions>);
  void prepareEnterprise();
  void configureEnterpriseRocksDBOptions(rocksdb::Options& options, bool createdEngineDir);
  void validateJournalFiles() const;
 
  Result readUserEncryptionSecrets(std::vector<enterprise::EncryptionSecret>& outlist) const;

  enterprise::RocksDBEngineEEData _eeData;

  /// load encryption at rest key from keystore
  Result decryptInternalKeystore();
  /// encrypt the internal keystore with all user keys
  Result encryptInternalKeystore();
#endif
 
 public:
  static std::string const EngineName;
  static std::string const FeatureName;

 private:
  /// single rocksdb database used in this storage engine
  rocksdb::TransactionDB* _db;
  /// default read options
  rocksdb::Options _options;
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

  uint64_t _maxParallelCompactions;

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

  // WAL sync interval, specified in milliseconds by end user, but uses
  // microseconds internally
  uint64_t _syncInterval;
  
  // WAL sync delay threshold. Any WAL disk sync longer ago than this value
  // will trigger a warning (in milliseconds)
  uint64_t _syncDelayThreshold;

  /// @brief minimum required percentage of free disk space for considering the
  /// server "healthy". this is expressed as a floating point value between 0 and 1!
  /// if set to 0.0, the % amount of free disk is ignored in checks.
  double _requiredDiskFreePercentage;

  /// @brief minimum number of free bytes on disk for considering the server healthy.
  /// if set to 0, the number of free bytes on disk is ignored in checks.
  uint64_t _requiredDiskFreeBytes;

  // use write-throttling
  bool _useThrottle;

  /// @brief whether or not to use _releasedTick when determining the WAL files to prune
  bool _useReleasedTick;

  /// @brief activate rocksdb's debug logging
  bool _debugLogging;

  /// @brief whether or not the in-memory cache for edges is used
  bool _useEdgeCache;
  
  /// @brief activate generation of SHA256 files to parallel .sst files
  bool _createShaFiles;

  /// @brief whether or not the last health check was successful.
  /// this is used to determine when to execute the potentially expensive
  /// checks for free disk space
  bool _lastHealthCheckSuccessful;

  /// @brief whether or not the DB existed at startup
  bool _dbExisted;

  // code to pace ingest rate of writes to reduce chances of compactions getting
  // too far behind and blocking incoming writes
  // (will only be set if _useThrottle is true)
  std::shared_ptr<RocksDBThrottle> _throttleListener;
  
  /// @brief background error listener. will be invoked by rocksdb in case of
  /// a non-recoverable error
  std::shared_ptr<RocksDBBackgroundErrorListener> _errorListener;

  arangodb::basics::ReadWriteLock _purgeLock;
  
  /// @brief mutex that protects the storage engine health check
  arangodb::Mutex _healthMutex;

  /// @brief timestamp of last health check log message. we only log health check
  /// errors every so often, in order to prevent log spamming
  std::chrono::steady_clock::time_point _lastHealthLogMessageTimestamp;
  
  /// @brief timestamp of last health check warning message. we only log health check
  /// warnings every so often, in order to prevent log spamming
  std::chrono::steady_clock::time_point _lastHealthLogWarningTimestamp;

  /// @brief global health data, updated periodically
  HealthData _healthData;

  /// @brief lock for _rebuildCollections
  arangodb::Mutex _rebuildCollectionsLock;
  /// @brief map of database/collection-guids for which we need to repair trees
  std::map<std::pair<TRI_voc_tick_t, std::string>, bool> _rebuildCollections;
  /// @brief number of currently running tree rebuild jobs jobs
  size_t _runningRebuilds;

  /// @brief lock for _pendingCompactions and _runningCompactions
  arangodb::basics::ReadWriteLock _pendingCompactionsLock;
  /// @brief bounds for compactions that we have to process
  std::deque<RocksDBKeyBounds> _pendingCompactions;
  /// @brief number of currently running compaction jobs
  size_t _runningCompactions;

  // frequency for throttle in milliseconds
  uint64_t _throttleFrequency = 60 * 1000; 

  // number of historic data slots to keep around for throttle
  uint64_t _throttleSlots = 63;
  // adaptiveness factor for throttle
  // following is a heuristic value, determined by trial and error.
  // its job is slow down the rate of change in the current throttle.
  // we do not want sudden changes in one or two intervals to swing
  // the throttle value wildly. the goal is a nice, even throttle value.
  uint64_t _throttleScalingFactor = 17;
  // max write rate enforced by throttle
  uint64_t _throttleMaxWriteRate = 0;
  // trigger point where level-0 file is considered "too many pending"
  // (from original Google leveldb db/dbformat.h)
  uint64_t _throttleSlowdownWritesTrigger = 8;
  
  metrics::Gauge<uint64_t>& _metricsWalSequenceLowerBound;
  metrics::Gauge<uint64_t>& _metricsArchivedWalFiles;
  metrics::Gauge<uint64_t>& _metricsPrunableWalFiles;
  metrics::Gauge<uint64_t>& _metricsWalPruningActive;
  metrics::Gauge<uint64_t>& _metricsTreeMemoryUsage;
  metrics::Counter& _metricsTreeRebuildsSuccess;
  metrics::Counter& _metricsTreeRebuildsFailure;
  metrics::Counter& _metricsTreeHibernations;
  metrics::Counter& _metricsTreeResurrections;
  
  // @brief persistor for replicated logs
  std::shared_ptr<RocksDBLogPersistor> _logPersistor;
};

static constexpr const char* kEncryptionTypeFile = "ENCRYPTION";
static constexpr const char* kEncryptionKeystoreFolder = "ENCRYPTION-KEYS";

}  // namespace arangodb

