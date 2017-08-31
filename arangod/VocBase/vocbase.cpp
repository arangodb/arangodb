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
#include "Aql/PlanCache.h"
#include "Aql/QueryCache.h"
#include "Aql/QueryList.h"
#include "Basics/ConditionLocker.h"
#include "Basics/Exceptions.h"
#include "Basics/FileUtils.h"
#include "Basics/HybridLogicalClock.h"
#include "Basics/ReadLocker.h"
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
#include "RestServer/ViewTypesFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/PhysicalCollection.h"
#include "StorageEngine/StorageEngine.h"
#include "Utils/CollectionKeysRepository.h"
#include "Utils/CursorRepository.h"
#include "Utils/Events.h"
#include "V8Server/v8-user-structures.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"
#include "VocBase/PhysicalView.h"
#include "VocBase/ViewImplementation.h"
#include "VocBase/replication-applier.h"
#include "VocBase/ticks.h"

#include <thread>

using namespace arangodb;
using namespace arangodb::basics;

/// @brief increase the reference counter for a database
bool TRI_vocbase_t::use() {
  auto expected = _refCount.load(std::memory_order_relaxed);
  while (true) {
    if ((expected & 1) != 0) {
      // deleted bit is set
      return false;
    }
    // increase the reference counter by 2.
    // this is because we use odd values to indicate that the database has been
    // marked as deleted
    auto updated = expected + 2;
    TRI_ASSERT((updated & 1) == 0);
    if (_refCount.compare_exchange_weak(expected, updated,
                                        std::memory_order_release,
                                        std::memory_order_relaxed)) {
      // compare-exchange worked. we're done
      return true;
    }
    // compare-exchange failed. try again!
    expected = _refCount.load(std::memory_order_relaxed);
  }
}

void TRI_vocbase_t::forceUse() { _refCount += 2; }

/// @brief decrease the reference counter for a database
void TRI_vocbase_t::release() {
  // decrease the reference counter by 2.
  // this is because we use odd values to indicate that the database has been
  // marked as deleted
  auto oldValue = _refCount.fetch_sub(2);
  TRI_ASSERT(oldValue >= 2);
}

/// @brief returns whether the database can be dropped
bool TRI_vocbase_t::isDangling() const {
  if (isSystem()) {
    return false;
  }
  auto refCount = _refCount.load();
  // we are intentionally comparing with exactly 1 here, because a 1 means
  // that noone else references the database but it has been marked as deleted
  return (refCount == 1);
}

/// @brief whether or not the vocbase has been marked as deleted
bool TRI_vocbase_t::isDropped() const {
  auto refCount = _refCount.load();
  // if the stored value is odd, it means the database has been marked as
  // deleted
  return (refCount % 2 == 1);
}

/// @brief marks a database as deleted
bool TRI_vocbase_t::markAsDropped() {
  TRI_ASSERT(!isSystem());

  auto oldValue = _refCount.fetch_or(1);
  // if the previously stored value is odd, it means the database has already
  // been marked as deleted
  return (oldValue % 2 == 0);
}

/// @brief signal the cleanup thread to wake up
void TRI_vocbase_t::signalCleanup() {
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  engine->signalCleanup(this);
}

/// @brief adds a new collection
/// caller must hold _collectionsLock in write mode or set doLock
void TRI_vocbase_t::registerCollection(
    bool doLock, arangodb::LogicalCollection* collection) {
  std::string name = collection->name();
  TRI_voc_cid_t cid = collection->cid();
  {
    CONDITIONAL_WRITE_LOCKER(writeLocker, _collectionsLock, doLock);

    // check name
    auto it = _collectionsByName.emplace(name, collection);

    if (!it.second) {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME)
          << "duplicate entry for collection name '" << name << "'";
      LOG_TOPIC(ERR, arangodb::Logger::FIXME)
          << "collection id " << cid
          << " has same name as already added collection "
          << _collectionsByName[name]->cid();

      THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DUPLICATE_NAME);
    }

    // check collection identifier
    try {
      auto it2 = _collectionsById.emplace(cid, collection);

      if (!it2.second) {
        _collectionsByName.erase(name);

        LOG_TOPIC(ERR, arangodb::Logger::FIXME)
            << "duplicate collection identifier " << collection->cid()
            << " for name '" << name << "'";

        THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DUPLICATE_IDENTIFIER);
      }
    } catch (...) {
      _collectionsByName.erase(name);
      throw;
    }

    TRI_ASSERT(_collectionsByName.size() == _collectionsById.size());

    try {
      _collections.emplace_back(collection);
    } catch (...) {
      _collectionsByName.erase(name);
      _collectionsById.erase(cid);
      TRI_ASSERT(_collectionsByName.size() == _collectionsById.size());
      throw;
    }

    collection->setStatus(TRI_VOC_COL_STATUS_UNLOADED);
    TRI_ASSERT(_collectionsByName.size() == _collectionsById.size());
  }
}

/// @brief removes a collection name from the global list of collections
/// This function is called when a collection is dropped.
/// NOTE: You need a writelock on _collectionsLock
bool TRI_vocbase_t::unregisterCollection(
    arangodb::LogicalCollection* collection) {
  TRI_ASSERT(collection != nullptr);
  std::string const colName(collection->name());

  // pre-condition
  TRI_ASSERT(_collectionsByName.size() == _collectionsById.size());

  // only if we find the collection by its id, we can delete it by name
  if (_collectionsById.erase(collection->cid()) > 0) {
    // this is because someone else might have created a new collection with the
    // same name, but with a different id
    _collectionsByName.erase(colName);
  }

  // post-condition
  TRI_ASSERT(_collectionsByName.size() == _collectionsById.size());

  return true;
}

/// @brief adds a new view
/// caller must hold _viewLock in write mode or set doLock
void TRI_vocbase_t::registerView(bool doLock,
                                 std::shared_ptr<arangodb::LogicalView> view) {
  std::string const name = view->name();
  TRI_voc_cid_t const id = view->id();
  {
    CONDITIONAL_WRITE_LOCKER(writeLocker, _viewsLock, doLock);

    // check name
    auto it = _viewsByName.emplace(name, view);

    if (!it.second) {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME)
          << "duplicate entry for view name '" << name << "'";
      LOG_TOPIC(ERR, arangodb::Logger::FIXME)
          << "view id " << id << " has same name as already added view "
          << _viewsByName[name]->id();

      THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DUPLICATE_NAME);
    }

    // check id
    try {
      auto it2 = _viewsById.emplace(id, view);

      if (!it2.second) {
        _viewsByName.erase(name);

        LOG_TOPIC(ERR, arangodb::Logger::FIXME)
            << "duplicate view identifier " << view->id() << " for name '"
            << name << "'";

        THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DUPLICATE_IDENTIFIER);
      }
    } catch (...) {
      _viewsByName.erase(name);
      throw;
    }

    TRI_ASSERT(_viewsByName.size() == _viewsById.size());
  }
}

/// @brief removes a views name from the global list of views
/// This function is called when a view is dropped.
/// NOTE: You need a writelock on _viewsLock
bool TRI_vocbase_t::unregisterView(
    std::shared_ptr<arangodb::LogicalView> view) {
  TRI_ASSERT(view != nullptr);
  std::string const name(view->name());

  // pre-condition
  TRI_ASSERT(_viewsByName.size() == _viewsById.size());

  // only if we find the collection by its id, we can delete it by name
  if (_viewsById.erase(view->id()) > 0) {
    // this is because someone else might have created a new view with the
    // same name, but with a different id
    _viewsByName.erase(name);
  }

  // post-condition
  TRI_ASSERT(_viewsByName.size() == _viewsById.size());

  return true;
}

/// @brief drops a collection
bool TRI_vocbase_t::DropCollectionCallback(
    arangodb::LogicalCollection* collection) {
  std::string const name(collection->name());

  {
    WRITE_LOCKER_EVENTUAL(statusLock, collection->_lock);

    if (collection->status() != TRI_VOC_COL_STATUS_DELETED) {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME)
          << "someone resurrected the collection '" << name << "'";
      return false;
    }
  }  // release status lock

  // remove from list of collections
  TRI_vocbase_t* vocbase = collection->vocbase();

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
  collection->drop();

  return true;
}

/// @brief creates a new collection, worker function
arangodb::LogicalCollection* TRI_vocbase_t::createCollectionWorker(
    VPackSlice parameters) {
  std::string name = arangodb::basics::VelocyPackHelper::getStringValue(
      parameters, "name", "");
  TRI_ASSERT(!name.empty());

  // Try to create a new collection. This is not registered yet

  std::unique_ptr<arangodb::LogicalCollection> collection =
      std::make_unique<arangodb::LogicalCollection>(this, parameters);
  TRI_ASSERT(collection != nullptr);

  WRITE_LOCKER(writeLocker, _collectionsLock);

  // reserve room for the new collection
  _collections.reserve(_collections.size() + 1);
  _deadCollections.reserve(_deadCollections.size() + 1);

  auto it = _collectionsByName.find(name);

  if (it != _collectionsByName.end()) {
    events::CreateCollection(name, TRI_ERROR_ARANGO_DUPLICATE_NAME);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DUPLICATE_NAME);
  }

  registerCollection(basics::ConditionalLocking::DoNotLock, collection.get());

  try {
    collection->setStatus(TRI_VOC_COL_STATUS_LOADED);
    // set collection version to 3.1, as the collection is just created
    collection->setVersion(LogicalCollection::VERSION_31);

    // Let's try to persist it.
    collection->persistPhysicalCollection();

    events::CreateCollection(name, TRI_ERROR_NO_ERROR);
    return collection.release();
  } catch (...) {
    unregisterCollection(collection.get());
    throw;
  }
}

/// @brief loads an existing collection
/// Note that this will READ lock the collection. You have to release the
/// collection lock by yourself.
int TRI_vocbase_t::loadCollection(arangodb::LogicalCollection* collection,
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

  WRITE_LOCKER_EVENTUAL(locker, collection->_lock);

  // someone else loaded the collection, release the WRITE lock and try again
  if (collection->status() == TRI_VOC_COL_STATUS_LOADED) {
    locker.unlock();
    return loadCollection(collection, status, false);
  }

  // someone is trying to unload the collection, cancel this,
  // release the WRITE lock and try again
  if (collection->status() == TRI_VOC_COL_STATUS_UNLOADING) {
    // check if the collection is dropped
    if (collection->deleted()) {
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
      auto databaseFeature =
          application_features::ApplicationServer::getFeature<DatabaseFeature>(
              "Database");
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

    try {
      collection->open(ignoreDatafileErrors);
    } catch (arangodb::basics::Exception const& ex) {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME)
          << "caught exception while opening collection '" << collection->name()
          << "': " << ex.what();
      collection->setStatus(TRI_VOC_COL_STATUS_CORRUPTED);
      return TRI_ERROR_ARANGO_CORRUPTED_COLLECTION;
    } catch (std::exception const& ex) {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME)
          << "caught exception while opening collection '" << collection->name()
          << "': " << ex.what();
      collection->setStatus(TRI_VOC_COL_STATUS_CORRUPTED);
      return TRI_ERROR_ARANGO_CORRUPTED_COLLECTION;
    } catch (...) {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME)
          << "caught unknown exception while opening collection '"
          << collection->name() << "'";
      collection->setStatus(TRI_VOC_COL_STATUS_CORRUPTED);
      return TRI_ERROR_ARANGO_CORRUPTED_COLLECTION;
    }

    // lock again to adjust the status
    locker.lockEventual();

    // no one else must have changed the status
    TRI_ASSERT(collection->status() == TRI_VOC_COL_STATUS_LOADING);

    collection->setStatus(TRI_VOC_COL_STATUS_LOADED);
    collection->load();

    // release the WRITE lock and try again
    locker.unlock();

    return loadCollection(collection, status, false);
  }

  std::string const colName(collection->name());
  LOG_TOPIC(ERR, arangodb::Logger::FIXME)
      << "unknown collection status " << collection->status() << " for '"
      << colName << "'";

  return TRI_set_errno(TRI_ERROR_INTERNAL);
}

/// @brief drops a collection, worker function
int TRI_vocbase_t::dropCollectionWorker(arangodb::LogicalCollection* collection,
                                        DropState& state, double timeout) {
  state = DROP_EXIT;
  std::string const colName(collection->name());

  double startTime = TRI_microtime();

  // do not acquire these locks instantly
  CONDITIONAL_WRITE_LOCKER(writeLocker, _collectionsLock,
                           basics::ConditionalLocking::DoNotLock);
  CONDITIONAL_WRITE_LOCKER(locker, collection->_lock,
                           basics::ConditionalLocking::DoNotLock);

  while (true) {
    TRI_ASSERT(!writeLocker.isLocked());
    TRI_ASSERT(!locker.isLocked());

    // block until we have acquired this lock
    writeLocker.lock();
    // we now have the one lock

    TRI_ASSERT(writeLocker.isLocked());

    if (locker.tryLock()) {
      // we now have both locks and can continue outside of this loop
      break;
    }

    // unlock the write locker so we don't block other operations
    writeLocker.unlock();

    TRI_ASSERT(!writeLocker.isLocked());
    TRI_ASSERT(!locker.isLocked());

    if (timeout >= 0.0 && TRI_microtime() > startTime + timeout) {
      events::DropCollection(colName, TRI_ERROR_LOCK_TIMEOUT);
      return TRI_ERROR_LOCK_TIMEOUT;
    }

    // sleep for a while
    std::this_thread::yield();
  }

  TRI_ASSERT(writeLocker.isLocked());
  TRI_ASSERT(locker.isLocked());

#if USE_PLAN_CACHE
  arangodb::aql::PlanCache::instance()->invalidate(this);
#endif
  arangodb::aql::QueryCache::instance()->invalidate(this);

  switch (collection->status()) {
    case TRI_VOC_COL_STATUS_DELETED: {
      // collection already deleted
      // mark collection as deleted
      unregisterCollection(collection);
      break;
    }
    case TRI_VOC_COL_STATUS_LOADING: {
      // collection is loading
      // loop until status changes
      // try again later
      state = DROP_AGAIN;
      break;
    }
    case TRI_VOC_COL_STATUS_UNLOADED: {
      // collection is unloaded
      StorageEngine* engine = EngineSelectorFeature::ENGINE;
      bool doSync =
          !engine->inRecovery() &&
          application_features::ApplicationServer::getFeature<DatabaseFeature>(
              "Database")
              ->forceSyncProperties();

      if (!collection->deleted()) {
        collection->setDeleted(true);
        try {
          engine->changeCollection(this, collection->cid(), collection, doSync);
        } catch (arangodb::basics::Exception const& ex) {
          collection->setDeleted(false);
          events::DropCollection(colName, ex.code());
          return ex.code();
        } catch (std::exception const&) {
          collection->setDeleted(false);
          events::DropCollection(colName, TRI_ERROR_INTERNAL);
          return TRI_ERROR_INTERNAL;
        }
      }

      collection->setStatus(TRI_VOC_COL_STATUS_DELETED);
      unregisterCollection(collection);

      locker.unlock();

      writeLocker.unlock();

      TRI_ASSERT(engine != nullptr);
      engine->dropCollection(this, collection);

      DropCollectionCallback(collection);
      break;
    }
    case TRI_VOC_COL_STATUS_LOADED:
    case TRI_VOC_COL_STATUS_UNLOADING: {
      // collection is loaded
      collection->setDeleted(true);

      StorageEngine* engine = EngineSelectorFeature::ENGINE;
      bool doSync =
          !engine->inRecovery() &&
          application_features::ApplicationServer::getFeature<DatabaseFeature>(
              "Database")
              ->forceSyncProperties();

      VPackBuilder builder;
      engine->getCollectionInfo(this, collection->cid(), builder, false, 0);
      arangodb::Result res = collection->updateProperties(
          builder.slice().get("parameters"), doSync);

      if (!res.ok()) {
        return res.errorNumber();
      }

      collection->setStatus(TRI_VOC_COL_STATUS_DELETED);
      unregisterCollection(collection);

      locker.unlock();
      writeLocker.unlock();

      engine->dropCollection(this, collection);
      state = DROP_PERFORM;
      break;
    }
    default: {
      // unknown status
      events::DropCollection(colName, TRI_ERROR_INTERNAL);
      return TRI_ERROR_INTERNAL;
    }
  }
  events::DropCollection(colName, TRI_ERROR_NO_ERROR);
  return TRI_ERROR_NO_ERROR;
}

/// @brief closes a database and all collections
void TRI_vocbase_t::shutdown() {
  // stop replication
  if (_replicationApplier != nullptr) {
    _replicationApplier->stop(false, true);
  }

  // mark all cursors as deleted so underlying collections can be freed soon
  _cursorRepository->garbageCollect(true);

  // mark all collection keys as deleted so underlying collections can be freed
  // soon
  _collectionKeys->garbageCollect(true);

  std::vector<arangodb::LogicalCollection*> collections;

  {
    READ_LOCKER(readLocker, _collectionsLock);
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
  // shutdownDatabase() stops all threads
  engine->shutdownDatabase(this);

  // this will signal the cleanup thread to do one last iteration
  setState(TRI_vocbase_t::State::SHUTDOWN_CLEANUP);

  {
    WRITE_LOCKER(readLocker, _collectionsLock);
    _collectionsByName.clear();
    _collectionsById.clear();
  }

  // free dead collections (already dropped but pointers still around)
  for (auto& collection : _deadCollections) {
    delete collection;
  }
  _deadCollections.clear();

  // free collections
  for (auto& collection : _collections) {
    collection->getPhysical()->close();
    delete collection;
  }
  _collections.clear();
}

/// @brief returns names of all known (document) collections
std::vector<std::string> TRI_vocbase_t::collectionNames() {
  std::vector<std::string> result;

  READ_LOCKER(readLocker, _collectionsLock);

  result.reserve(_collectionsByName.size());
  for (auto const& it : _collectionsByName) {
    result.emplace_back(it.first);
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

std::shared_ptr<VPackBuilder> TRI_vocbase_t::inventory(
    TRI_voc_tick_t maxTick, bool (*filter)(arangodb::LogicalCollection*, void*),
    void* data, bool shouldSort,
    std::function<bool(arangodb::LogicalCollection*,
                       arangodb::LogicalCollection*)>
        sortCallback) {
  std::vector<arangodb::LogicalCollection*> collections;

  // cycle on write-lock
  WRITE_LOCKER_EVENTUAL(writeLock, _inventoryLock);

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

    // In cluster case cids are not created by ticks but by cluster uniqIds
    if (!ServerState::instance()->isRunningInCluster() &&
        collection->cid() > maxTick) {
      // collection is too new
      continue;
    }

    // check if we want this collection
    if (filter != nullptr && !filter(collection, data)) {
      continue;
    }

    TRI_ASSERT(!builder->isClosed());
    StorageEngine* engine = EngineSelectorFeature::ENGINE;
    engine->getCollectionInfo(collection->vocbase(), collection->cid(),
                              *(builder.get()), true, maxTick);
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
LogicalCollection* TRI_vocbase_t::lookupCollection(std::string const& name) const {
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

/// @brief looks up a collection by name
LogicalCollection* TRI_vocbase_t::lookupCollectionNoLock(
    std::string const& name) const {
  if (name.empty()) {
    return nullptr;
  }

  // if collection name is passed as a stringified id, we'll use the lookupbyid
  // function
  // this is safe because collection names must not start with a digit
  if (name[0] >= '0' && name[0] <= '9') {
    TRI_voc_cid_t id = StringUtils::uint64(name);
    auto it = _collectionsById.find(id);

    if (it == _collectionsById.end()) {
      return nullptr;
    }
    return (*it).second;
  }

  // otherwise we'll look up the collection by name
  auto it = _collectionsByName.find(name);

  if (it == _collectionsByName.end()) {
    return nullptr;
  }
  return (*it).second;
}

/// @brief looks up a collection by identifier
LogicalCollection* TRI_vocbase_t::lookupCollection(TRI_voc_cid_t id) const {
  READ_LOCKER(readLocker, _collectionsLock);

  auto it = _collectionsById.find(id);
  if (it == _collectionsById.end()) {
    return nullptr;
  }
  return (*it).second;
}

/// @brief looks up a view by name
std::shared_ptr<LogicalView> TRI_vocbase_t::lookupView(
    std::string const& name) {
  if (name.empty()) {
    return std::shared_ptr<LogicalView>();
  }

  // if view name is passed as a stringified id, we'll use the lookupbyid
  // function
  // this is safe because view names must not start with a digit
  if (name[0] >= '0' && name[0] <= '9') {
    return lookupView(StringUtils::uint64(name));
  }

  // otherwise we'll look up the view by name
  READ_LOCKER(readLocker, _viewsLock);

  auto it = _viewsByName.find(name);

  if (it == _viewsByName.end()) {
    return std::shared_ptr<LogicalView>();
  }
  return (*it).second;
}

/// @brief looks up a view by identifier
std::shared_ptr<LogicalView> TRI_vocbase_t::lookupView(TRI_voc_cid_t id) {
  READ_LOCKER(readLocker, _viewsLock);

  auto it = _viewsById.find(id);

  if (it == _viewsById.end()) {
    return std::shared_ptr<LogicalView>();
  }
  return (*it).second;
}

/// @brief creates a new collection from parameter set
/// collection id (cid) is normally passed with a value of 0
/// this means that the system will assign a new collection id automatically
/// using a cid of > 0 is supported to import dumps from other servers etc.
/// but the functionality is not advertised
arangodb::LogicalCollection* TRI_vocbase_t::createCollection(
    VPackSlice parameters) {
  // check that the name does not contain any strange characters
  if (!LogicalCollection::IsAllowedName(parameters)) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_ILLEGAL_NAME);
  }

  // augment creation parameters
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  TRI_ASSERT(engine != nullptr);

  VPackBuilder merge;
  merge.openObject();
  engine->addParametersForNewCollection(merge, parameters);
  merge.close();

  merge = VPackCollection::merge(parameters, merge.slice(), true, false);
  parameters = merge.slice();

  READ_LOCKER(readLocker, _inventoryLock);

  // note: cid may be modified by this function call
  arangodb::LogicalCollection* collection = createCollectionWorker(parameters);

  if (collection == nullptr) {
    // something went wrong... must not continue
    return nullptr;
  }

  arangodb::Result res2 = engine->persistCollection(this, collection);
  // API compatibility, we always return the collection, even if creation
  // failed.

  return collection;
}

/// @brief unloads a collection
int TRI_vocbase_t::unloadCollection(arangodb::LogicalCollection* collection,
                                    bool force) {
  {
    WRITE_LOCKER_EVENTUAL(locker, collection->_lock);

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
      return TRI_ERROR_INTERNAL;
    }

    // mark collection as unloading
    collection->setStatus(TRI_VOC_COL_STATUS_UNLOADING);
  }  // release locks

  collection->unload();

  // wake up the cleanup thread
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  engine->unloadCollection(this, collection);

  return TRI_ERROR_NO_ERROR;
}

/// @brief drops a collection
int TRI_vocbase_t::dropCollection(arangodb::LogicalCollection* collection,
                                  bool allowDropSystem, double timeout) {
  TRI_ASSERT(collection != nullptr);

  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  if (!allowDropSystem && collection->isSystem() && !engine->inRecovery()) {
    // prevent dropping of system collections
    return TRI_set_errno(TRI_ERROR_FORBIDDEN);
  }

  while (true) {
    DropState state = DROP_EXIT;
    int res;
    {
      READ_LOCKER(readLocker, _inventoryLock);

      res = dropCollectionWorker(collection, state, timeout);
    }

    if (state == DROP_PERFORM) {
      if (engine->inRecovery()) {
        DropCollectionCallback(collection);
      } else {
        collection->deferDropCollection(DropCollectionCallback);
        // wake up the cleanup thread
        engine->signalCleanup(collection->vocbase());
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
int TRI_vocbase_t::renameCollection(arangodb::LogicalCollection* collection,
                                    std::string const& newName,
                                    bool doOverride) {
  if (collection->isSystem()) {
    return TRI_set_errno(TRI_ERROR_FORBIDDEN);
  }

  // lock collection because we are going to copy its current name
  std::string oldName = collection->name();

  // old name should be different

  // check if names are actually different
  if (oldName == newName) {
    return TRI_ERROR_NO_ERROR;
  }

  if (!doOverride) {
    bool isSystem;
    isSystem = LogicalCollection::IsSystemName(oldName);

    if (isSystem && !LogicalCollection::IsSystemName(newName)) {
      // a system collection shall not be renamed to a non-system collection
      // name
      return TRI_set_errno(TRI_ERROR_ARANGO_ILLEGAL_NAME);
    } else if (!isSystem && LogicalCollection::IsSystemName(newName)) {
      // a non-system collection shall not be renamed to a system collection
      // name
      return TRI_set_errno(TRI_ERROR_ARANGO_ILLEGAL_NAME);
    }

    if (!LogicalCollection::IsAllowedName(isSystem, newName)) {
      return TRI_set_errno(TRI_ERROR_ARANGO_ILLEGAL_NAME);
    }
  }

  READ_LOCKER(readLocker, _inventoryLock);

  CONDITIONAL_WRITE_LOCKER(writeLocker, _collectionsLock, false);
  CONDITIONAL_WRITE_LOCKER(locker, collection->_lock, false);

  while (true) {
    TRI_ASSERT(!writeLocker.isLocked());
    TRI_ASSERT(!locker.isLocked());

    // block until we have acquired this lock
    writeLocker.lock();
    // we now have the one lock

    TRI_ASSERT(writeLocker.isLocked());

    if (locker.tryLock()) {
      // we now have both locks and can continue outside of this loop
      break;
    }

    // unlock the write locker so we don't block other operations
    writeLocker.unlock();

    TRI_ASSERT(!writeLocker.isLocked());
    TRI_ASSERT(!locker.isLocked());

    // sleep for a while
    std::this_thread::yield();
  }

  TRI_ASSERT(writeLocker.isLocked());
  TRI_ASSERT(locker.isLocked());

  // Check for duplicate name
  auto other = lookupCollectionNoLock(newName);

  if (other != nullptr) {
    return TRI_ERROR_ARANGO_DUPLICATE_NAME;
  }

  int res = collection->rename(newName);

  if (res != TRI_ERROR_NO_ERROR) {
    // Renaming failed
    return res;
  }

  // The collection is renamed. Now swap cache entries.
  auto it2 = _collectionsByName.emplace(newName, collection);
  TRI_ASSERT(it2.second);

  try {
    _collectionsByName.erase(oldName);
  } catch (...) {
    _collectionsByName.erase(newName);
    throw;
  }
  TRI_ASSERT(_collectionsByName.size() == _collectionsById.size());

  locker.unlock();
  writeLocker.unlock();

  // invalidate all entries for the two collections
  arangodb::aql::PlanCache::instance()->invalidate(this);
  arangodb::aql::QueryCache::instance()->invalidate(
      this, std::vector<std::string>{oldName, newName});

  // Tell the engine.
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  TRI_ASSERT(engine != nullptr);
  arangodb::Result res2 = engine->renameCollection(this, collection, oldName);

  return res2.errorNumber();
}

/// @brief locks a collection for usage, loading or manifesting it
int TRI_vocbase_t::useCollection(arangodb::LogicalCollection* collection,
                                 TRI_vocbase_col_status_e& status) {
  return loadCollection(collection, status);
}

/// @brief locks a (document) collection for usage by id
arangodb::LogicalCollection* TRI_vocbase_t::useCollection(
    TRI_voc_cid_t cid, TRI_vocbase_col_status_e& status) {
  // check that we have an existing name
  arangodb::LogicalCollection* collection = nullptr;
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
arangodb::LogicalCollection* TRI_vocbase_t::useCollection(
    std::string const& name, TRI_vocbase_col_status_e& status) {
  // check that we have an existing name
  arangodb::LogicalCollection* collection = nullptr;

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
void TRI_vocbase_t::releaseCollection(arangodb::LogicalCollection* collection) {
  collection->_lock.unlock();
}

/// @brief creates a new view, worker function
std::shared_ptr<arangodb::LogicalView> TRI_vocbase_t::createViewWorker(
    VPackSlice parameters, TRI_voc_cid_t& id) {
  std::string name = arangodb::basics::VelocyPackHelper::getStringValue(
      parameters, "name", "");

  // check that the name does not contain any strange characters
  if (!LogicalView::IsAllowedName(name)) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_ILLEGAL_NAME);
  }

  std::string type = arangodb::basics::VelocyPackHelper::getStringValue(
      parameters, "type", "");

  ViewTypesFeature* viewTypesFeature =
      application_features::ApplicationServer::getFeature<ViewTypesFeature>(
          "ViewTypes");

  // will throw if type is invalid
  ViewCreator& creator = viewTypesFeature->creator(type);
  // Try to create a new view. This is not registered yet
  std::shared_ptr<arangodb::LogicalView> view =
      std::make_shared<arangodb::LogicalView>(this, parameters);
  TRI_ASSERT(view != nullptr);

  WRITE_LOCKER(writeLocker, _viewsLock);

  auto it = _viewsByName.find(name);

  if (it != _viewsByName.end()) {
    events::CreateView(name, TRI_ERROR_ARANGO_DUPLICATE_NAME);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DUPLICATE_NAME);
  }

  registerView(basics::ConditionalLocking::DoNotLock, view);
  try {
    // id might have been assigned
    id = view->id();

    // now let's actually create the backing implementation
    view->spawnImplementation(creator, parameters, true);

    // Let's try to persist it.
    view->persistPhysicalView();

    events::CreateView(name, TRI_ERROR_NO_ERROR);
    return view;
  } catch (...) {
    unregisterView(view);
    throw;
  }
}

/// @brief creates a new view from parameter set
/// view id is normally passed with a value of 0
/// this means that the system will assign a new id automatically
/// using a cid of > 0 is supported to import dumps from other servers etc.
/// but the functionality is not advertised
std::shared_ptr<arangodb::LogicalView> TRI_vocbase_t::createView(
    VPackSlice parameters, TRI_voc_cid_t id) {
  READ_LOCKER(readLocker, _inventoryLock);

  // note: id may be modified by this function call
  std::shared_ptr<LogicalView> view = createViewWorker(parameters, id);

  if (view == nullptr) {
    // something went wrong... must not continue
    return nullptr;
  }

  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  TRI_ASSERT(engine != nullptr);
  // TODO Review
  arangodb::Result res2 = engine->persistView(this, view.get());
  // API compatibility, we always return the view, even if creation failed.

  return view;
}

int TRI_vocbase_t::dropView(std::string const& name) {
  std::shared_ptr<LogicalView> view = lookupView(name);

  if (view == nullptr) {
    return TRI_ERROR_ARANGO_VIEW_NOT_FOUND;
  }

  return dropView(view);
}

/// @brief drops a view
int TRI_vocbase_t::dropView(std::shared_ptr<arangodb::LogicalView> view) {
  TRI_ASSERT(view != nullptr);

  READ_LOCKER(readLocker, _inventoryLock);

  // do not acquire these locks instantly
  CONDITIONAL_WRITE_LOCKER(writeLocker, _viewsLock,
                           basics::ConditionalLocking::DoNotLock);
  CONDITIONAL_WRITE_LOCKER(locker, view->_lock,
                           basics::ConditionalLocking::DoNotLock);

  while (true) {
    TRI_ASSERT(!writeLocker.isLocked());
    TRI_ASSERT(!locker.isLocked());

    // block until we have acquired this lock
    writeLocker.lock();
    // we now have the one lock

    TRI_ASSERT(writeLocker.isLocked());

    if (locker.tryLock()) {
      // we now have both locks and can continue outside of this loop
      break;
    }

    // unlock the write locker so we don't block other operations
    writeLocker.unlock();

    TRI_ASSERT(!writeLocker.isLocked());
    TRI_ASSERT(!locker.isLocked());

    // sleep for a while
    std::this_thread::yield();
  }

  TRI_ASSERT(writeLocker.isLocked());
  TRI_ASSERT(locker.isLocked());

  arangodb::aql::PlanCache::instance()->invalidate(this);
  arangodb::aql::QueryCache::instance()->invalidate(this);

  view->drop();
  /*
  VPackBuilder b;
  b.openObject();
  view->toVelocyPack(b, true, true);
  b.close();

  bool doSync =
      application_features::ApplicationServer::getFeature<DatabaseFeature>(
          "Database")
          ->forceSyncProperties();
  view->updateProperties(b.slice(), doSync);
*/
  unregisterView(view);

  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  engine->dropView(this, view.get());

  locker.unlock();
  writeLocker.unlock();

  events::DropView(view->name(), TRI_ERROR_NO_ERROR);

  return TRI_ERROR_NO_ERROR;
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

  // do a final cleanup of collections
  for (auto& it : _collections) {
    delete it;
  }
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

void TRI_vocbase_t::garbageCollectReplicationClients(double ttl) {
  WRITE_LOCKER(writeLocker, _replicationClientsLock);

  try {
    double now = TRI_microtime();
    auto it = _replicationClients.cbegin();
    while (it != _replicationClients.cend()) {
      double lastUpdate = it->second.first;
      double diff = now - lastUpdate;
      if (diff > ttl) {
        it = _replicationClients.erase(it);
      } else {
        ++it;
      }
    }
  } catch (...) {
    // silently fail...
    // all we would be missing is the progress information of a slave
  }
}

std::vector<std::shared_ptr<arangodb::LogicalView>> TRI_vocbase_t::views() {
  std::vector<std::shared_ptr<arangodb::LogicalView>> views;

  {
    READ_LOCKER(readLocker, _viewsLock);
    views.reserve(_viewsById.size());
    for (auto const& it : _viewsById) {
      views.emplace_back(it.second);
    }
  }
  return views;
}

void TRI_vocbase_t::processCollections(
    std::function<void(LogicalCollection*)> const& cb, bool includeDeleted) {
  READ_LOCKER(readLocker, _collectionsLock);

  if (includeDeleted) {
    for (auto const& it : _collections) {
      cb(it);
    }
  } else {
    for (auto const& it : _collectionsById) {
      cb(it.second);
    }
  }
}

std::vector<arangodb::LogicalCollection*> TRI_vocbase_t::collections(
    bool includeDeleted) {
  std::vector<arangodb::LogicalCollection*> collections;

  {
    READ_LOCKER(readLocker, _collectionsLock);
    if (includeDeleted) {
      // return deleted collections as well. the cleanup thread needs them
      collections.reserve(_collections.size());
      for (auto const& it : _collections) {
        collections.emplace_back(it);
      }
    } else {
      collections.reserve(_collectionsById.size());
      for (auto const& it : _collectionsById) {
        collections.emplace_back(it.second);
      }
    }
  }
  return collections;
}

/// @brief extract the _rev attribute from a slice
TRI_voc_rid_t TRI_ExtractRevisionId(VPackSlice slice) {
  slice = slice.resolveExternal();
  TRI_ASSERT(slice.isObject());

  VPackSlice r(slice.get(StaticStrings::RevString));
  if (r.isString()) {
    VPackValueLength l;
    char const* p = r.getString(l);
    return TRI_StringToRid(p, l, false);
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
        (key != StaticStrings::KeyString && key != StaticStrings::IdString &&
         key != StaticStrings::RevString)) {
      builder.add(key.data(), key.size(), it.value());
    }
    it.next();
  }
}

/// @brief sanitize an object, given as slice, builder must contain an
/// open object which will remain open. also excludes _from and _to
void TRI_SanitizeObjectWithEdges(VPackSlice const slice,
                                 VPackBuilder& builder) {
  TRI_ASSERT(slice.isObject());
  VPackObjectIterator it(slice, true);
  while (it.valid()) {
    StringRef key(it.key());
    if (key.empty() || key[0] != '_' ||
        (key != StaticStrings::KeyString && key != StaticStrings::IdString &&
         key != StaticStrings::RevString && key != StaticStrings::FromString &&
         key != StaticStrings::ToString)) {
      builder.add(key.data(), key.length(), it.value());
    }
    it.next();
  }
}

/// @brief Convert a revision ID to a string
constexpr static TRI_voc_rid_t tickLimit =
    static_cast<TRI_voc_rid_t>(2016ULL - 1970ULL) * 1000ULL * 60ULL * 60ULL *
    24ULL * 365ULL;

std::string TRI_RidToString(TRI_voc_rid_t rid) {
  if (rid <= tickLimit) {
    return arangodb::basics::StringUtils::itoa(rid);
  }
  return HybridLogicalClock::encodeTimeStamp(rid);
}

/// @brief Convert a string into a revision ID, no check variant
TRI_voc_rid_t TRI_StringToRid(char const* p, size_t len, bool warn) {
  bool isOld;
  return TRI_StringToRid(p, len, isOld, warn);
}

/// @brief Convert a string into a revision ID, returns 0 if format invalid
TRI_voc_rid_t TRI_StringToRid(std::string const& ridStr, bool& isOld,
                              bool warn) {
  return TRI_StringToRid(ridStr.c_str(), ridStr.size(), isOld, warn);
}

/// @brief Convert a string into a revision ID, returns 0 if format invalid
TRI_voc_rid_t TRI_StringToRid(char const* p, size_t len, bool& isOld,
                              bool warn) {
  if (len > 0 && *p >= '1' && *p <= '9') {
    TRI_voc_rid_t r = arangodb::basics::StringUtils::uint64_check(p, len);
    if (warn && r > tickLimit) {
      // An old tick value that could be confused with a time stamp
      LOG_TOPIC(WARN, arangodb::Logger::FIXME)
          << "Saw old _rev value that could be confused with a time stamp!";
    }
    isOld = true;
    return r;
  }
  isOld = false;
  return HybridLogicalClock::decodeTimeStamp(p, len);
}
