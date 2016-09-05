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

#ifndef ARANGOD_STORAGE_ENGINE_STORAGE_ENGINE_H
#define ARANGOD_STORAGE_ENGINE_STORAGE_ENGINE_H 1

#include "Basics/Common.h"
#include "ApplicationFeatures/ApplicationFeature.h"
#include "VocBase/datafile.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"
#include "Wal/CollectorCache.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

namespace arangodb {
class LogicalCollection;
class PhysicalCollection;

class StorageEngine : public application_features::ApplicationFeature {
 public:

  // create the storage engine
  StorageEngine(application_features::ApplicationServer* server, std::string const& name) 
      : application_features::ApplicationFeature(server, name), _typeName(name) {
 
    // each specific storage engine feature is optional. the storage engine selection feature
    // will make sure that exactly one engine is selected at startup 
    setOptional(true);
    // storage engines must not use elevated privileges for files etc
    requiresElevatedPrivileges(false);
    
    startsAfter("DatabasePath");
    startsAfter("EngineSelector");
    startsAfter("FileDescriptors");
    startsAfter("Temp");
  }

  virtual void start() {}
  virtual void stop() {}
  virtual void recoveryDone(TRI_vocbase_t* vocbase) {} 
  
  
  // create storage-engine specific collection
  virtual PhysicalCollection* createPhysicalCollection(LogicalCollection*) = 0;
  

  // status functionality
  // --------------------

  // return the name of the storage engine
  char const* typeName() const { return _typeName.c_str(); }

  // inventory functionality
  // -----------------------

  // fill the Builder object with an array of databases that were detected
  // by the storage engine. this method must sort out databases that were not
  // fully created (see "createDatabase" below). called at server start only
  virtual void getDatabases(arangodb::velocypack::Builder& result) = 0;
 
  // fills the provided builder with information about the collection 
  virtual void getCollectionInfo(TRI_vocbase_t* vocbase, TRI_voc_cid_t cid, 
                                 arangodb::velocypack::Builder& result, 
                                 bool includeIndexes, TRI_voc_tick_t maxTick) = 0;

  // fill the Builder object with an array of collections (and their corresponding
  // indexes) that were detected by the storage engine. called at server start separately
  // for each database
  virtual int getCollectionsAndIndexes(TRI_vocbase_t* vocbase, arangodb::velocypack::Builder& result, 
                                       bool wasCleanShutdown, bool isUpgrade) = 0;
 
  // determine the maximum revision id previously handed out by the storage
  // engine. this value is used as a lower bound for further HLC values handed out by
  // the server. called at server start only, after getDatabases() and getCollectionsAndIndexes()
  virtual uint64_t getMaxRevision() = 0;

  // return the path for a database
  virtual std::string databasePath(TRI_vocbase_t const* vocbase) const = 0;
  
  // return the path for a collection
  virtual std::string collectionPath(TRI_vocbase_t const* vocbase, TRI_voc_cid_t id) const = 0;

  virtual TRI_vocbase_t* openDatabase(arangodb::velocypack::Slice const& parameters, bool isUpgrade) = 0;

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
  virtual TRI_vocbase_t* createDatabase(TRI_voc_tick_t id, arangodb::velocypack::Slice const& data) = 0;

  // asks the storage engine to drop the specified database and persist the 
  // deletion info. Note that physical deletion of the database data must not 
  // be carried out by this call, as there may still be readers of the database's data. 
  // It is recommended that this operation only sets a deletion flag for the database 
  // but let's an async task perform the actual deletion. 
  // the WAL entry for database deletion will be written *after* the call
  // to "prepareDropDatabase" returns
  virtual int prepareDropDatabase(TRI_vocbase_t* vocbase) = 0;
  
  // perform a physical deletion of the database      
  virtual int dropDatabase(TRI_vocbase_t* vocbase) = 0;
  
  /// @brief wait until a database directory disappears
  virtual int waitUntilDeletion(TRI_voc_tick_t id, bool force) = 0;

  // asks the storage engine to create a collection as specified in the VPack
  // Slice object and persist the creation info. It is guaranteed by the server 
  // that no other active collection with the same name and id exists in the same
  // database when this function is called. If this operation fails somewhere in 
  // the middle, the storage engine is required to fully clean up the creation 
  // and throw only then, so that subsequent collection creation requests will not fail.
  // the WAL entry for the collection creation will be written *after* the call
  // to "createCollection" returns
  virtual std::string createCollection(TRI_vocbase_t* vocbase, TRI_voc_cid_t id,
                                       arangodb::LogicalCollection const* parameters) = 0;
  
  // asks the storage engine to drop the specified collection and persist the 
  // deletion info. Note that physical deletion of the collection data must not 
  // be carried out by this call, as there may
  // still be readers of the collection's data. It is recommended that this operation
  // only sets a deletion flag for the collection but let's an async task perform
  // the actual deletion.
  // the WAL entry for collection deletion will be written *after* the call
  // to "dropCollection" returns
  virtual void prepareDropCollection(TRI_vocbase_t* vocbase, arangodb::LogicalCollection* collection) = 0; 
  
  // perform a physical deletion of the collection
  virtual void dropCollection(TRI_vocbase_t* vocbase, arangodb::LogicalCollection* collection) = 0; 
  
  // asks the storage engine to change properties of the collection as specified in 
  // the VPack Slice object and persist them. If this operation fails 
  // somewhere in the middle, the storage engine is required to fully revert the 
  // property changes and throw only then, so that subsequent operations will not fail.
  // the WAL entry for the propery change will be written *after* the call
  // to "changeCollection" returns
  virtual void changeCollection(TRI_vocbase_t* vocbase, TRI_voc_cid_t id,
                                arangodb::LogicalCollection const* parameters,
                                bool doSync) = 0;
  
  // asks the storage engine to create an index as specified in the VPack
  // Slice object and persist the creation info. The database id, collection id 
  // and index data are passed in the Slice object. Note that this function
  // is not responsible for inserting the individual documents into the index.
  // If this operation fails somewhere in the middle, the storage engine is required 
  // to fully clean up the creation and throw only then, so that subsequent index 
  // creation requests will not fail.
  // the WAL entry for the index creation will be written *after* the call
  // to "createIndex" returns
  virtual void createIndex(TRI_vocbase_t* vocbase, TRI_voc_cid_t collectionId,
                           TRI_idx_iid_t id, arangodb::velocypack::Slice const& data) = 0;

  // asks the storage engine to drop the specified index and persist the deletion 
  // info. Note that physical deletion of the index must not be carried out by this call, 
  // as there may still be users of the index. It is recommended that this operation
  // only sets a deletion flag for the index but let's an async task perform
  // the actual deletion.
  // the WAL entry for index deletion will be written *after* the call
  // to "dropIndex" returns
  virtual void dropIndex(TRI_vocbase_t* vocbase, TRI_voc_cid_t collectionId,
                         TRI_idx_iid_t id) = 0;

  virtual void unloadCollection(TRI_vocbase_t* vocbase, TRI_voc_cid_t collectionId) = 0;
  
  virtual void signalCleanup(TRI_vocbase_t* vocbase) = 0;

  // document operations
  // -------------------

  // iterate all documents of the underlying collection
  // this is called when a collection is openend, and all its documents need to be added to
  // indexes etc.
  virtual void iterateDocuments(TRI_voc_tick_t databaseId, TRI_voc_cid_t collectionId,
                                std::function<void(arangodb::velocypack::Slice const&)> const& cb) = 0;


  // adds a document to the storage engine
  // this will be called by the WAL collector when surviving documents are being moved
  // into the storage engine's realm
  virtual void addDocumentRevision(TRI_voc_tick_t databaseId, TRI_voc_cid_t collectionId,
                                   arangodb::velocypack::Slice const& document) = 0;
  
  // removes a document from the storage engine
  // this will be called by the WAL collector when non-surviving documents are being removed
  // from the storage engine's realm
  virtual void removeDocumentRevision(TRI_voc_tick_t databaseId, TRI_voc_cid_t collectionId,
                                      arangodb::velocypack::Slice const& document) = 0;
  
  /// @brief remove data of expired compaction blockers
  virtual bool cleanupCompactionBlockers(TRI_vocbase_t* vocbase) = 0;

  /// @brief insert a compaction blocker
  virtual int insertCompactionBlocker(TRI_vocbase_t* vocbase, double ttl, TRI_voc_tick_t& id) = 0;

  /// @brief touch an existing compaction blocker
  virtual int extendCompactionBlocker(TRI_vocbase_t* vocbase, TRI_voc_tick_t id, double ttl) = 0;

  /// @brief remove an existing compaction blocker
  virtual int removeCompactionBlocker(TRI_vocbase_t* vocbase, TRI_voc_tick_t id) = 0;
  
  /// @brief a callback function that is run while it is guaranteed that there is no compaction ongoing
  virtual void preventCompaction(TRI_vocbase_t* vocbase,
                                 std::function<void(TRI_vocbase_t*)> const& callback) = 0;
  
  /// @brief a callback function that is run there is no compaction ongoing
  virtual bool tryPreventCompaction(TRI_vocbase_t* vocbase,
                                    std::function<void(TRI_vocbase_t*)> const& callback,
                                    bool checkForActiveBlockers) = 0;
  
  virtual int shutdownDatabase(TRI_vocbase_t* vocbase) = 0; 
  
  virtual int openCollection(TRI_vocbase_t* vocbase, LogicalCollection* collection, bool ignoreErrors) = 0;

  /// @brief transfer markers into a collection
  virtual int transferMarkers(LogicalCollection* collection, wal::CollectorCache*,
                              wal::OperationsType const&) = 0;
  
 protected:
  arangodb::LogicalCollection* registerCollection(
      bool doLock, TRI_vocbase_t* vocbase, arangodb::velocypack::Slice params) {
    return vocbase->registerCollection(doLock, params);
  }
 
 private:
  std::string const _typeName;
};

}

#endif
