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
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_STORAGE_ENGINE_OTHER_ENGINE_H
#define ARANGOD_STORAGE_ENGINE_OTHER_ENGINE_H 1

#include "Basics/Common.h"
#include "StorageEngine/StorageEngine.h"

namespace arangodb {

class OtherEngine final : public StorageEngine {
 public:

  // create the storage engine
  explicit OtherEngine(application_features::ApplicationServer*);

  ~OtherEngine();

  // inherited from ApplicationFeature
  // ---------------------------------
  
  // add the storage engine's specifc options to the global list of options
  void collectOptions(std::shared_ptr<options::ProgramOptions>) override;
  
  // validate the storage engine's specific options
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override;

  // preparation phase for storage engine. can be used for internal setup.
  // the storage engine must not start any threads here or write any files
  void prepare() override;
  
  // create storage-engine specific collection
  PhysicalCollection* createPhysicalCollection(LogicalCollection*) override;
  
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

  // fill the Builder object with an array of collections (and their corresponding
  // indexes) that were detected by the storage engine. called at server start only
  int getCollectionsAndIndexes(TRI_vocbase_t* vocbase, arangodb::velocypack::Builder& result, bool, bool) override;
  
  // determine the maximum revision id previously handed out by the storage
  // engine. this value is used as a lower bound for further HLC values handed out by
  // the server. called at server start only, after getDatabases() and getCollectionsAndIndexes()
  uint64_t getMaxRevision() override;
  
  // return the path for a database
  std::string databasePath(TRI_vocbase_t const*) const override { return "none"; }
  
  // return the path for a collection
  std::string collectionPath(TRI_vocbase_t const*, TRI_voc_cid_t) const override { return "none"; }
  
  TRI_vocbase_t* openDatabase(arangodb::velocypack::Slice const& parameters, bool isUpgrade) override { 
    return nullptr; 
  }

  // database, collection and index management
  // -----------------------------------------

  // asks the storage engine to create a database as specified in the VPack
  // Slice object and persist the creation info. It is guaranteed by the server that 
  // no other active database with the same name and id exists when this function
  // is called. If this operation fails somewhere in the middle, the storage 
  // engine is required to fully clean up the creation and throw only then, 
  // so that subsequent database creation requests will not fail.
  // the WAL entry for the database creation will be written *after* the call
  // to "createDatabase" returns
  TRI_vocbase_t* createDatabase(TRI_voc_tick_t id, arangodb::velocypack::Slice const& data) override;

  // asks the storage engine to drop the specified database and persist the 
  // deletion info. Note that physical deletion of the database data must not 
  // be carried out by this call, as there may still be readers of the database's data. 
  // It is recommended that this operation only sets a deletion flag for the database 
  // but let's an async task perform the actual deletion. 
  // the WAL entry for database deletion will be written *after* the call
  // to "prepareDropDatabase" returns
  int prepareDropDatabase(TRI_vocbase_t* vocbase) override;
  
  // perform a physical deletion of the database      
  int dropDatabase(TRI_vocbase_t* vocbase) override;
  
  /// @brief wait until a database directory disappears
  int waitUntilDeletion(TRI_voc_tick_t id, bool force) override;

  // asks the storage engine to create a collection as specified in the VPack
  // Slice object and persist the creation info. It is guaranteed by the server 
  // that no other active collection with the same name and id exists in the same
  // database when this function is called. If this operation fails somewhere in 
  // the middle, the storage engine is required to fully clean up the creation 
  // and throw only then, so that subsequent collection creation requests will not fail.
  // the WAL entry for the collection creation will be written *after* the call
  // to "createCollection" returns
  std::string createCollection(TRI_vocbase_t* vocbase, TRI_voc_cid_t id,
                               arangodb::LogicalCollection const* parameters) override;

  // asks the storage engine to drop the specified collection and persist the 
  // deletion info. Note that physical deletion of the collection data must not 
  // be carried out by this call, as there may
  // still be readers of the collection's data. It is recommended that this operation
  // only sets a deletion flag for the collection but let's an async task perform
  // the actual deletion.
  // the WAL entry for collection deletion will be written *after* the call
  // to "dropCollection" returns
  void prepareDropCollection(TRI_vocbase_t* vocbase,
                             arangodb::LogicalCollection* collection) override;

  // perform a physical deletion of the collection
  void dropCollection(TRI_vocbase_t* vocbase,
                      arangodb::LogicalCollection* collection) override;

  // asks the storage engine to change properties of the collection as specified in 
  // the VPack Slice object and persist them. If this operation fails 
  // somewhere in the middle, the storage engine is required to fully revert the 
  // property changes and throw only then, so that subsequent operations will not fail.
  // the WAL entry for the propery change will be written *after* the call
  // to "changeCollection" returns
  void changeCollection(TRI_vocbase_t* vocbase, TRI_voc_cid_t id,
                        arangodb::LogicalCollection const* parameters,
                        bool doSync) override;
  
  // asks the storage engine to create an index as specified in the VPack
  // Slice object and persist the creation info. The database id, collection id 
  // and index data are passed in the Slice object. Note that this function
  // is not responsible for inserting the individual documents into the index.
  // If this operation fails somewhere in the middle, the storage engine is required 
  // to fully clean up the creation and throw only then, so that subsequent index 
  // creation requests will not fail.
  // the WAL entry for the index creation will be written *after* the call
  // to "createIndex" returns
  void createIndex(TRI_vocbase_t* vocbase, TRI_voc_cid_t collectionId,
                   TRI_idx_iid_t id, arangodb::velocypack::Slice const& data) override;

  // asks the storage engine to drop the specified index and persist the deletion 
  // info. Note that physical deletion of the index must not be carried out by this call, 
  // as there may still be users of the index. It is recommended that this operation
  // only sets a deletion flag for the index but let's an async task perform
  // the actual deletion.
  // the WAL entry for index deletion will be written *after* the call
  // to "dropIndex" returns
  void dropIndex(TRI_vocbase_t* vocbase, TRI_voc_cid_t collectionId,
                 TRI_idx_iid_t id) override;
  
  void unloadCollection(TRI_vocbase_t* vocbase, TRI_voc_cid_t collectionId) override {}
  
  void signalCleanup(TRI_vocbase_t* vocbase) override {}

  // document operations
  // -------------------

  // iterate all documents of the underlying collection
  // this is called when a collection is openend, and all its documents need to be added to
  // indexes etc.
  void iterateDocuments(TRI_voc_tick_t databaseId, TRI_voc_cid_t collectionId,
                        std::function<void(arangodb::velocypack::Slice const&)> const& cb) override;


  // adds a document to the storage engine
  // this will be called by the WAL collector when surviving documents are being moved
  // into the storage engine's realm
  void addDocumentRevision(TRI_voc_tick_t databaseId, TRI_voc_cid_t collectionId,
                           arangodb::velocypack::Slice const& document) override;
  
  // removes a document from the storage engine
  // this will be called by the WAL collector when non-surviving documents are being removed
  // from the storage engine's realm
  void removeDocumentRevision(TRI_voc_tick_t databaseId, TRI_voc_cid_t collectionId,
                              arangodb::velocypack::Slice const& document) override;
  
  /// @brief remove data of expired compaction blockers
  bool cleanupCompactionBlockers(TRI_vocbase_t* vocbase) override { return false; }

  /// @brief insert a compaction blocker
  int insertCompactionBlocker(TRI_vocbase_t* vocbase, double ttl, TRI_voc_tick_t& id) override { 
    id = 0; 
    return TRI_ERROR_NO_ERROR; 
  }

  /// @brief touch an existing compaction blocker
  int extendCompactionBlocker(TRI_vocbase_t* vocbase, TRI_voc_tick_t id, double ttl) override {
    return TRI_ERROR_NO_ERROR; 
  }

  /// @brief remove an existing compaction blocker
  int removeCompactionBlocker(TRI_vocbase_t* vocbase, TRI_voc_tick_t id) override {
    return TRI_ERROR_NO_ERROR; 
  }
  
  /// @brief a callback function that is run while it is guaranteed that there is no compaction ongoing
  void preventCompaction(TRI_vocbase_t* vocbase,
                         std::function<void(TRI_vocbase_t*)> const& callback) override {
  }
  
  /// @brief a callback function that is run there is no compaction ongoing
  bool tryPreventCompaction(TRI_vocbase_t* vocbase,
                            std::function<void(TRI_vocbase_t*)> const& callback,
                            bool checkForActiveBlockers) override {
    return true;
  }
  
  int shutdownDatabase(TRI_vocbase_t* vocbase) override { 
    return TRI_ERROR_NO_ERROR;
  }

  int openCollection(TRI_vocbase_t* vocbase, LogicalCollection* collection, bool ignoreErrors) override {
    return TRI_ERROR_NO_ERROR;
  }
  
  /// @brief transfer markers into a collection
  int transferMarkers(LogicalCollection* collection, wal::CollectorCache*,
                      wal::OperationsType const&) override {
    return TRI_ERROR_NO_ERROR;
  }

 public:
  static std::string const EngineName;
};

}

#endif
