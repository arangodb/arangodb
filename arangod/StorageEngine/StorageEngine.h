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
#include "VocBase/voc-types.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

struct TRI_vocbase_t;

namespace arangodb {

class StorageEngine : public application_features::ApplicationFeature {
 public:

  // create the storage engine
  StorageEngine(application_features::ApplicationServer* server, std::string const& name) 
      : application_features::ApplicationFeature(server, name) {
 
    // each specific storage engine feature is optional. the storage engine selection feature
    // will make sure that exactly one engine is selected at startup 
    setOptional(true);
    // storage engines must not use elevated privileges for files etc
    requiresElevatedPrivileges(false);
    // TODO: determine more sensible startup order for storage engine
    startsAfter("EngineSelector");
    startsAfter("LogfileManager");
  }

  // these methods must not be used in the storage engine implementations
  void start() override final {}
  void stop() override final {}

  virtual void initialize() {}
  virtual void shutdown() {}

  // status functionality
  // --------------------

  // return the name of the storage engine
  virtual char const* typeName() const = 0;
  
  // inventory functionality
  // -----------------------

  // fill the Builder object with an array of databases that were detected
  // by the storage engine. this method must sort out databases that were not
  // fully created (see "createDatabase" below). called at server start only
  virtual void getDatabases(arangodb::velocypack::Builder& result) = 0;

  // fill the Builder object with an array of collections (and their corresponding
  // indexes) that were detected by the storage engine. called at server start only
  virtual void getCollectionsAndIndexes(arangodb::velocypack::Builder& result) = 0;

  // determine the maximum revision id previously handed out by the storage
  // engine. this value is used as a lower bound for further HLC values handed out by
  // the server. called at server start only, after getDatabases() and getCollectionsAndIndexes()
  virtual uint64_t getMaxRevision() = 0;

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
  virtual void createDatabase(TRI_voc_tick_t id, arangodb::velocypack::Slice const& data) = 0;

  // asks the storage engine to drop the specified database and persist the 
  // deletion info. Note that physical deletion of the database data must not 
  // be carried out by this call, as there may
  // still be readers of the database's data. It is recommended that this operation
  // only sets a deletion flag for the database but let's an async task perform
  // the actual deletion. The async task can later call the callback function to 
  // check whether the physical deletion of the database is possible.
  // the WAL entry for database deletion will be written *after* the call
  // to "dropDatabase" returns
  virtual void dropDatabase(TRI_voc_tick_t id, std::function<bool()> const& canRemovePhysically) = 0;

  // asks the storage engine to create a collection as specified in the VPack
  // Slice object and persist the creation info. It is guaranteed by the server 
  // that no other active collection with the same name and id exists in the same
  // database when this function is called. If this operation fails somewhere in 
  // the middle, the storage engine is required to fully clean up the creation 
  // and throw only then, so that subsequent collection creation requests will not fail.
  // the WAL entry for the collection creation will be written *after* the call
  // to "createCollection" returns
  virtual void createCollection(TRI_voc_tick_t databaseId, TRI_voc_cid_t id,
                                arangodb::velocypack::Slice const& data) = 0;

  // asks the storage engine to drop the specified collection and persist the 
  // deletion info. Note that physical deletion of the collection data must not 
  // be carried out by this call, as there may
  // still be readers of the collection's data. It is recommended that this operation
  // only sets a deletion flag for the collection but let's an async task perform
  // the actual deletion.
  // the WAL entry for collection deletion will be written *after* the call
  // to "dropCollection" returns
  virtual void dropCollection(TRI_voc_tick_t databaseId, TRI_voc_cid_t id, 
                              std::function<bool()> const& canRemovePhysically) = 0;
  
  // asks the storage engine to rename the collection as specified in the VPack
  // Slice object and persist the renaming info. It is guaranteed by the server 
  // that no other active collection with the same name and id exists in the same
  // database when this function is called. If this operation fails somewhere in 
  // the middle, the storage engine is required to fully revert the rename operation
  // and throw only then, so that subsequent collection creation/rename requests will 
  // not fail. the WAL entry for the rename will be written *after* the call
  // to "renameCollection" returns
  virtual void renameCollection(TRI_voc_tick_t databaseId, TRI_voc_cid_t id,
                                arangodb::velocypack::Slice const& data) = 0;
  
  // asks the storage engine to change properties of the collection as specified in 
  // the VPack Slice object and persist them. If this operation fails 
  // somewhere in the middle, the storage engine is required to fully revert the 
  // property changes and throw only then, so that subsequent operations will not fail.
  // the WAL entry for the propery change will be written *after* the call
  // to "changeCollection" returns
  virtual void changeCollection(TRI_voc_tick_t databaseId, TRI_voc_cid_t id,
                                arangodb::velocypack::Slice const& data) = 0;
  
  // asks the storage engine to create an index as specified in the VPack
  // Slice object and persist the creation info. The database id, collection id 
  // and index data are passed in the Slice object. Note that this function
  // is not responsible for inserting the individual documents into the index.
  // If this operation fails somewhere in the middle, the storage engine is required 
  // to fully clean up the creation and throw only then, so that subsequent index 
  // creation requests will not fail.
  // the WAL entry for the index creation will be written *after* the call
  // to "createIndex" returns
  virtual void createIndex(TRI_voc_tick_t databaseId, TRI_voc_cid_t collectionId,
                           TRI_idx_iid_t id, arangodb::velocypack::Slice const& data) = 0;

  // asks the storage engine to drop the specified index and persist the deletion 
  // info. Note that physical deletion of the index must not be carried out by this call, 
  // as there may still be users of the index. It is recommended that this operation
  // only sets a deletion flag for the index but let's an async task perform
  // the actual deletion.
  // the WAL entry for index deletion will be written *after* the call
  // to "dropIndex" returns
  virtual void dropIndex(TRI_voc_tick_t databaseId, TRI_voc_cid_t collectionId,
                         TRI_idx_iid_t id) = 0;

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
};

}

#endif
