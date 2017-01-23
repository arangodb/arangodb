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

#include "RocksDBEngine.h"
#include "Basics/FileUtils.h"
#include "Basics/MutexLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Basics/files.h"
//#include "Random/RandomGenerator.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"
#include "VocBase/vocbase.h"
#include "StorageEngine/MMFilesLogfileManager.h"

#include "StorageEngine/MMFilesPersistentIndexFeature.h"
#include "StorageEngine/MMFilesPersistentIndex.h"

#include <velocypack/Collection.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;

std::string const RocksDBEngine::EngineName("rocksdb");
std::string const RocksDBEngine::FeatureName("RocksDBEngine");

// create the storage engine
RocksDBEngine::RocksDBEngine(application_features::ApplicationServer* server)
    : StorageEngine(server, EngineName, FeatureName) {
}

RocksDBEngine::~RocksDBEngine() {
}

// add the storage engine's specifc options to the global list of options
void RocksDBEngine::collectOptions(std::shared_ptr<options::ProgramOptions>) {
}
  
// validate the storage engine's specific options
void RocksDBEngine::validateOptions(std::shared_ptr<options::ProgramOptions>) {
}

// preparation phase for storage engine. can be used for internal setup.
// the storage engine must not start any threads here or write any files
void RocksDBEngine::prepare() {
  TRI_ASSERT(EngineSelectorFeature::ENGINE == this);

  // get base path from DatabaseServerFeature 
  auto databasePathFeature = application_features::ApplicationServer::getFeature<DatabasePathFeature>("DatabasePath");
  _basePath = databasePathFeature->directory();
  _databasePath += databasePathFeature->subdirectoryName("databases") + TRI_DIR_SEPARATOR_CHAR;
  
  TRI_ASSERT(!_basePath.empty());
  TRI_ASSERT(!_databasePath.empty());
}

// initialize engine
void RocksDBEngine::start() {
  TRI_ASSERT(EngineSelectorFeature::ENGINE == this);
  
  // test if the "databases" directory is present and writable
  verifyDirectories();
}

// stop the storage engine. this can be used to flush all data to disk,
// shutdown threads etc. it is guaranteed that there will be no read and
// write requests to the storage engine after this call
void RocksDBEngine::stop() {
  TRI_ASSERT(EngineSelectorFeature::ENGINE == this);
}

// create storage-engine specific collection
PhysicalCollection* RocksDBEngine::createPhysicalCollection(LogicalCollection* collection) {
  TRI_ASSERT(EngineSelectorFeature::ENGINE == this);
  return nullptr;
}

void RocksDBEngine::recoveryDone(TRI_vocbase_t* vocbase) {    
}

// fill the Builder object with an array of databases that were detected
// by the storage engine. this method must sort out databases that were not
// fully created (see "createDatabase" below). called at server start only
void RocksDBEngine::getDatabases(arangodb::velocypack::Builder& result) {
  result.openArray();
  result.close();
}

// fills the provided builder with information about the collection 
void RocksDBEngine::getCollectionInfo(TRI_vocbase_t* vocbase, TRI_voc_cid_t id, 
                                      arangodb::velocypack::Builder& builder, 
                                      bool includeIndexes, TRI_voc_tick_t maxTick) {

  builder.openObject();
  builder.close();
}

// fill the Builder object with an array of collections (and their corresponding
// indexes) that were detected by the storage engine. called at server start only
int RocksDBEngine::getCollectionsAndIndexes(TRI_vocbase_t* vocbase, 
                                            arangodb::velocypack::Builder& result,
                                            bool wasCleanShutdown,
                                            bool isUpgrade) {
  result.openArray();
  result.close();

  return TRI_ERROR_NO_ERROR;
}

TRI_vocbase_t* RocksDBEngine::openDatabase(VPackSlice const& parameters, bool isUpgrade) {
  return nullptr;
}

// asks the storage engine to create a database as specified in the VPack
// Slice object and persist the creation info. It is guaranteed by the server that 
// no other active database with the same name and id exists when this function
// is called. If this operation fails somewhere in the middle, the storage 
// engine is required to fully clean up the creation and throw only then, 
// so that subsequent database creation requests will not fail.
// the WAL entry for the database creation will be written *after* the call
// to "createDatabase" returns
TRI_vocbase_t* RocksDBEngine::createDatabase(TRI_voc_tick_t id, arangodb::velocypack::Slice const& data) {
  return nullptr;
}

// asks the storage engine to drop the specified database and persist the 
// deletion info. Note that physical deletion of the database data must not 
// be carried out by this call, as there may still be readers of the database's data. 
// It is recommended that this operation only sets a deletion flag for the database 
// but let's an async task perform the actual deletion. 
// the WAL entry for database deletion will be written *after* the call
// to "prepareDropDatabase" returns
int RocksDBEngine::prepareDropDatabase(TRI_vocbase_t* vocbase) {
  return TRI_ERROR_NO_ERROR;
}

// perform a physical deletion of the database      
int RocksDBEngine::dropDatabase(TRI_vocbase_t* vocbase) {
  return TRI_ERROR_NO_ERROR;
}

/// @brief wait until a database directory disappears
int RocksDBEngine::waitUntilDeletion(TRI_voc_tick_t id, bool force) {
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
std::string RocksDBEngine::createCollection(TRI_vocbase_t* vocbase, TRI_voc_cid_t id,
                                            arangodb::LogicalCollection const* parameters) {
  return "";
}
 
// asks the storage engine to drop the specified collection and persist the 
// deletion info. Note that physical deletion of the collection data must not 
// be carried out by this call, as there may still be readers of the collection's 
// data. It is recommended that this operation
// only sets a deletion flag for the collection but let's an async task perform
// the actual deletion.
// the WAL entry for collection deletion will be written *after* the call
// to "dropCollection" returns
void RocksDBEngine::prepareDropCollection(TRI_vocbase_t*, arangodb::LogicalCollection*) {
  // nothing to do here
}

// perform a physical deletion of the collection
void RocksDBEngine::dropCollection(TRI_vocbase_t* vocbase, arangodb::LogicalCollection* collection) {
}

// asks the storage engine to change properties of the collection as specified in 
// the VPack Slice object and persist them. If this operation fails 
// somewhere in the middle, the storage engine is required to fully revert the 
// property changes and throw only then, so that subsequent operations will not fail.
// the WAL entry for the propery change will be written *after* the call
// to "changeCollection" returns
void RocksDBEngine::changeCollection(TRI_vocbase_t* vocbase, TRI_voc_cid_t id,
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
void RocksDBEngine::createIndex(TRI_vocbase_t* vocbase, TRI_voc_cid_t collectionId,
                                TRI_idx_iid_t id, arangodb::velocypack::Slice const& data) {
}

// asks the storage engine to drop the specified index and persist the deletion 
// info. Note that physical deletion of the index must not be carried out by this call, 
// as there may still be users of the index. It is recommended that this operation
// only sets a deletion flag for the index but let's an async task perform
// the actual deletion.
// the WAL entry for index deletion will be written *after* the call
// to "dropIndex" returns
void RocksDBEngine::dropIndex(TRI_vocbase_t* vocbase, TRI_voc_cid_t collectionId,
                              TRI_idx_iid_t id) {
}
  
void RocksDBEngine::unloadCollection(TRI_vocbase_t* vocbase, TRI_voc_cid_t collectionId) {
}

void RocksDBEngine::signalCleanup(TRI_vocbase_t* vocbase) {
}

// iterate all documents of the underlying collection
// this is called when a collection is openend, and all its documents need to be added to
// indexes etc.
void RocksDBEngine::iterateDocuments(TRI_voc_tick_t databaseId, TRI_voc_cid_t collectionId,
                                     std::function<void(arangodb::velocypack::Slice const&)> const& cb) {
}

// adds a document to the storage engine
// this will be called by the WAL collector when surviving documents are being moved
// into the storage engine's realm
void RocksDBEngine::addDocumentRevision(TRI_voc_tick_t databaseId, TRI_voc_cid_t collectionId,
                                        arangodb::velocypack::Slice const& document) {
}

// removes a document from the storage engine
// this will be called by the WAL collector when non-surviving documents are being removed
// from the storage engine's realm
void RocksDBEngine::removeDocumentRevision(TRI_voc_tick_t databaseId, TRI_voc_cid_t collectionId,
                                           arangodb::velocypack::Slice const& document) {
}

/// @brief remove data of expired compaction blockers
bool RocksDBEngine::cleanupCompactionBlockers(TRI_vocbase_t* vocbase) {
  TRY_WRITE_LOCKER(locker, _compactionBlockersLock);

  if (!locker.isLocked()) {
    // couldn't acquire lock
    return false;
  }

  auto it = _compactionBlockers.find(vocbase);

  if (it == _compactionBlockers.end()) {
    // no entry for this database
    return true;
  }

  // we are now holding the write lock
  double now = TRI_microtime();

  size_t n = (*it).second.size();

  for (size_t i = 0; i < n; /* no hoisting */) {
    auto& blocker = (*it).second[i];

    if (blocker._expires < now) {
      (*it).second.erase((*it).second.begin() + i);
      n--;
    } else {
      i++;
    }
  }

  if ((*it).second.empty()) {
    // remove last element
    _compactionBlockers.erase(it);
  }

  return true;
}

/// @brief insert a compaction blocker
int RocksDBEngine::insertCompactionBlocker(TRI_vocbase_t* vocbase, double ttl,
                                           TRI_voc_tick_t& id) {
  id = 0;

  if (ttl <= 0.0) {
    return TRI_ERROR_BAD_PARAMETER;
  }

  CompactionBlocker blocker(TRI_NewTickServer(), TRI_microtime() + ttl);

  {
    WRITE_LOCKER_EVENTUAL(locker, _compactionBlockersLock, 1000); 

    auto it = _compactionBlockers.find(vocbase);

    if (it == _compactionBlockers.end()) {
      it = _compactionBlockers.emplace(vocbase, std::vector<CompactionBlocker>()).first;
    }

    (*it).second.emplace_back(blocker);
  }

  id = blocker._id;

  return TRI_ERROR_NO_ERROR;
}

/// @brief touch an existing compaction blocker
int RocksDBEngine::extendCompactionBlocker(TRI_vocbase_t* vocbase, TRI_voc_tick_t id,
                                           double ttl) {
  if (ttl <= 0.0) {
    return TRI_ERROR_BAD_PARAMETER;
  }

  WRITE_LOCKER_EVENTUAL(locker, _compactionBlockersLock, 1000); 

  auto it = _compactionBlockers.find(vocbase);

  if (it == _compactionBlockers.end()) {
    return TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND;
  }

  for (auto& blocker : (*it).second) {
    if (blocker._id == id) {
      blocker._expires = TRI_microtime() + ttl;
      return TRI_ERROR_NO_ERROR;
    }
  }

  return TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND;
}

/// @brief remove an existing compaction blocker
int RocksDBEngine::removeCompactionBlocker(TRI_vocbase_t* vocbase,
                                           TRI_voc_tick_t id) {
  WRITE_LOCKER_EVENTUAL(locker, _compactionBlockersLock, 1000); 
  
  auto it = _compactionBlockers.find(vocbase);

  if (it == _compactionBlockers.end()) {
    return TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND;
  }

  size_t const n = (*it).second.size();

  for (size_t i = 0; i < n; ++i) {
    auto& blocker = (*it).second[i];
    if (blocker._id == id) {
      (*it).second.erase((*it).second.begin() + i);

      if ((*it).second.empty()) {
        // remove last item
        _compactionBlockers.erase(it);
      }
      return TRI_ERROR_NO_ERROR;
    }
  }

  return TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND;
}

void RocksDBEngine::preventCompaction(TRI_vocbase_t* vocbase,
                                      std::function<void(TRI_vocbase_t*)> const& callback) {
  WRITE_LOCKER_EVENTUAL(locker, _compactionBlockersLock, 5000);
  callback(vocbase);
}

bool RocksDBEngine::tryPreventCompaction(TRI_vocbase_t* vocbase,
                                         std::function<void(TRI_vocbase_t*)> const& callback,
                                         bool checkForActiveBlockers) {
  TRY_WRITE_LOCKER(locker, _compactionBlockersLock);

  if (locker.isLocked()) {
    if (checkForActiveBlockers) {
      double const now = TRI_microtime();

      // check if we have a still-valid compaction blocker
      auto it = _compactionBlockers.find(vocbase);

      if (it != _compactionBlockers.end()) {
        for (auto const& blocker : (*it).second) {
          if (blocker._expires > now) {
            // found a compaction blocker
            return false;
          }
        }
      }
    }
    callback(vocbase);
    return true;
  }
  return false;
}

int RocksDBEngine::shutdownDatabase(TRI_vocbase_t* vocbase) {
  return TRI_ERROR_NO_ERROR;
}

/// @brief checks a collection
int RocksDBEngine::openCollection(TRI_vocbase_t* vocbase, LogicalCollection* collection, bool ignoreErrors) {
  return TRI_ERROR_NO_ERROR;
}

/// @brief transfer markers into a collection, actual work
/// the collection must have been prepared to call this function
int RocksDBEngine::transferMarkers(LogicalCollection* collection,
                                   MMFilesCollectorCache* cache,
                                   MMFilesOperationsType const& operations) {
  return TRI_ERROR_NO_ERROR;
}

void RocksDBEngine::verifyDirectories() {
  if (!TRI_IsDirectory(_basePath.c_str())) {
    LOG(ERR) << "database path '" << _basePath << "' is not a directory";

    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DATADIR_INVALID);
  }

  if (!TRI_IsWritable(_basePath.c_str())) {
    // database directory is not writable for the current user... bad luck
    LOG(ERR) << "database directory '" << _basePath
             << "' is not writable for current user";

    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DATADIR_NOT_WRITABLE);
  }

  // verify existence of "databases" subdirectory
  if (!TRI_IsDirectory(_databasePath.c_str())) {
    long systemError;
    std::string errorMessage;
    int res = TRI_CreateDirectory(_databasePath.c_str(), systemError, errorMessage);

    if (res != TRI_ERROR_NO_ERROR) {
      LOG(ERR) << "unable to create database directory '"
               << _databasePath << "': " << errorMessage;

      THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DATADIR_NOT_WRITABLE);
    }
  }

  if (!TRI_IsWritable(_databasePath.c_str())) {
    LOG(ERR) << "database directory '" << _databasePath << "' is not writable";

    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DATADIR_NOT_WRITABLE);
  }
}
