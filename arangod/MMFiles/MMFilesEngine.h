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

#ifndef ARANGOD_MMFILES_MMFILES_ENGINE_H
#define ARANGOD_MMFILES_MMFILES_ENGINE_H 1

#include "Basics/Common.h"
#include "Basics/Mutex.h"
#include "MMFiles/MMFilesCollectorCache.h"
#include "MMFiles/MMFilesDatafile.h"
#include "StorageEngine/StorageEngine.h"
#include "VocBase/AccessMode.h"

#include <velocypack/Builder.h>

namespace arangodb {
class MMFilesCleanupThread;
class MMFilesCompactorThread;
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

/// @brief collection file structure
struct MMFilesEngineCollectionFiles {
  std::vector<std::string> journals;
  std::vector<std::string> compactors;
  std::vector<std::string> datafiles;
  std::vector<std::string> indexes;
};

class MMFilesEngine final : public StorageEngine {
 public:
  // create the storage engine
  explicit MMFilesEngine(application_features::ApplicationServer*);

  ~MMFilesEngine();

  // inherited from ApplicationFeature
  // ---------------------------------

  // add the storage engine's specifc options to the global list of options
  void collectOptions(std::shared_ptr<options::ProgramOptions>) override;

  // validate the storage engine's specific options
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override;

  // preparation phase for storage engine. can be used for internal setup.
  // the storage engine must not start any threads here or write any files
  void prepare() override;

  // initialize engine
  void start() override;

  // flush wal wait for collector
  void stop() override;
  
  bool supportsDfdb() const override { return true; }
  
  bool useRawDocumentPointers() override { return true; }

  std::shared_ptr<arangodb::velocypack::Builder>
  getReplicationApplierConfiguration(TRI_vocbase_t* vocbase,
                                     int& status) override;
  int removeReplicationApplierConfiguration(TRI_vocbase_t* vocbase) override;
  int saveReplicationApplierConfiguration(TRI_vocbase_t* vocbase,
                                          arangodb::velocypack::Slice slice,
                                          bool doSync) override;
  int handleSyncKeys(arangodb::InitialSyncer& syncer,
                     arangodb::LogicalCollection* col,
                     std::string const& keysId, std::string const& cid,
                     std::string const& collectionName, TRI_voc_tick_t maxTick,
                     std::string& errorMsg) override;

  Result createLoggerState(TRI_vocbase_t* vocbase, VPackBuilder& builder) override;
  Result createTickRanges(VPackBuilder& builder) override;
  Result firstTick(uint64_t& tick) override;
  Result lastLogger(TRI_vocbase_t* vocbase, std::shared_ptr<transaction::Context>, uint64_t tickStart, uint64_t tickEnd,  std::shared_ptr<VPackBuilder>& builderSPtr) override;

  TransactionManager* createTransactionManager() override;
  transaction::ContextData* createTransactionContextData() override;
  TransactionState* createTransactionState(TRI_vocbase_t*, transaction::Options const&) override;
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

  // fill the Builder object with an array of databases that were detected
  // by the storage engine. this method must sort out databases that were not
  // fully created (see "createDatabase" below). called at server start only
  void getDatabases(arangodb::velocypack::Builder& result) override;

  // fills the provided builder with information about the collection
  void getCollectionInfo(TRI_vocbase_t* vocbase, TRI_voc_cid_t cid,
                         arangodb::velocypack::Builder& result,
                         bool includeIndexes, TRI_voc_tick_t maxTick) override;

  // fill the Builder object with an array of collections (and their
  // corresponding
  // indexes) that were detected by the storage engine. called at server start
  // separately
  // for each database
  int getCollectionsAndIndexes(TRI_vocbase_t* vocbase,
                               arangodb::velocypack::Builder& result,
                               bool wasCleanShutdown, bool isUpgrade) override;

  int getViews(TRI_vocbase_t* vocbase,
               arangodb::velocypack::Builder& result) override;

  // return the path for a collection
  std::string collectionPath(TRI_vocbase_t const* vocbase,
                             TRI_voc_cid_t id) const override {
    return collectionDirectory(vocbase->id(), id);
  }

  // database, collection and index management
  // -----------------------------------------

  // return the path for a database
  std::string databasePath(TRI_vocbase_t const* vocbase) const override {
    return databaseDirectory(vocbase->id());
  }

  std::string versionFilename(TRI_voc_tick_t id) const override;

  void waitForSync(TRI_voc_tick_t tick) override;

  virtual TRI_vocbase_t* openDatabase(
      arangodb::velocypack::Slice const& parameters, bool isUpgrade,
      int&) override;
  TRI_vocbase_t* createDatabase(TRI_voc_tick_t id,
                                arangodb::velocypack::Slice const& args,
                                int& status) override {
    status = TRI_ERROR_NO_ERROR;
    return createDatabaseMMFiles(id, args);
  }
  int writeCreateDatabaseMarker(TRI_voc_tick_t id,
                                VPackSlice const& slice) override;

  void prepareDropDatabase(TRI_vocbase_t* vocbase, bool useWriteMarker,
                           int& status) override;
  Result dropDatabase(TRI_vocbase_t* database) override;
  void waitUntilDeletion(TRI_voc_tick_t id, bool force, int& status) override;

  // wal in recovery
  bool inRecovery() override;

  // start compactor thread and delete files form collections marked as deleted
  void recoveryDone(TRI_vocbase_t* vocbase) override;

 private:
  int dropDatabaseMMFiles(TRI_vocbase_t* vocbase);
  TRI_vocbase_t* createDatabaseMMFiles(TRI_voc_tick_t id,
                                       arangodb::velocypack::Slice const& data);

 public:
  // asks the storage engine to create a collection as specified in the VPack
  // Slice object and persist the creation info. It is guaranteed by the server
  // that no other active collection with the same name and id exists in the
  // same
  // database when this function is called. If this operation fails somewhere in
  // the middle, the storage engine is required to fully clean up the creation
  // and throw only then, so that subsequent collection creation requests will
  // not fail.
  // the WAL entry for the collection creation will be written *after* the call
  // to "createCollection" returns
  std::string createCollection(TRI_vocbase_t* vocbase, TRI_voc_cid_t id,
                               arangodb::LogicalCollection const*) override;

  // asks the storage engine to persist the collection.
  // After this call the collection is persisted over recovery.
  // This call will write wal markers.
  arangodb::Result persistCollection(
      TRI_vocbase_t* vocbase, arangodb::LogicalCollection const*) override;

  // asks the storage engine to drop the specified collection and persist the
  // deletion info. Note that physical deletion of the collection data must not
  // be carried out by this call, as there may
  // still be readers of the collection's data.
  // This call will write the WAL entry for collection deletion
  arangodb::Result dropCollection(TRI_vocbase_t* vocbase,
                                  arangodb::LogicalCollection*) override;

  // perform a physical deletion of the collection
  // After this call data of this collection is corrupted, only perform if
  // assured that no one is using the collection anymore
  void destroyCollection(TRI_vocbase_t* vocbase,
                         arangodb::LogicalCollection*) override;

  // asks the storage engine to change properties of the collection as specified
  // in
  // the VPack Slice object and persist them. If this operation fails
  // somewhere in the middle, the storage engine is required to fully revert the
  // property changes and throw only then, so that subsequent operations will
  // not fail.
  // the WAL entry for the propery change will be written *after* the call
  // to "changeCollection" returns
  void changeCollection(TRI_vocbase_t* vocbase, TRI_voc_cid_t id,
                        arangodb::LogicalCollection const*,
                        bool doSync) override;

  // asks the storage engine to persist renaming of a collection
  // This will write a renameMarker if not in recovery
  arangodb::Result renameCollection(TRI_vocbase_t* vocbase,
                                    arangodb::LogicalCollection const*,
                                    std::string const& oldName) override;

  // asks the storage engine to create an index as specified in the VPack
  // Slice object and persist the creation info. The database id, collection id
  // and index data are passed in the Slice object. Note that this function
  // is not responsible for inserting the individual documents into the index.
  // If this operation fails somewhere in the middle, the storage engine is
  // required
  // to fully clean up the creation and throw only then, so that subsequent
  // index
  // creation requests will not fail.
  // the WAL entry for the index creation will be written *after* the call
  // to "createIndex" returns
  void createIndex(TRI_vocbase_t* vocbase, TRI_voc_cid_t collectionId,
                   TRI_idx_iid_t id,
                   arangodb::velocypack::Slice const& data) override;

  // asks the storage engine to drop the specified index and persist the
  // deletion
  // info. Note that physical deletion of the index must not be carried out by
  // this call,
  // as there may still be users of the index. It is recommended that this
  // operation
  // only sets a deletion flag for the index but let's an async task perform
  // the actual deletion.
  // the WAL entry for index deletion will be written *after* the call
  // to "dropIndex" returns
  void dropIndex(TRI_vocbase_t* vocbase, TRI_voc_cid_t collectionId,
                 TRI_idx_iid_t id);

  void dropIndexWalMarker(TRI_vocbase_t* vocbase, TRI_voc_cid_t collectionId,
                          arangodb::velocypack::Slice const& data,
                          bool writeMarker, int&);

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

  std::string createViewDirectoryName(std::string const& basePath,
                                      TRI_voc_cid_t id);

  void saveViewInfo(TRI_vocbase_t* vocbase, TRI_voc_cid_t id,
                    arangodb::LogicalView const*, bool forceSync) const;

  void signalCleanup(TRI_vocbase_t* vocbase) override;

  /// @brief scans a collection and locates all files
  MMFilesEngineCollectionFiles scanCollectionDirectory(std::string const& path);

  /// @brief remove data of expired compaction blockers
  bool cleanupCompactionBlockers(TRI_vocbase_t* vocbase);

  /// @brief insert a compaction blocker
  int insertCompactionBlocker(TRI_vocbase_t* vocbase, double ttl,
                              TRI_voc_tick_t& id);

  /// @brief touch an existing compaction blocker
  int extendCompactionBlocker(TRI_vocbase_t* vocbase, TRI_voc_tick_t id,
                              double ttl);

  /// @brief remove an existing compaction blocker
  int removeCompactionBlocker(TRI_vocbase_t* vocbase,
                              TRI_voc_tick_t id);

  /// @brief a callback function that is run while it is guaranteed that there
  /// is no compaction ongoing
  void preventCompaction(
      TRI_vocbase_t* vocbase,
      std::function<void(TRI_vocbase_t*)> const& callback);

  /// @brief a callback function that is run there is no compaction ongoing
  bool tryPreventCompaction(TRI_vocbase_t* vocbase,
                            std::function<void(TRI_vocbase_t*)> const& callback,
                            bool checkForActiveBlockers);

  int shutdownDatabase(TRI_vocbase_t* vocbase) override;

  int openCollection(TRI_vocbase_t* vocbase, LogicalCollection* collection,
                     bool ignoreErrors);

  /// @brief Add engine-specific AQL functions.
  void addAqlFunctions() override;

  /// @brief Add engine-specific optimizer rules
  void addOptimizerRules() override;

  /// @brief Add engine-specific V8 functions
  void addV8Functions() override;

  /// @brief Add engine-specific REST handlers
  void addRestHandlers(rest::RestHandlerFactory*) override;

  /// @brief transfer markers into a collection
  int transferMarkers(LogicalCollection* collection, MMFilesCollectorCache*,
                      MMFilesOperationsType const&);

  std::string viewDirectory(TRI_voc_tick_t databaseId,
                            TRI_voc_cid_t viewId) const;

 private:
  /// @brief: check the initial markers in a datafile
  bool checkDatafileHeader(MMFilesDatafile* datafile,
                           std::string const& filename) const;

  /// @brief transfer markers into a collection, worker function
  int transferMarkersWorker(LogicalCollection* collection,
                            MMFilesCollectorCache*,
                            MMFilesOperationsType const&);

  /// @brief sync the active journal of a collection
  int syncJournalCollection(LogicalCollection* collection);

  /// @brief get the next free position for a new marker of the specified size
  char* nextFreeMarkerPosition(LogicalCollection* collection, TRI_voc_tick_t,
                               MMFilesMarkerType, TRI_voc_size_t,
                               MMFilesCollectorCache*);

  /// @brief set the tick of a marker and calculate its CRC value
  void finishMarker(char const*, char*, LogicalCollection* collection,
                    TRI_voc_tick_t, MMFilesCollectorCache*);

  void verifyDirectories();
  std::vector<std::string> getDatabaseNames() const;

  /// @brief create a new database directory
  int createDatabaseDirectory(TRI_voc_tick_t id, std::string const& name);

  /// @brief save a parameter.json file for a database
  int saveDatabaseParameters(TRI_voc_tick_t id, std::string const& name,
                             bool deleted);

  arangodb::velocypack::Builder databaseToVelocyPack(TRI_voc_tick_t databaseId,
                                                     std::string const& name,
                                                     bool deleted) const;

  std::string databaseDirectory(TRI_voc_tick_t databaseId) const;
  std::string databaseParametersFilename(TRI_voc_tick_t databaseId) const;
  std::string collectionDirectory(TRI_voc_tick_t databaseId,
                                  TRI_voc_cid_t collectionId) const;
  std::string collectionParametersFilename(TRI_voc_tick_t databaseId,
                                           TRI_voc_cid_t collectionId) const;
  std::string viewParametersFilename(TRI_voc_tick_t databaseId,
                                     TRI_voc_cid_t viewId) const;
  std::string indexFilename(TRI_voc_tick_t databaseId,
                            TRI_voc_cid_t collectionId,
                            TRI_idx_iid_t indexId) const;
  std::string indexFilename(TRI_idx_iid_t indexId) const;

  /// @brief open an existing database. internal function
  TRI_vocbase_t* openExistingDatabase(TRI_voc_tick_t id,
                                      std::string const& name,
                                      bool wasCleanShutdown, bool isUpgrade);

  /// @brief note the maximum local tick
  void noteTick(TRI_voc_tick_t tick) {
    if (tick > _maxTick) {
      _maxTick = tick;
    }
  }

  /// @brief physically erases the database directory
  int dropDatabaseDirectory(std::string const& path);

  /// @brief iterate over a set of datafiles, identified by filenames
  /// note: the files will be opened and closed
  bool iterateFiles(std::vector<std::string> const& files);

  /// @brief find the maximum tick in a collection's journals
  bool findMaxTickInJournals(std::string const& path);

  /// @brief create a full directory name for a collection
  std::string createCollectionDirectoryName(std::string const& basePath,
                                            TRI_voc_cid_t cid);

  void registerCollectionPath(TRI_voc_tick_t databaseId, TRI_voc_cid_t id,
                              std::string const& path);
  void unregisterCollectionPath(TRI_voc_tick_t databaseId, TRI_voc_cid_t id);
  void registerViewPath(TRI_voc_tick_t databaseId, TRI_voc_cid_t id,
                        std::string const& path);
  void unregisterViewPath(TRI_voc_tick_t databaseId, TRI_voc_cid_t id);

  void saveCollectionInfo(TRI_vocbase_t* vocbase, TRI_voc_cid_t id,
                          arangodb::LogicalCollection const* parameters,
                          bool forceSync) const;

  arangodb::velocypack::Builder loadCollectionInfo(TRI_vocbase_t* vocbase,
                                                   std::string const& path);
  arangodb::velocypack::Builder loadViewInfo(TRI_vocbase_t* vocbase,
                                             std::string const& path);

  // start the cleanup thread for the database
  int startCleanup(TRI_vocbase_t* vocbase);
  // stop and delete the cleanup thread for the database
  int stopCleanup(TRI_vocbase_t* vocbase);

  // start the compactor thread for the database
  int startCompactor(TRI_vocbase_t* vocbase);
  // signal the compactor thread to stop
  int beginShutdownCompactor(TRI_vocbase_t* vocbase);
  // stop and delete the compactor thread for the database
  int stopCompactor(TRI_vocbase_t* vocbase);

  /// @brief writes a drop-database marker into the log
  int writeDropMarker(TRI_voc_tick_t id);

 public:
  static std::string const EngineName;
  static std::string const FeatureName;

 private:
  std::string _basePath;
  std::string _databasePath;
  bool _isUpgrade;
  TRI_voc_tick_t _maxTick;
  std::vector<std::pair<std::string, std::string>> _deleted;

  arangodb::basics::ReadWriteLock mutable _pathsLock;
  std::unordered_map<TRI_voc_tick_t,
                     std::unordered_map<TRI_voc_cid_t, std::string>>
      _collectionPaths;
  std::unordered_map<TRI_voc_tick_t,
                     std::unordered_map<TRI_voc_cid_t, std::string>>
      _viewPaths;

  struct CompactionBlocker {
    CompactionBlocker(TRI_voc_tick_t id, double expires)
        : _id(id), _expires(expires) {}
    CompactionBlocker() = delete;

    TRI_voc_tick_t _id;
    double _expires;
  };

  // lock for compaction blockers
  arangodb::basics::ReadWriteLock mutable _compactionBlockersLock;
  // cross-database map of compaction blockers, protected by
  // _compactionBlockersLock
  std::unordered_map<TRI_vocbase_t*, std::vector<CompactionBlocker>>
      _compactionBlockers;

  // lock for threads
  arangodb::Mutex _threadsLock;
  // per-database compactor threads, protected by _threadsLock
  std::unordered_map<TRI_vocbase_t*, MMFilesCompactorThread*> _compactorThreads;
  // per-database cleanup threads, protected by _threadsLock
  std::unordered_map<TRI_vocbase_t*, MMFilesCleanupThread*> _cleanupThreads;
};
}

#endif
