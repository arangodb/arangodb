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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/Parser.h>
#include <velocypack/velocypack-aliases.h>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/QueryCache.h"
#include "Aql/QueryList.h"
#include "Basics/ConditionLocker.h"
#include "Basics/Exceptions.h"
#include "Basics/FileUtils.h"
#include "Basics/HybridLogicalClock.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringRef.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Basics/conversions.h"
#include "Basics/files.h"
#include "Basics/locks.h"
#include "Basics/memory-map.h"
#include "Basics/threads.h"
#include "Basics/tri-strings.h"
#include "Logger/Logger.h"
#include "RestServer/DatabaseFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "Utils/CollectionKeysRepository.h"
#include "Utils/CursorRepository.h"
#include "V8Server/v8-user-structures.h"
#include "VocBase/Ditch.h"
#include "VocBase/collection.h"
#include "VocBase/replication-applier.h"
#include "VocBase/ticks.h"
#include "VocBase/transaction.h"
#include "Wal/LogfileManager.h"

using namespace arangodb;
using namespace arangodb::basics;

/// @brief collection constructor
TRI_vocbase_col_t::TRI_vocbase_col_t(TRI_vocbase_t* vocbase, TRI_col_type_e type,
                                     TRI_voc_cid_t cid, std::string const& name,
                                     TRI_voc_cid_t planId, std::string const& path,
                                     bool isLocal) 
    : _vocbase(vocbase),
      _cid(cid),
      _planId(planId),
      _type(static_cast<TRI_col_type_t>(type)),
      _internalVersion(0),
      _lock(),
      _status(TRI_VOC_COL_STATUS_CORRUPTED),
      _collection(nullptr),
      _dbName(vocbase->name()),
      _name(name),
      _path(path),
      _isLocal(isLocal),
      _canDrop(true),
      _canRename(true) {
  // check for special system collection names
  if (TRI_collection_t::IsSystemName(name)) {
    // a few system collections have special behavior
    if (TRI_EqualString(name.c_str(), TRI_COL_NAME_USERS) ||
        TRI_IsPrefixString(name.c_str(), TRI_COL_NAME_STATISTICS)) {
      // these collections cannot be dropped or renamed
      _canDrop = false;
      _canRename = false;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief collection destructor
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_col_t::~TRI_vocbase_col_t() {}
          
/// @brief signal the cleanup thread to wake up
void TRI_vocbase_t::signalCleanup() {
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  engine->signalCleanup(this);
}

/// @brief adds a new collection
/// caller must hold _collectionsLock in write mode or set doLock
TRI_vocbase_col_t* TRI_vocbase_t::registerCollection(bool doLock,
                                                     TRI_col_type_e type, TRI_voc_cid_t cid,
                                                     std::string const& name,
                                                     TRI_voc_cid_t planId,
                                                     std::string const& path) {
  TRI_ASSERT(cid != 0);

  // create a new proxy
  auto collection =
      std::make_unique<TRI_vocbase_col_t>(this, type, cid, name, planId, path, true);
   
  {
    CONDITIONAL_WRITE_LOCKER(writeLocker, _collectionsLock, doLock);

    // check name
    auto it = _collectionsByName.emplace(name, collection.get());

    if (!it.second) {
      LOG(ERR) << "duplicate entry for collection name '" << name << "'";
      LOG(ERR) << "collection id " << cid
              << " has same name as already added collection "
              << _collectionsByName[name]->_cid;

      TRI_set_errno(TRI_ERROR_ARANGO_DUPLICATE_NAME);

      return nullptr;
    }

    // check collection identifier
    TRI_ASSERT(collection->_cid == cid);
    try {
      auto it2 = _collectionsById.emplace(cid, collection.get());

      if (!it2.second) {
        _collectionsByName.erase(name);

        LOG(ERR) << "duplicate collection identifier " << collection->_cid
                << " for name '" << name << "'";

        TRI_set_errno(TRI_ERROR_ARANGO_DUPLICATE_IDENTIFIER);

        return nullptr;
      }
    }
    catch (...) {
      _collectionsByName.erase(name);
      return nullptr;
    }

    TRI_ASSERT(_collectionsByName.size() == _collectionsById.size());

    try {
      _collections.emplace_back(collection.get());
    }
    catch (...) {
      _collectionsByName.erase(name);
      _collectionsById.erase(cid);
      return nullptr;
    }
  }

  collection->setStatus(TRI_VOC_COL_STATUS_UNLOADED);

  return collection.release();
}

/// @brief write a drop collection marker into the log
int TRI_vocbase_t::writeDropCollectionMarker(TRI_voc_cid_t collectionId,
                                             std::string const& name) {
  int res = TRI_ERROR_NO_ERROR;

  try {
    VPackBuilder builder;
    builder.openObject();
    builder.add("id", VPackValue(std::to_string(collectionId)));
    builder.add("name", VPackValue(name));
    builder.close();

    arangodb::wal::CollectionMarker marker(TRI_DF_MARKER_VPACK_DROP_COLLECTION, _id, collectionId, builder.slice());

    arangodb::wal::SlotInfoCopy slotInfo =
        arangodb::wal::LogfileManager::instance()->allocateAndWrite(marker, false);

    if (slotInfo.errorCode != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(slotInfo.errorCode);
    }
  } catch (arangodb::basics::Exception const& ex) {
    res = ex.code();
  } catch (...) {
    res = TRI_ERROR_INTERNAL;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    LOG(WARN) << "could not save collection drop marker in log: "
              << TRI_errno_string(res);
  }

  return res;
}

/// @brief removes a collection name from the global list of collections
/// This function is called when a collection is dropped.
bool TRI_vocbase_t::unregisterCollection(TRI_vocbase_col_t* collection) {
  TRI_ASSERT(collection != nullptr);
  std::string const colName(collection->name());

  WRITE_LOCKER(writeLocker, _collectionsLock);

  // pre-condition
  TRI_ASSERT(_collectionsByName.size() == _collectionsById.size());

  // only if we find the collection by its id, we can delete it by name
  if (_collectionsById.erase(collection->_cid) > 0) {
    // this is because someone else might have created a new collection with the
    // same name, but with a different id
    _collectionsByName.erase(colName);
  } 

  // post-condition
  TRI_ASSERT(_collectionsByName.size() == _collectionsById.size());

  return true;
}

/// @brief callback for unloading a collection
bool TRI_vocbase_t::UnloadCollectionCallback(TRI_collection_t* col, void* data) {
  TRI_vocbase_col_t* collection = static_cast<TRI_vocbase_col_t*>(data);
  TRI_ASSERT(collection != nullptr);

  WRITE_LOCKER_EVENTUAL(locker, collection->_lock, 1000);

  if (collection->status() != TRI_VOC_COL_STATUS_UNLOADING) {
    return false;
  }

  if (collection->_collection == nullptr) {
    collection->setStatus(TRI_VOC_COL_STATUS_CORRUPTED);
    return true;
  }

  auto ditches = collection->_collection->ditches();

  if (ditches->contains(arangodb::Ditch::TRI_DITCH_DOCUMENT) ||
      ditches->contains(arangodb::Ditch::TRI_DITCH_REPLICATION) ||
      ditches->contains(arangodb::Ditch::TRI_DITCH_COMPACTION)) {
    locker.unlock();

    // still some ditches left...
    // as the cleanup thread has already popped the unload ditch from the
    // ditches list,
    // we need to insert a new one to really executed the unload
    collection->_collection->_vocbase->unloadCollection(collection, false);
    return false;
  }

  TRI_collection_t* document = collection->_collection;

  TRI_ASSERT(document != nullptr);

  int res = document->unload(true);

  if (res != TRI_ERROR_NO_ERROR) {
    std::string const colName(collection->name());
    LOG(ERR) << "failed to close collection '" << colName
             << "': " << TRI_last_error();

    collection->setStatus(TRI_VOC_COL_STATUS_CORRUPTED);
    return true;
  }

  delete document;

  collection->setStatus(TRI_VOC_COL_STATUS_UNLOADED);

  return true;
}

/// @brief drops a collection
bool TRI_vocbase_t::DropCollectionCallback(TRI_vocbase_col_t* collection) {
  std::string const name(collection->name());

  {
    WRITE_LOCKER_EVENTUAL(statusLock, collection->_lock, 1000); 

    if (collection->status() != TRI_VOC_COL_STATUS_DELETED) {
      LOG(ERR) << "someone resurrected the collection '" << name << "'";
      return false;
    }

    // unload collection
    if (collection->_collection != nullptr) {
      TRI_collection_t* document = collection->_collection;

      int res = document->unload(false);

      if (res != TRI_ERROR_NO_ERROR) {
        LOG(ERR) << "failed to close collection '" << name
                << "': " << TRI_last_error();
        return false;
      }

      delete document;

      collection->_collection = nullptr;
    }
  } // release status lock

  // remove from list of collections
  TRI_vocbase_t* vocbase = collection->_vocbase;

  {
    WRITE_LOCKER(writeLocker, vocbase->_collectionsLock);

    auto it = std::find(vocbase->_collections.begin(),
                        vocbase->_collections.end(), collection);

    if (it != vocbase->_collections.end()) {
      vocbase->_collections.erase(it);
    }

    // we need to clean up the pointers later so we insert it into this vector
    try {
      vocbase->_deadCollections.emplace_back(collection);
    } catch (...) {
    }
  }
      
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  engine->dropCollection(vocbase, collection);

  return true;
}

/// @brief creates a new collection, worker function
TRI_vocbase_col_t* TRI_vocbase_t::createCollectionWorker(
    arangodb::VocbaseCollectionInfo& parameters,
    TRI_voc_cid_t& cid, bool writeMarker, VPackBuilder& builder) {
  TRI_ASSERT(!builder.isClosed());
  std::string name = parameters.name();
    
  WRITE_LOCKER(writeLocker, _collectionsLock);

  // reserve room for the new collection
  _collections.reserve(_collections.size() + 1);
  _deadCollections.reserve(_deadCollections.size() + 1);

  auto it = _collectionsByName.find(name);

  if (it != _collectionsByName.end()) {
    TRI_set_errno(TRI_ERROR_ARANGO_DUPLICATE_NAME);
    return nullptr;
  }

  // ok, construct the collection
  TRI_collection_t* document = TRI_collection_t::create(this, parameters, cid);

  if (document == nullptr) {
    return nullptr;
  }

  TRI_collection_t* col = document;
  TRI_vocbase_col_t* collection = nullptr;
  TRI_voc_cid_t planId = parameters.planId();
  col->_info.setPlanId(planId);

  TRI_ASSERT(col->_info.id() != 0);
  
  try {
    collection = registerCollection(ConditionalWriteLocker::DoNotLock(), col->_info.type(), col->_info.id(), col->_info.name(), planId, col->path());
  } catch (...) {
    // if an exception is caught, collection will be a nullptr
  }

  if (collection == nullptr) {
    document->unload(false);
    delete document;
    // TODO: does the collection directory need to be removed?
    return nullptr;
  }

  // cid might have been assigned
  cid = col->_info.id();

  collection->setStatus(TRI_VOC_COL_STATUS_LOADED);
  collection->_collection = document;

  if (writeMarker) {
    col->_info.toVelocyPack(builder);
  }

  return collection;
}

/// @brief renames a collection, worker function
int TRI_vocbase_t::renameCollectionWorker(TRI_vocbase_col_t* collection, 
                            std::string const& oldName,
                            std::string const& newName) {
  // cannot rename a corrupted collection
  if (collection->status() == TRI_VOC_COL_STATUS_CORRUPTED) {
    return TRI_ERROR_ARANGO_CORRUPTED_COLLECTION;
  }

  // cannot rename a deleted collection
  if (collection->status() == TRI_VOC_COL_STATUS_DELETED) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }
    
  {
    WRITE_LOCKER(writeLocker, _collectionsLock);

    // check if the new name is unused
    auto it = _collectionsByName.find(newName);

    if (it != _collectionsByName.end()) {
      return TRI_ERROR_ARANGO_DUPLICATE_NAME;
    }

    if (collection->status() == TRI_VOC_COL_STATUS_UNLOADED) {
      // collection is unloaded
      collection->setName(newName);

      StorageEngine* engine = EngineSelectorFeature::ENGINE;
      engine->renameCollection(this, collection->cid(), newName); 
      // fall-through intentional
    }
    else if (collection->status() == TRI_VOC_COL_STATUS_LOADED ||
             collection->status() == TRI_VOC_COL_STATUS_UNLOADING ||
             collection->status() == TRI_VOC_COL_STATUS_LOADING) {
      // collection is loaded
      int res = collection->_collection->rename(newName);

      if (res != TRI_ERROR_NO_ERROR) {
        return res;
      }
      
      StorageEngine* engine = EngineSelectorFeature::ENGINE;
      engine->renameCollection(this, collection->cid(), newName); 
      // fall-through intentional
    }
    else {
      // unknown status
      return TRI_ERROR_INTERNAL;
    }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    auto it2 =
#endif
    _collectionsByName.emplace(newName, collection);
    TRI_ASSERT(it2.second);
    
    _collectionsByName.erase(oldName);
    TRI_ASSERT(_collectionsByName.size() == _collectionsById.size());

    collection->setName(newName);
  }  // _colllectionsLock

  // to prevent caching returning now invalid old collection name in db's
  // NamedPropertyAccessor, i.e. db.<old-collection-name>
  collection->increaseVersion();

  // invalidate all entries for the two collections
  arangodb::aql::QueryCache::instance()->invalidate(
      this, std::vector<std::string>{oldName, newName});

  return TRI_ERROR_NO_ERROR;
}

/// @brief loads an existing collection
/// Note that this will READ lock the collection. You have to release the
/// collection lock by yourself.
int TRI_vocbase_t::loadCollection(TRI_vocbase_col_t* collection,
                                  TRI_vocbase_col_status_e& status,
                                  bool setStatus) {
  TRI_ASSERT(collection->cid() != 0);

  // read lock
  // check if the collection is already loaded
  {
    READ_LOCKER_EVENTUAL(locker, collection->_lock, 1000);

    // return original status to the caller
    if (setStatus) {
      status = collection->status();
    }

    if (collection->status() == TRI_VOC_COL_STATUS_LOADED) {
      // DO NOT release the lock
      locker.steal();
      return TRI_ERROR_NO_ERROR;
    }

    if (collection->status() == TRI_VOC_COL_STATUS_DELETED) {
      return TRI_set_errno(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
    }

    if (collection->status() == TRI_VOC_COL_STATUS_CORRUPTED) {
      return TRI_set_errno(TRI_ERROR_ARANGO_CORRUPTED_COLLECTION);
    }
  }
  // release the read lock and acquire a write lock, we have to do some work

  // .............................................................................
  // write lock
  // .............................................................................

  WRITE_LOCKER_EVENTUAL(locker, collection->_lock, 1000);

  // someone else loaded the collection, release the WRITE lock and try again
  if (collection->status() == TRI_VOC_COL_STATUS_LOADED) {
    locker.unlock();
    return loadCollection(collection, status, false);
  }

  // someone is trying to unload the collection, cancel this,
  // release the WRITE lock and try again
  if (collection->status() == TRI_VOC_COL_STATUS_UNLOADING) {
    // check if there is a deferred drop action going on for this collection
    if (collection->_collection->ditches()->contains(
            arangodb::Ditch::TRI_DITCH_COLLECTION_DROP)) {
      // drop call going on, we must abort
      locker.unlock();

      // someone requested the collection to be dropped, so it's not there
      // anymore
      return TRI_set_errno(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
    }

    // no drop action found, go on
    collection->setStatus(TRI_VOC_COL_STATUS_LOADED);
    locker.unlock();

    return loadCollection(collection, status, false);
  }

  // deleted, give up
  if (collection->status() == TRI_VOC_COL_STATUS_DELETED) {
    return TRI_set_errno(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
  }

  // corrupted, give up
  if (collection->status() == TRI_VOC_COL_STATUS_CORRUPTED) {
    return TRI_set_errno(TRI_ERROR_ARANGO_CORRUPTED_COLLECTION);
  }

  // currently loading
  if (collection->status() == TRI_VOC_COL_STATUS_LOADING) {
    locker.unlock();

    // loop until the status changes
    while (true) {
      TRI_vocbase_col_status_e status;
      {
        READ_LOCKER_EVENTUAL(readLocker, collection->_lock, 1000);
        status = collection->status();
      }

      if (status != TRI_VOC_COL_STATUS_LOADING) {
        break;
      }

      // only throw this particular error if the server is configured to do so
      auto databaseFeature = application_features::ApplicationServer::getFeature<DatabaseFeature>("Database");
      if (databaseFeature->throwCollectionNotLoadedError()) {
        return TRI_ERROR_ARANGO_COLLECTION_NOT_LOADED;
      }

      usleep(collectionStatusPollInterval());
    }

    return loadCollection(collection, status, false);
  }

  // unloaded, load collection
  if (collection->status() == TRI_VOC_COL_STATUS_UNLOADED) {
    // set the status to loading
    collection->setStatus(TRI_VOC_COL_STATUS_LOADING);

    // release the lock on the collection temporarily
    // this will allow other threads to check the collection's
    // status while it is loading (loading may take a long time because of
    // disk activity, index creation etc.)
    locker.unlock();
      
    bool ignoreDatafileErrors = false;
    if (DatabaseFeature::DATABASE != nullptr) {
      ignoreDatafileErrors = DatabaseFeature::DATABASE->ignoreDatafileErrors();
    }

    TRI_collection_t* document = nullptr;
    try {
      document = TRI_collection_t::open(this, collection, ignoreDatafileErrors);
    } catch (...) {
    }

    // lock again to adjust the status
    locker.lockEventual(1000);

    // no one else must have changed the status
    TRI_ASSERT(collection->status() == TRI_VOC_COL_STATUS_LOADING);

    if (document == nullptr) {
      collection->setStatus(TRI_VOC_COL_STATUS_CORRUPTED);
      return TRI_set_errno(TRI_ERROR_ARANGO_CORRUPTED_COLLECTION);
    }

    collection->_collection = document;
    collection->setStatus(TRI_VOC_COL_STATUS_LOADED);

    // release the WRITE lock and try again
    locker.unlock();

    return loadCollection(collection, status, false);
  }

  std::string const colName(collection->name());
  LOG(ERR) << "unknown collection status " << collection->status() << " for '"
           << colName << "'";

  return TRI_set_errno(TRI_ERROR_INTERNAL);
}

/// @brief drops a collection, worker function
int TRI_vocbase_t::dropCollectionWorker(TRI_vocbase_col_t* collection,
                                        bool writeMarker, DropState& state) {
  state = DROP_EXIT;
  std::string const colName(collection->name());

  WRITE_LOCKER_EVENTUAL(locker, collection->_lock, 1000);

  arangodb::aql::QueryCache::instance()->invalidate(this, colName.c_str());

  // collection already deleted
  if (collection->status() == TRI_VOC_COL_STATUS_DELETED) {
    // mark collection as deleted
    unregisterCollection(collection);
    return TRI_ERROR_NO_ERROR;
  }

  // collection is unloaded
  if (collection->status() == TRI_VOC_COL_STATUS_UNLOADED) {
    bool doSync = application_features::ApplicationServer::getFeature<DatabaseFeature>("Database")->forceSyncProperties();
    doSync = (doSync && !arangodb::wal::LogfileManager::instance()->isInRecovery());
    
    StorageEngine* engine = EngineSelectorFeature::ENGINE;
    VPackBuilder builder;
    engine->getCollectionInfo(this, collection->cid(), builder, false, 0);

    arangodb::VocbaseCollectionInfo info(this, collection->name(), builder.slice().get("parameters"), true);

    if (!info.deleted()) {
      info.setDeleted(true);
 
      try { 
        engine->changeCollection(this, collection->cid(), info, doSync);
      } catch (arangodb::basics::Exception const& ex) {
        info.setDeleted(false);
        return ex.code();
      }
    }

    collection->setStatus(TRI_VOC_COL_STATUS_DELETED);
    unregisterCollection(collection);

    locker.unlock();

    if (writeMarker) {
      writeDropCollectionMarker(collection->_cid, collection->name());
    }

    DropCollectionCallback(collection);

    return TRI_ERROR_NO_ERROR;
  }

  // collection is loading
  if (collection->status() == TRI_VOC_COL_STATUS_LOADING) {
    // loop until status changes
    // try again later
    state = DROP_AGAIN;
    return TRI_ERROR_NO_ERROR;
  }

  // collection is loaded
  if (collection->status() == TRI_VOC_COL_STATUS_LOADED ||
      collection->status() == TRI_VOC_COL_STATUS_UNLOADING) {
    collection->_collection->_info.setDeleted(true);

    bool doSync = application_features::ApplicationServer::getFeature<DatabaseFeature>("Database")->forceSyncProperties();
    doSync = (doSync && !arangodb::wal::LogfileManager::instance()->isInRecovery());
    VPackSlice slice;
    int res = collection->_collection->updateCollectionInfo(this, slice, doSync);

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }

    collection->setStatus(TRI_VOC_COL_STATUS_DELETED);
    unregisterCollection(collection);

    locker.unlock();

    if (writeMarker) {
      writeDropCollectionMarker(collection->_cid, collection->name());
    }

    state = DROP_PERFORM;
    return TRI_ERROR_NO_ERROR;
  }

  // unknown status
  return TRI_ERROR_INTERNAL;
}

/// @brief try to fetch the collection status under a lock
/// the boolean value will be set to true if the lock could be acquired
/// if the boolean is false, the return value is always TRI_VOC_COL_STATUS_CORRUPTED 
TRI_vocbase_col_status_e TRI_vocbase_col_t::tryFetchStatus(bool& found) {
  TRY_READ_LOCKER(locker, _lock);
  if (locker.isLocked()) {
    found = true;
    return _status;
  }
  found = false;
  return TRI_VOC_COL_STATUS_CORRUPTED;
}

void TRI_vocbase_col_t::toVelocyPack(VPackBuilder& builder, bool includeIndexes,
                                     TRI_voc_tick_t maxTick) {
  TRI_ASSERT(!builder.isClosed());
  
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  engine->getCollectionInfo(_vocbase, _cid, builder, includeIndexes, maxTick);
}

std::shared_ptr<VPackBuilder> TRI_vocbase_col_t::toVelocyPack(
    bool includeIndexes, TRI_voc_tick_t maxTick) {
  auto builder = std::make_shared<VPackBuilder>();
  {
    VPackObjectBuilder b(builder.get());
    toVelocyPack(*builder, includeIndexes, maxTick);
  }

  return builder;
}

/// @brief returns a translation of a collection status
char const* TRI_vocbase_col_t::statusString(TRI_vocbase_col_status_e status) {
  switch (status) {
    case TRI_VOC_COL_STATUS_UNLOADED:
      return "unloaded";
    case TRI_VOC_COL_STATUS_LOADED:
      return "loaded";
    case TRI_VOC_COL_STATUS_UNLOADING:
      return "unloading";
    case TRI_VOC_COL_STATUS_DELETED:
      return "deleted";
    case TRI_VOC_COL_STATUS_LOADING:
      return "loading";
    case TRI_VOC_COL_STATUS_CORRUPTED:
    case TRI_VOC_COL_STATUS_NEW_BORN:
    default:
      return "unknown";
  }
}
   
/// @brief set the collection status from the outside
void TRI_vocbase_col_t::setStatus(TRI_vocbase_col_status_e status) {
  _status = status; 

  if (status == TRI_VOC_COL_STATUS_LOADED) {
    _internalVersion = 0;
  } else if (status == TRI_VOC_COL_STATUS_UNLOADED) {
    _collection = nullptr;
  }
}

/// @brief closes a database and all collections
void TRI_vocbase_t::shutdown() {
  // stop replication
  if (_replicationApplier != nullptr) {
    _replicationApplier->stop(false);
  }

  // mark all cursors as deleted so underlying collections can be freed soon
  _cursorRepository->garbageCollect(true);

  // mark all collection keys as deleted so underlying collections can be freed
  // soon
  _collectionKeys->garbageCollect(true);

  std::vector<TRI_vocbase_col_t*> collections;

  {
    READ_LOCKER(writeLocker, _collectionsLock);
    collections = _collections;
  }

  // from here on, the vocbase is unusable, i.e. no collections can be
  // created/loaded etc.

  // starts unloading of collections
  for (auto& collection : collections) {
    unloadCollection(collection, true);
  }

  // this will signal the compactor thread to do one last iteration
  setState(TRI_vocbase_t::State::SHUTDOWN_COMPACTOR);

  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  engine->shutdownDatabase(this);

  // this will signal the cleanup thread to do one last iteration
  setState(TRI_vocbase_t::State::SHUTDOWN_CLEANUP);

  // free dead collections (already dropped but pointers still around)
  for (auto& collection : _deadCollections) {
    delete collection;
  }

  // free collections
  for (auto& collection : _collections) {
    delete collection;
  }
}

/// @brief returns names of all known (document) collections
std::vector<std::string> TRI_vocbase_t::collectionNames() {
  std::vector<std::string> result;

  READ_LOCKER(readLocker, _collectionsLock);

  for (auto const& it : _collectionsById) {
    result.emplace_back(it.second->name());
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all known (document) collections with their parameters
/// and indexes, up to a specific tick value
/// while the collections are iterated over, there will be a global lock so
/// that there will be consistent view of collections & their properties
/// The list of collections will be sorted if sort function is given
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<VPackBuilder> TRI_vocbase_t::inventory(TRI_voc_tick_t maxTick,
    bool (*filter)(TRI_vocbase_col_t*, void*), void* data, bool shouldSort,
    std::function<bool(TRI_vocbase_col_t*, TRI_vocbase_col_t*)> sortCallback) {
  std::vector<TRI_vocbase_col_t*> collections;

  // cycle on write-lock
  WRITE_LOCKER_EVENTUAL(writeLock, _inventoryLock, 1000);

  // copy collection pointers into vector so we can work with the copy without
  // the global lock
  {
    READ_LOCKER(readLocker, _collectionsLock);
    collections = _collections;
  }

  if (shouldSort && collections.size() > 1) {
    std::sort(collections.begin(), collections.end(), sortCallback);
  }

  auto builder = std::make_shared<VPackBuilder>();
  builder->openArray();

  for (auto& collection : collections) {
    READ_LOCKER(readLocker, collection->_lock);

    if (collection->status() == TRI_VOC_COL_STATUS_DELETED ||
        collection->status() == TRI_VOC_COL_STATUS_CORRUPTED) {
      // we do not need to care about deleted or corrupted collections
      continue;
    }

    if (collection->_cid > maxTick) {
      // collection is too new
      continue;
    }

    // check if we want this collection
    if (filter != nullptr && !filter(collection, data)) {
      continue;
    }

    collection->toVelocyPack(*builder, true, maxTick);
  }

  builder->close();

  return builder;
}

/// @brief gets a collection name by a collection id
/// the name is fetched under a lock to make this thread-safe.
/// returns empty string if the collection does not exist.
std::string TRI_vocbase_t::collectionName(TRI_voc_cid_t id) {
  READ_LOCKER(readLocker, _collectionsLock);

  auto it = _collectionsById.find(id);

  if (it == _collectionsById.end()) {
    return StaticStrings::Empty;
  }

  return (*it).second->name();
}

/// @brief looks up a collection by name
TRI_vocbase_col_t* TRI_vocbase_t::lookupCollection(std::string const& name) {
  if (name.empty()) {
    return nullptr;
  }

  // if collection name is passed as a stringified id, we'll use the lookupbyid
  // function
  // this is safe because collection names must not start with a digit
  if (name[0] >= '0' && name[0] <= '9') {
    return lookupCollection(StringUtils::uint64(name));
  }

  // otherwise we'll look up the collection by name
  READ_LOCKER(readLocker, _collectionsLock);

  auto it = _collectionsByName.find(name);

  if (it == _collectionsByName.end()) {
    return nullptr;
  }
  return (*it).second;
}

/// @brief looks up a collection by identifier
TRI_vocbase_col_t* TRI_vocbase_t::lookupCollection(TRI_voc_cid_t id) {
  READ_LOCKER(readLocker, _collectionsLock);
  
  auto it = _collectionsById.find(id);

  if (it == _collectionsById.end()) {
    return nullptr;
  }
  return (*it).second;
}

/// @brief creates a new collection from parameter set
/// collection id (cid) is normally passed with a value of 0
/// this means that the system will assign a new collection id automatically
/// using a cid of > 0 is supported to import dumps from other servers etc.
/// but the functionality is not advertised
TRI_vocbase_col_t* TRI_vocbase_t::createCollection(
    arangodb::VocbaseCollectionInfo& parameters,
    TRI_voc_cid_t cid, bool writeMarker) {
  // check that the name does not contain any strange characters
  if (!TRI_collection_t::IsAllowedName(parameters.isSystem(),
                                       parameters.name())) {
    TRI_set_errno(TRI_ERROR_ARANGO_ILLEGAL_NAME);

    return nullptr;
  }
  
  TRI_vocbase_col_t* collection = nullptr;
  VPackBuilder builder;

  READ_LOCKER(readLocker, _inventoryLock);

  {
    VPackObjectBuilder b(&builder);
    // note: cid may be modified by this function call
    try {
      collection = createCollectionWorker(parameters, cid, writeMarker, builder);
    } catch (basics::Exception const& ex) {
      TRI_set_errno(ex.code());
      return nullptr;
    } catch (...) {
      TRI_set_errno(TRI_ERROR_INTERNAL);
      return nullptr;
    }
  }

  if (!writeMarker) {
    return collection;
  }

  if (collection == nullptr) {
    // something went wrong... must not continue
    return nullptr;
  }

  VPackSlice const slice = builder.slice();

  TRI_ASSERT(cid != 0);

  int res = TRI_ERROR_NO_ERROR;

  try {
    arangodb::wal::CollectionMarker marker(TRI_DF_MARKER_VPACK_CREATE_COLLECTION, _id, cid, slice);

    arangodb::wal::SlotInfoCopy slotInfo =
        arangodb::wal::LogfileManager::instance()->allocateAndWrite(marker, false);

    if (slotInfo.errorCode != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(slotInfo.errorCode);
    }

    return collection;
  } catch (arangodb::basics::Exception const& ex) {
    res = ex.code();
  } catch (...) {
    res = TRI_ERROR_INTERNAL;
  }

  LOG(WARN) << "could not save collection create marker in log: "
            << TRI_errno_string(res);

  // TODO: what to do here?
  return collection;
}

/// @brief unloads a collection
int TRI_vocbase_t::unloadCollection(TRI_vocbase_col_t* collection, bool force) {
  TRI_voc_cid_t cid = collection->cid();

  {
    WRITE_LOCKER_EVENTUAL(locker, collection->_lock, 1000);

    // cannot unload a corrupted collection
    if (collection->status() == TRI_VOC_COL_STATUS_CORRUPTED) {
      return TRI_set_errno(TRI_ERROR_ARANGO_CORRUPTED_COLLECTION);
    }

    // an unloaded collection is unloaded
    if (collection->status() == TRI_VOC_COL_STATUS_UNLOADED) {
      return TRI_ERROR_NO_ERROR;
    }

    // an unloading collection is treated as unloaded
    if (collection->status() == TRI_VOC_COL_STATUS_UNLOADING) {
      return TRI_ERROR_NO_ERROR;
    }

    // a loading collection
    if (collection->status() == TRI_VOC_COL_STATUS_LOADING) {
      // throw away the write locker. we're going to switch to a read locker now
      locker.unlock();

      // loop until status changes
      while (1) {
        TRI_vocbase_col_status_e status;

        {
          READ_LOCKER_EVENTUAL(readLocker, collection->_lock, 1000);
          status = collection->status();
        }

        if (status != TRI_VOC_COL_STATUS_LOADING) {
          break;
        }
        // sleep without lock
        usleep(collectionStatusPollInterval());
      }
      // if we get here, the status has changed
      return unloadCollection(collection, force);
    }

    // a deleted collection is treated as unloaded
    if (collection->status() == TRI_VOC_COL_STATUS_DELETED) {
      return TRI_ERROR_NO_ERROR;
    }

    // must be loaded
    if (collection->status() != TRI_VOC_COL_STATUS_LOADED) {
      return TRI_set_errno(TRI_ERROR_INTERNAL);
    }

    // mark collection as unloading
    collection->setStatus(TRI_VOC_COL_STATUS_UNLOADING);

    // add callback for unload
    collection->_collection->ditches()->createUnloadCollectionDitch(
        collection->_collection, collection, UnloadCollectionCallback, __FILE__,
        __LINE__);
  } // release locks

  // wake up the cleanup thread
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  engine->unloadCollection(this, cid);

  return TRI_ERROR_NO_ERROR;
}

/// @brief drops a collection
int TRI_vocbase_t::dropCollection(TRI_vocbase_col_t* collection, bool writeMarker) {
  TRI_ASSERT(collection != nullptr);

  if (!collection->canDrop() &&
      !arangodb::wal::LogfileManager::instance()->isInRecovery()) {
    return TRI_set_errno(TRI_ERROR_FORBIDDEN);
  }

  while (true) {
    DropState state = DROP_EXIT;
    int res;
    {
      READ_LOCKER(readLocker, _inventoryLock);

      res = dropCollectionWorker(collection, writeMarker, state);
    }

    if (state == DROP_PERFORM) {
      if (arangodb::wal::LogfileManager::instance()->isInRecovery()) {
        DropCollectionCallback(collection);
      } else {
        // add callback for dropping
        collection->_collection->ditches()->createDropCollectionDitch(
            collection, DropCollectionCallback,
            __FILE__, __LINE__);

        // wake up the cleanup thread
        StorageEngine* engine = EngineSelectorFeature::ENGINE;
        engine->signalCleanup(collection->_vocbase);
      }
    }

    if (state == DROP_PERFORM || state == DROP_EXIT) {
      return res;
    }

    // try again in next iteration
    TRI_ASSERT(state == DROP_AGAIN);
    usleep(collectionStatusPollInterval());
  }
}

/// @brief renames a collection
int TRI_vocbase_t::renameCollection(TRI_vocbase_col_t* collection,
                                    std::string const& newName, bool doOverride,
                                    bool writeMarker) {
  if (!collection->canRename()) {
    return TRI_set_errno(TRI_ERROR_FORBIDDEN);
  }

  // lock collection because we are going to copy its current name
  std::string oldName;
  {
    READ_LOCKER(readLocker, collection->_lock);
    oldName = collection->name();
  }

  // old name should be different

  // check if names are actually different
  if (oldName == newName) {
    return TRI_ERROR_NO_ERROR;
  }

  if (!doOverride) {
    bool isSystem;
    isSystem = TRI_collection_t::IsSystemName(oldName);

    if (isSystem && !TRI_collection_t::IsSystemName(newName)) {
      // a system collection shall not be renamed to a non-system collection
      // name
      return TRI_set_errno(TRI_ERROR_ARANGO_ILLEGAL_NAME);
    } else if (!isSystem && TRI_collection_t::IsSystemName(newName)) {
      // a non-system collection shall not be renamed to a system collection
      // name
      return TRI_set_errno(TRI_ERROR_ARANGO_ILLEGAL_NAME);
    }

    if (!TRI_collection_t::IsAllowedName(isSystem, newName)) {
      return TRI_set_errno(TRI_ERROR_ARANGO_ILLEGAL_NAME);
    }
  }

  READ_LOCKER(readLocker, _inventoryLock);

  int res;
  {
    WRITE_LOCKER_EVENTUAL(locker, collection->_lock, 1000);
    res = renameCollectionWorker(collection, oldName, newName);
  }

  if (res == TRI_ERROR_NO_ERROR && writeMarker) {
    // now log the operation
    try {
      VPackBuilder builder;
      builder.openObject();
      builder.add("id", VPackValue(std::to_string(collection->_cid)));
      builder.add("oldName", VPackValue(oldName));
      builder.add("name", VPackValue(newName));
      builder.close();

      arangodb::wal::CollectionMarker marker(TRI_DF_MARKER_VPACK_RENAME_COLLECTION, _id, collection->_cid, builder.slice());

      arangodb::wal::SlotInfoCopy slotInfo =
          arangodb::wal::LogfileManager::instance()->allocateAndWrite(marker, false);

      if (slotInfo.errorCode != TRI_ERROR_NO_ERROR) {
        THROW_ARANGO_EXCEPTION(slotInfo.errorCode);
      }

      return TRI_ERROR_NO_ERROR;
    } catch (arangodb::basics::Exception const& ex) {
      res = ex.code();
    } catch (...) {
      res = TRI_ERROR_INTERNAL;
    }

    if (res != TRI_ERROR_NO_ERROR) {
      LOG(WARN) << "could not save collection rename marker in log: "
                << TRI_errno_string(res);
    }
  }

  return res;
}

/// @brief locks a collection for usage, loading or manifesting it
int TRI_vocbase_t::useCollection(TRI_vocbase_col_t* collection,
                                 TRI_vocbase_col_status_e& status) {
  return loadCollection(collection, status);
}

/// @brief locks a (document) collection for usage by id
TRI_vocbase_col_t* TRI_vocbase_t::useCollection(TRI_voc_cid_t cid,
    TRI_vocbase_col_status_e& status) {
  // check that we have an existing name
  TRI_vocbase_col_t* collection = nullptr;
  {
    READ_LOCKER(readLocker, _collectionsLock);

    auto it = _collectionsById.find(cid);

    if (it != _collectionsById.end()) {
      collection = (*it).second;
    }
  }

  if (collection == nullptr) {
    TRI_set_errno(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
    return nullptr;
  }

  // try to load the collection
  int res = loadCollection(collection, status);

  if (res == TRI_ERROR_NO_ERROR) {
    return collection;
  }

  TRI_set_errno(res);

  return nullptr;
}

/// @brief locks a collection for usage by name
TRI_vocbase_col_t* TRI_vocbase_t::useCollection(std::string const& name,
    TRI_vocbase_col_status_e& status) {
  // check that we have an existing name
  TRI_vocbase_col_t* collection = nullptr;

  {
    READ_LOCKER(readLocker, _collectionsLock);

    auto it = _collectionsByName.find(name);

    if (it != _collectionsByName.end()) {
      collection = (*it).second;
    }
  }

  if (collection == nullptr) {
    TRI_set_errno(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
    return nullptr;
  }

  // try to load the collection
  int res = loadCollection(collection, status);

  if (res == TRI_ERROR_NO_ERROR) {
    return collection;
  }

  TRI_set_errno(res);
  return nullptr;
}

/// @brief releases a collection from usage
void TRI_vocbase_t::releaseCollection(TRI_vocbase_col_t* collection) {
  collection->_lock.unlock();
}

/// @brief create a vocbase object
TRI_vocbase_t::TRI_vocbase_t(TRI_vocbase_type_e type, TRI_voc_tick_t id,
                             std::string const& name)
    : _id(id),
      _name(name),
      _type(type),
      _refCount(0),
      _state(TRI_vocbase_t::State::NORMAL),
      _isOwnAppsDirectory(true),
      _deadlockDetector(false),
      _userStructures(nullptr) {

  _queries.reset(new arangodb::aql::QueryList(this));
  _cursorRepository.reset(new arangodb::CursorRepository(this));
  _collectionKeys.reset(new arangodb::CollectionKeysRepository());

  // init collections
  _collections.reserve(32);
  _deadCollections.reserve(32);

  TRI_CreateUserStructuresVocBase(this);
}

/// @brief destroy a vocbase object
TRI_vocbase_t::~TRI_vocbase_t() {
  if (_userStructures != nullptr) {
    TRI_FreeUserStructuresVocBase(this);
  }

  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  engine->shutdownDatabase(this);
}
  
std::string TRI_vocbase_t::path() const {
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  return engine->databasePath(this);
}

/// @brief checks if a database name is allowed
/// returns true if the name is allowed and false otherwise
bool TRI_vocbase_t::IsAllowedName(bool allowSystem, std::string const& name) {
  size_t length = 0;

  // check allow characters: must start with letter or underscore if system is
  // allowed
  for (char const* ptr = name.c_str(); *ptr; ++ptr) {
    bool ok;
    if (length == 0) {
      if (allowSystem) {
        ok = (*ptr == '_') || ('a' <= *ptr && *ptr <= 'z') ||
             ('A' <= *ptr && *ptr <= 'Z');
      } else {
        ok = ('a' <= *ptr && *ptr <= 'z') || ('A' <= *ptr && *ptr <= 'Z');
      }
    } else {
      ok = (*ptr == '_') || (*ptr == '-') || ('0' <= *ptr && *ptr <= '9') ||
           ('a' <= *ptr && *ptr <= 'z') || ('A' <= *ptr && *ptr <= 'Z');
    }

    if (!ok) {
      return false;
    }

    ++length;
  }

  // invalid name length
  if (length == 0 || length > TRI_COL_NAME_LENGTH) {
    return false;
  }

  return true;
}
  
void TRI_vocbase_t::addReplicationApplier(TRI_replication_applier_t* applier) {
  _replicationApplier.reset(applier);
}

/// @brief note the progress of a connected replication client
void TRI_vocbase_t::updateReplicationClient(TRI_server_id_t serverId,
                                            TRI_voc_tick_t lastFetchedTick) {
  WRITE_LOCKER(writeLocker, _replicationClientsLock);

  try {
    auto it = _replicationClients.find(serverId);

    if (it == _replicationClients.end()) {
      _replicationClients.emplace(
          serverId, std::make_pair(TRI_microtime(), lastFetchedTick));
    } else {
      (*it).second.first = TRI_microtime();
      if (lastFetchedTick > 0) {
        (*it).second.second = lastFetchedTick;
      }
    }
  } catch (...) {
    // silently fail...
    // all we would be missing is the progress information of a slave
  }
}

/// @brief return the progress of all replication clients
std::vector<std::tuple<TRI_server_id_t, double, TRI_voc_tick_t>>
TRI_vocbase_t::getReplicationClients() {
  std::vector<std::tuple<TRI_server_id_t, double, TRI_voc_tick_t>> result;

  READ_LOCKER(readLocker, _replicationClientsLock);

  for (auto& it : _replicationClients) {
    result.emplace_back(
        std::make_tuple(it.first, it.second.first, it.second.second));
  }
  return result;
}
      
std::vector<TRI_vocbase_col_t*> TRI_vocbase_t::collections() {
  std::vector<TRI_vocbase_col_t*> collections;

  {
    READ_LOCKER(readLocker, _collectionsLock);
    collections = _collections;
  }
  return collections;
}

/// @brief velocypack sub-object (for indexes, as part of TRI_index_element_t, 
/// if offset is non-zero, then it is an offset into the VelocyPack data in
/// the data or WAL file. If offset is 0, then data contains the actual data
/// in place.
VPackSlice TRI_vpack_sub_t::slice(TRI_doc_mptr_t const* mptr) const {
  if (isValue()) {
    return VPackSlice(&value.data[0]);
  } 
  return VPackSlice(mptr->vpack() + value.offset);
}

/// @brief fill a TRI_vpack_sub_t structure with a subvalue
void TRI_FillVPackSub(TRI_vpack_sub_t* sub,
                      VPackSlice const base, VPackSlice const value) noexcept {
  if (value.byteSize() <= TRI_vpack_sub_t::maxValueLength()) {
    sub->setValue(value.start(), static_cast<size_t>(value.byteSize()));
  } else {
    size_t off = value.start() - base.start();
    TRI_ASSERT(off <= UINT32_MAX);
    sub->setOffset(static_cast<uint32_t>(off));
  }
}

/// @brief extract the _rev attribute from a slice
TRI_voc_rid_t TRI_ExtractRevisionId(VPackSlice slice) {
  slice = slice.resolveExternal();
  TRI_ASSERT(slice.isObject());

  VPackSlice r(slice.get(StaticStrings::RevString));
  if (r.isString()) {
    bool isOld;
    return TRI_StringToRid(r.copyString(), isOld);
  }
  if (r.isInteger()) {
    return r.getNumber<TRI_voc_rid_t>();
  }
  return 0;
}
  
/// @brief extract the _rev attribute from a slice as a slice
VPackSlice TRI_ExtractRevisionIdAsSlice(VPackSlice const slice) {
  if (!slice.isObject()) {
    return VPackSlice();
  }

  return slice.get(StaticStrings::RevString);
}

/// @brief sanitize an object, given as slice, builder must contain an
/// open object which will remain open
/// the result is the object excluding _id, _key and _rev
void TRI_SanitizeObject(VPackSlice const slice, VPackBuilder& builder) {
  TRI_ASSERT(slice.isObject());
  VPackObjectIterator it(slice);
  while (it.valid()) {
    StringRef key(it.key());
    if (key.empty() || key[0] != '_' ||
         (key != StaticStrings::KeyString &&
          key != StaticStrings::IdString &&
          key != StaticStrings::RevString)) {
      builder.add(key.data(), key.size(), it.value());
    }
    it.next();
  }
}

/// @brief sanitize an object, given as slice, builder must contain an
/// open object which will remain open. also excludes _from and _to
void TRI_SanitizeObjectWithEdges(VPackSlice const slice, VPackBuilder& builder) {
  TRI_ASSERT(slice.isObject());
  VPackObjectIterator it(slice);
  while (it.valid()) {
    StringRef key(it.key());
    if (key.empty() || key[0] != '_' ||
         (key != StaticStrings::KeyString &&
          key != StaticStrings::IdString &&
          key != StaticStrings::RevString &&
          key != StaticStrings::FromString &&
          key != StaticStrings::ToString)) {
      builder.add(key.data(), key.length(), it.value());
    }
    it.next();
  }
}

/// @brief Convert a revision ID to a string
constexpr static TRI_voc_rid_t tickLimit 
  = static_cast<TRI_voc_rid_t>(2016 - 1970) * 1000 * 60 * 60 * 24 * 365;

std::string TRI_RidToString(TRI_voc_rid_t rid) {
  if (rid <= tickLimit) {
    return arangodb::basics::StringUtils::itoa(rid);
  }
  return HybridLogicalClock::encodeTimeStamp(rid);
}

/// @brief Convert a string into a revision ID, no check variant
TRI_voc_rid_t TRI_StringToRid(std::string const& ridStr, bool& isOld) {
  return TRI_StringToRid(ridStr.c_str(), ridStr.size(), isOld);
}

/// @brief Convert a string into a revision ID, no check variant
TRI_voc_rid_t TRI_StringToRid(char const* p, size_t len, bool& isOld) {
  if (len > 0 && *p >= '1' && *p <= '9') {
    // Remove this case before the year 3887 AD because then it will
    // start to clash with encoded timestamps:
    TRI_voc_rid_t r = arangodb::basics::StringUtils::uint64(p, len);
    if (r > tickLimit) {
      // An old tick value that could be confused with a time stamp
      LOG(WARN)
        << "Saw old _rev value that could be confused with a time stamp!";
    }
    isOld = true;
    return r;
  }
  isOld = false;
  return HybridLogicalClock::decodeTimeStamp(p, len);
}

/// @brief Convert a string into a revision ID, returns 0 if format invalid
TRI_voc_rid_t TRI_StringToRidWithCheck(std::string const& ridStr, bool& isOld) {
  return TRI_StringToRidWithCheck(ridStr.c_str(), ridStr.size(), isOld);
}

/// @brief Convert a string into a revision ID, returns 0 if format invalid
TRI_voc_rid_t TRI_StringToRidWithCheck(char const* p, size_t len, bool& isOld) {
  if (len > 0 && *p >= '1' && *p <= '9') {
    // Remove this case before the year 3887 AD because then it will
    // start to clash with encoded timestamps:
    TRI_voc_rid_t r = arangodb::basics::StringUtils::uint64_check(p, len);
    if (r > tickLimit) {
      // An old tick value that could be confused with a time stamp
      LOG(WARN)
        << "Saw old _rev value that could be confused with a time stamp!";
    }
    isOld = true;
    return r;
  }
  isOld = false;
  return HybridLogicalClock::decodeTimeStampWithCheck(p, len);
}

