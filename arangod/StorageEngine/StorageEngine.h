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

#ifndef ARANGOD_STORAGE_ENGINE_STORAGE_ENGINE_H
#define ARANGOD_STORAGE_ENGINE_STORAGE_ENGINE_H 1

#include "ApplicationFeatures/ApplicationFeature.h"
#include "Basics/Common.h"
#include "Basics/Result.h"
#include "Indexes/IndexFactory.h"
#include "VocBase/AccessMode.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

#include <chrono>

namespace arangodb {

enum class RecoveryState : uint32_t {
  /// @brief recovery is not yet started
  BEFORE = 0,

  /// @brief recovery is in progress
  IN_PROGRESS,

  /// @brief recovery is done
  DONE
};

class DatabaseInitialSyncer;
class LogicalCollection;
class LogicalView;
class PhysicalCollection;
class PhysicalView;
class Result;
class TransactionCollection;
class TransactionState;
class WalAccess;

namespace rest {
class RestHandlerFactory;
}

namespace transaction {

class Context;
class ContextData;
class Manager;
struct Options;

}  // namespace transaction

class StorageEngine : public application_features::ApplicationFeature {
 public:
  // create the storage engine
  StorageEngine(application_features::ApplicationServer& server,
                std::string const& engineName, std::string const& featureName,
                std::unique_ptr<IndexFactory>&& indexFactory)
      : application_features::ApplicationFeature(server, featureName),
        _indexFactory(std::move(indexFactory)),
        _typeName(engineName) {
    // each specific storage engine feature is optional. the storage engine
    // selection feature will make sure that exactly one engine is selected at
    // startup
    setOptional(true);
    // storage engines must not use elevated privileges for files etc

    startsAfter("BasicsPhase");
    startsAfter("CacheManager");
    startsBefore("StorageEngine");
    startsAfter("TransactionManager");
    startsAfter("ViewTypes");
  }

  virtual bool supportsDfdb() const = 0;

  virtual std::unique_ptr<transaction::Manager> createTransactionManager() = 0;
  virtual std::unique_ptr<transaction::ContextData> createTransactionContextData() = 0;
  virtual std::unique_ptr<TransactionState> createTransactionState(
      TRI_vocbase_t& vocbase, TRI_voc_tid_t, transaction::Options const& options) = 0;
  virtual std::unique_ptr<TransactionCollection> createTransactionCollection(
      TransactionState& state, TRI_voc_cid_t cid, AccessMode::Type accessType,
      int nestingLevel) = 0;

  // when a new collection is created, this method is called to augment the
  // collection creation data with engine-specific information
  virtual void addParametersForNewCollection(VPackBuilder&, VPackSlice /*info*/) {}

  // create storage-engine specific collection
  virtual std::unique_ptr<PhysicalCollection> createPhysicalCollection(
      LogicalCollection& collection, velocypack::Slice const& info) = 0;

  // minimum timeout for the synchronous replication
  virtual double minimumSyncReplicationTimeout() const = 0;

  // status functionality
  // --------------------

  // return the name of the specific storage engine e.g. rocksdb
  virtual std::string const& typeName() const { return _typeName; }

  // inventory functionality
  // -----------------------

  // fill the Builder object with an array of databases that were detected
  // by the storage engine. this method must sort out databases that were not
  // fully created (see "createDatabase" below). called at server start only
  virtual void getDatabases(arangodb::velocypack::Builder& result) = 0;

  // fills the provided builder with information about the collection
  virtual void getCollectionInfo(TRI_vocbase_t& vocbase, TRI_voc_cid_t cid,
                                 arangodb::velocypack::Builder& result,
                                 bool includeIndexes, TRI_voc_tick_t maxTick) = 0;

  // fill the Builder object with an array of collections (and their
  // corresponding indexes) that were detected by the storage engine. called at
  // server start separately for each database
  virtual int getCollectionsAndIndexes(TRI_vocbase_t& vocbase,
                                       arangodb::velocypack::Builder& result,
                                       bool wasCleanShutdown, bool isUpgrade) = 0;

  virtual int getViews(TRI_vocbase_t& vocbase, arangodb::velocypack::Builder& result) = 0;

  // return the absolute path for the VERSION file of a database
  virtual std::string versionFilename(TRI_voc_tick_t id) const = 0;

  // return the path for the actual data
  virtual std::string dataPath() const = 0;

  // return the path for a database
  virtual std::string databasePath(TRI_vocbase_t const* vocbase) const = 0;

  // return the path for a collection
  virtual std::string collectionPath(TRI_vocbase_t const& vocbase,
                                     TRI_voc_cid_t id) const = 0;

  // database, collection and index management
  // -----------------------------------------

  // if not stated other wise functions may throw and the caller has to take
  // care of error handling the return values will be the usual  TRI_ERROR_*
  // codes.

  virtual void waitForSyncTick(TRI_voc_tick_t tick) = 0;

  /// @brief return a list of the currently open WAL files
  virtual std::vector<std::string> currentWalFiles() const = 0;

  virtual Result flushWal(bool waitForSync = false, bool waitForCollector = false,
                          bool writeShutdownFile = false) = 0;

  virtual void waitForEstimatorSync(std::chrono::milliseconds maxWaitTime) = 0;

  //// operations on databases

  /// @brief opens a database
  virtual std::unique_ptr<TRI_vocbase_t> openDatabase(arangodb::velocypack::Slice const& args,
                                                      bool isUpgrade, int& status) = 0;
  std::unique_ptr<TRI_vocbase_t> openDatabase(velocypack::Slice const& args, bool isUpgrade) {
    int status;
    auto rv = openDatabase(args, isUpgrade, status);
    TRI_ASSERT(status == TRI_ERROR_NO_ERROR);
    TRI_ASSERT(rv != nullptr);
    return rv;
  }

  // asks the storage engine to create a database as specified in the VPack
  // Slice object and persist the creation info. It is guaranteed by the server
  // that no other active database with the same name and id exists when this
  // function is called. If this operation fails somewhere in the middle, the
  // storage engine is required to fully clean up the creation and throw only
  // then, so that subsequent database creation requests will not fail. the WAL
  // entry for the database creation will be written *after* the call to
  // "createDatabase" returns no way to acquire id within this function?!
  virtual std::unique_ptr<TRI_vocbase_t> createDatabase(TRI_voc_tick_t id,
                                                        velocypack::Slice const& args,
                                                        int& status) = 0;

  // @brief write create marker for database
  virtual int writeCreateDatabaseMarker(TRI_voc_tick_t id, VPackSlice const& slice) = 0;

  // asks the storage engine to drop the specified database and persist the
  // deletion info. Note that physical deletion of the database data must not
  // be carried out by this call, as there may still be readers of the
  // database's data. It is recommended that this operation only sets a deletion
  // flag for the database but let's an async task perform the actual deletion.
  // the WAL entry for database deletion will be written *after* the call
  // to "prepareDropDatabase" returns
  //
  // is done under a lock in database feature
  virtual void prepareDropDatabase(TRI_vocbase_t& vocbase, bool useWriteMarker,
                                   int& status) = 0;
  void prepareDropDatabase(TRI_vocbase_t& db, bool useWriteMarker) {
    int status = 0;
    prepareDropDatabase(db, useWriteMarker, status);
    TRI_ASSERT(status == TRI_ERROR_NO_ERROR);
  };

  // perform a physical deletion of the database
  virtual Result dropDatabase(TRI_vocbase_t& database) = 0;

  /// @brief wait until a database directory disappears - not under lock in
  /// databaseFreature
  virtual void waitUntilDeletion(TRI_voc_tick_t id, bool force, int& status) = 0;

  /// @brief is database in recovery
  bool inRecovery() { return recoveryState() < RecoveryState::DONE; }

  /// @brief current recovery state
  virtual RecoveryState recoveryState() = 0;

  /// @brief current recovery tick
  virtual TRI_voc_tick_t recoveryTick() = 0;

  /// @brief function to be run when recovery is done
  virtual void recoveryDone(TRI_vocbase_t& /*vocbase*/) {}

  //// Operations on Collections
  // asks the storage engine to create a collection as specified in the VPack
  // Slice object and persist the creation info. It is guaranteed by the server
  // that no other active collection with the same name and id exists in the
  // same database when this function is called. If this operation fails
  // somewhere in the middle, the storage engine is required to fully clean up
  // the creation and throw only then, so that subsequent collection creation
  // requests will not fail. the WAL entry for the collection creation will be
  // written *after* the call to "createCollection" returns
  virtual std::string createCollection(TRI_vocbase_t& vocbase,
                                       LogicalCollection const& collection) = 0;

  // asks the storage engine to persist the collection.
  // After this call the collection is persisted over recovery.
  virtual arangodb::Result persistCollection(TRI_vocbase_t& vocbase,
                                             LogicalCollection const& collection) = 0;

  // asks the storage engine to drop the specified collection and persist the
  // deletion info. Note that physical deletion of the collection data must not
  // be carried out by this call, as there may
  // still be readers of the collection's data. It is recommended that this
  // operation only sets a deletion flag for the collection but lets an async
  // task perform the actual deletion. the WAL entry for collection deletion
  // will be written *after* the call to "dropCollection" returns
  virtual arangodb::Result dropCollection(TRI_vocbase_t& vocbase,
                                          LogicalCollection& collection) = 0;

  // perform a physical deletion of the collection
  // After this call data of this collection is corrupted, only perform if
  // assured that no one is using the collection anymore
  virtual void destroyCollection(TRI_vocbase_t& vocbase, LogicalCollection& collection) = 0;

  // asks the storage engine to change properties of the collection as specified
  // in the VPack Slice object and persist them. If this operation fails
  // somewhere in the middle, the storage engine is required to fully revert the
  // property changes and throw only then, so that subsequent operations will
  // not fail. the WAL entry for the propery change will be written *after* the
  // call to "changeCollection" returns
  virtual void changeCollection(TRI_vocbase_t& vocbase,
                                LogicalCollection const& collection, bool doSync) = 0;

  // asks the storage engine to persist renaming of a collection
  virtual arangodb::Result renameCollection(TRI_vocbase_t& vocbase,
                                            LogicalCollection const& collection,
                                            std::string const& oldName) = 0;

  // asks the storage engine to change properties of the view as specified in
  // the VPack Slice object and persist them. If this operation fails
  // somewhere in the middle, the storage engine is required to fully revert the
  // property changes and throw only then, so that subsequent operations will
  // not fail. the WAL entry for the propery change will be written *after* the
  // call to "changeView" returns
  virtual arangodb::Result changeView(TRI_vocbase_t& vocbase,
                                      arangodb::LogicalView const& view, bool doSync) = 0;

  //// Operations on Views
  // asks the storage engine to create a view as specified in the VPack
  // Slice object and persist the creation info. It is guaranteed by the server
  // that no other active view with the same name and id exists in the same
  // database when this function is called. If this operation fails somewhere in
  // the middle, the storage engine is required to fully clean up the creation
  // and throw only then, so that subsequent view creation requests will not
  // fail. the WAL entry for the view creation will be written *after* the call
  // to "createCview" returns
  virtual arangodb::Result createView(TRI_vocbase_t& vocbase, TRI_voc_cid_t id,
                                      arangodb::LogicalView const& view) = 0;

  // asks storage engine to put some view
  // specific properties into a specified builder
  virtual void getViewProperties(TRI_vocbase_t& vocbase, LogicalView const& view,
                                 VPackBuilder& builder) = 0;

  // asks the storage engine to drop the specified view and persist the
  // deletion info. Note that physical deletion of the view data must not
  // be carried out by this call, as there may
  // still be readers of the view's data. It is recommended that this operation
  // only sets a deletion flag for the view but lets an async task perform
  // the actual deletion.
  // the WAL entry for view deletion will be written *after* the call
  // to "dropView" returns
  virtual arangodb::Result dropView(TRI_vocbase_t const& vocbase,
                                    LogicalView const& view) = 0;

  // perform a physical deletion of the view
  // After this call data of this view is corrupted, only perform if
  // assured that no one is using the view anymore
  // 'noexcept' becuase it may be used in destructor
  virtual void destroyView(TRI_vocbase_t const& vocbase, LogicalView const& view) noexcept = 0;

  // Returns the StorageEngine-specific implementation
  // of the IndexFactory. This is used to validate
  // information about indexes.
  IndexFactory const& indexFactory() const {
    // The factory has to be created by the implementation
    // and shall never be deleted
    TRI_ASSERT(_indexFactory.get() != nullptr);
    return *_indexFactory;
  }

  virtual void unloadCollection(TRI_vocbase_t& vocbase, LogicalCollection& collection) = 0;

  virtual void signalCleanup(TRI_vocbase_t& vocbase) = 0;

  virtual int shutdownDatabase(TRI_vocbase_t& vocbase) = 0;

  // AQL functions
  // -------------

  /// @brief Add engine-specific optimizer rules
  virtual void addOptimizerRules() {}

  /// @brief Add engine-specific V8 functions
  virtual void addV8Functions() {}

  /// @brief Add engine-specific REST handlers
  virtual void addRestHandlers(rest::RestHandlerFactory& handlerFactory) {}

  // replication
  virtual void cleanupReplicationContexts() = 0;

  virtual velocypack::Builder getReplicationApplierConfiguration(TRI_vocbase_t& vocbase,
                                                                 int& status) = 0;
  virtual arangodb::velocypack::Builder getReplicationApplierConfiguration(int&) = 0;

  virtual int removeReplicationApplierConfiguration(TRI_vocbase_t& vocbase) = 0;
  virtual int removeReplicationApplierConfiguration() = 0;

  virtual int saveReplicationApplierConfiguration(TRI_vocbase_t& vocbase,
                                                  velocypack::Slice slice,
                                                  bool doSync) = 0;
  virtual int saveReplicationApplierConfiguration(velocypack::Slice slice, bool doSync) = 0;

  virtual Result handleSyncKeys(DatabaseInitialSyncer& syncer, LogicalCollection& col,
                                std::string const& keysId) = 0;
  virtual Result createLoggerState(TRI_vocbase_t* vocbase, velocypack::Builder& builder) = 0;
  virtual Result createTickRanges(velocypack::Builder& builder) = 0;
  virtual Result firstTick(uint64_t& tick) = 0;
  virtual Result lastLogger(TRI_vocbase_t& vocbase,
                            std::shared_ptr<transaction::Context> transactionContext,
                            uint64_t tickStart, uint64_t tickEnd,
                            std::shared_ptr<velocypack::Builder>& builderSPtr) = 0;
  virtual WalAccess const* walAccess() const = 0;

  virtual bool useRawDocumentPointers() = 0;

  void getCapabilities(velocypack::Builder& builder) const {
    builder.openObject();
    builder.add("name", velocypack::Value(typeName()));
    builder.add("supports", velocypack::Value(VPackValueType::Object));
    builder.add("dfdb", velocypack::Value(supportsDfdb()));
    
    builder.add("indexes", velocypack::Value(VPackValueType::Array));
    for (auto const& it : indexFactory().supportedIndexes()) {
      builder.add(velocypack::Value(it));
    }
    builder.close();  // indexes
    
    builder.add("aliases", velocypack::Value(VPackValueType::Object));
    builder.add("indexes", velocypack::Value(VPackValueType::Object));
    for (auto const& it : indexFactory().indexAliases()) {
      builder.add(it.first, velocypack::Value(it.second));
    }
    builder.close();  // indexes
    builder.close();  // aliases
    
    builder.close();  // supports
    builder.close();  // object
  }

  virtual void getStatistics(VPackBuilder& builder) const {
    builder.openObject();
    builder.close();
  }

  // management methods for synchronizing with external persistent stores
  virtual TRI_voc_tick_t currentTick() const = 0;
  virtual TRI_voc_tick_t releasedTick() const = 0;
  virtual void releaseTick(TRI_voc_tick_t) = 0;

 protected:
  void registerCollection(TRI_vocbase_t& vocbase,
                          std::shared_ptr<arangodb::LogicalCollection> const& collection) {
    vocbase.registerCollection(true, collection);
  }

  void registerView(TRI_vocbase_t& vocbase,
                    std::shared_ptr<arangodb::LogicalView> const& view) {
    vocbase.registerView(true, view);
  }

 private:
  std::unique_ptr<IndexFactory> const _indexFactory;
  std::string const _typeName;
};

}  // namespace arangodb

#endif
