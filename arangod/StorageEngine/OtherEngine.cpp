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

#include "OtherEngine.h"
#include "StorageEngine/EngineSelectorFeature.h"

using namespace arangodb;

std::string const OtherEngine::EngineName("OtherEngine");

// create the storage engine
OtherEngine::OtherEngine(application_features::ApplicationServer* server)
    : StorageEngine(server, EngineName) {
}

OtherEngine::~OtherEngine() {
}

// add the storage engine's specifc options to the global list of options
void OtherEngine::collectOptions(std::shared_ptr<options::ProgramOptions>) {
}
  
// validate the storage engine's specific options
void OtherEngine::validateOptions(std::shared_ptr<options::ProgramOptions>) {
}

// preparation phase for storage engine. can be used for internal setup.
// the storage engine must not start any threads here or write any files
void OtherEngine::prepare() {
  TRI_ASSERT(EngineSelectorFeature::ENGINE = this);
}
  
// create storage-engine specific collection
PhysicalCollection* OtherEngine::createPhysicalCollection(LogicalCollection*) {
  return nullptr;
}
  
// fill the Builder object with an array of databases that were detected
// by the storage engine. this method must sort out databases that were not
// fully created (see "createDatabase" below). called at server start only
void OtherEngine::getDatabases(arangodb::velocypack::Builder& result) {
}
  
// fills the provided builder with information about the collection 
void OtherEngine::getCollectionInfo(TRI_vocbase_t* vocbase, TRI_voc_cid_t cid, 
                                    arangodb::velocypack::Builder& result, 
                                    bool includeIndexes, TRI_voc_tick_t maxTick) {
}

// fill the Builder object with an array of collections (and their corresponding
// indexes) that were detected by the storage engine. called at server start only
int OtherEngine::getCollectionsAndIndexes(TRI_vocbase_t* vocbase, arangodb::velocypack::Builder& result, bool, bool) {
  return TRI_ERROR_NO_ERROR;
}
  
// determine the maximum revision id previously handed out by the storage
// engine. this value is used as a lower bound for further HLC values handed out by
// the server. called at server start only, after getDatabases() and getCollectionsAndIndexes()
uint64_t OtherEngine::getMaxRevision() {
  return 0; // TODO
}

// asks the storage engine to create a database as specified in the VPack
// Slice object and persist the creation info. It is guaranteed by the server that 
// no other active database with the same name and id exists when this function
// is called. If this operation fails somewhere in the middle, the storage 
// engine is required to fully clean up the creation and throw only then, 
// so that subsequent database creation requests will not fail.
// the WAL entry for the database creation will be written *after* the call
// to "createDatabase" returns
TRI_vocbase_t* OtherEngine::createDatabase(TRI_voc_tick_t id, arangodb::velocypack::Slice const& data) {
  return nullptr;
}

int OtherEngine::prepareDropDatabase(TRI_vocbase_t* vocbase) {
  return TRI_ERROR_NO_ERROR;
}

int OtherEngine::dropDatabase(TRI_vocbase_t* vocbase) {
  return TRI_ERROR_NO_ERROR;
}
  
/// @brief wait until a database directory disappears
int OtherEngine::waitUntilDeletion(TRI_voc_tick_t id, bool force) {
  return TRI_ERROR_NO_ERROR;
}

// asks the storage engine to create a collection as specified in the VPack
// Slice object and persist the creation info. It is guaranteed by the server 
// that no other active collection with the same name and id exists in the same
// database when this function is called. If this operation fails somewhere in 
// the middle, the storage engine is required to fully clean up the creation 
// and throw only then, so that subsequent collection creation requests will not fail.
// the WAL entry for the collection creation will be written *after* the call
// to "createCollection" returns
std::string OtherEngine::createCollection(TRI_vocbase_t* vocbase, TRI_voc_cid_t id,
                                          arangodb::LogicalCollection const* parameters) {
  return "test";
}
  
// asks the storage engine to drop the specified collection and persist the 
// deletion info. Note that physical deletion of the collection data must not 
// be carried out by this call, as there may
// still be readers of the collection's data. It is recommended that this operation
// only sets a deletion flag for the collection but let's an async task perform
// the actual deletion.
// the WAL entry for collection deletion will be written *after* the call
// to "dropCollection" returns
void OtherEngine::prepareDropCollection(TRI_vocbase_t* vocbase, arangodb::LogicalCollection* collection) {
}

// perform a physical deletion of the collection
void OtherEngine::dropCollection(TRI_vocbase_t* vocbase, arangodb::LogicalCollection* collection) {
}

// asks the storage engine to change properties of the collection as specified in 
// the VPack Slice object and persist them. If this operation fails 
// somewhere in the middle, the storage engine is required to fully revert the 
// property changes and throw only then, so that subsequent operations will not fail.
// the WAL entry for the propery change will be written *after* the call
// to "changeCollection" returns
void OtherEngine::changeCollection(TRI_vocbase_t* vocbase, TRI_voc_cid_t id,
                                   arangodb::LogicalCollection const* parameters,
                                   bool doSync) {
}

// asks the storage engine to create an index as specified in the VPack
// Slice object and persist the creation info. The database id, collection id 
// and index data are passed in the Slice object. Note that this function
// is not responsible for inserting the individual documents into the index.
// If this operation fails somewhere in the middle, the storage engine is required 
// to fully clean up the creation and throw only then, so that subsequent index 
// creation requests will not fail.
// the WAL entry for the index creation will be written *after* the call
// to "createIndex" returns
void OtherEngine::createIndex(TRI_vocbase_t* vocbase, TRI_voc_cid_t collectionId,
                              TRI_idx_iid_t id, arangodb::velocypack::Slice const& data) {
}

// asks the storage engine to drop the specified index and persist the deletion 
// info. Note that physical deletion of the index must not be carried out by this call, 
// as there may still be users of the index. It is recommended that this operation
// only sets a deletion flag for the index but let's an async task perform
// the actual deletion.
// the WAL entry for index deletion will be written *after* the call
// to "dropIndex" returns
void OtherEngine::dropIndex(TRI_vocbase_t* vocbase, TRI_voc_cid_t collectionId,
                            TRI_idx_iid_t id) {
}

// iterate all documents of the underlying collection
// this is called when a collection is openend, and all its documents need to be added to
// indexes etc.
void OtherEngine::iterateDocuments(TRI_voc_tick_t databaseId, TRI_voc_cid_t collectionId,
                                   std::function<void(arangodb::velocypack::Slice const&)> const& cb) {
}

// adds a document to the storage engine
// this will be called by the WAL collector when surviving documents are being moved
// into the storage engine's realm
void OtherEngine::addDocumentRevision(TRI_voc_tick_t databaseId, TRI_voc_cid_t collectionId,
                                      arangodb::velocypack::Slice const& document) {
}

// removes a document from the storage engine
// this will be called by the WAL collector when non-surviving documents are being removed
// from the storage engine's realm
void OtherEngine::removeDocumentRevision(TRI_voc_tick_t databaseId, TRI_voc_cid_t collectionId,
                                         arangodb::velocypack::Slice const& document) {
}

