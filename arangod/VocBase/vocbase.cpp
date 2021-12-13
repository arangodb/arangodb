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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "vocbase.h"

#include <algorithm>
#include <chrono>
#include <cinttypes>
#include <exception>
#include <memory>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include <velocypack/Collection.h>
#include <velocypack/Slice.h>
#include <velocypack/Utf8Helper.h>
#include <velocypack/Value.h>
#include <velocypack/ValueType.h>
#include <velocypack/velocypack-aliases.h>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/PlanCache.h"
#include "Aql/QueryCache.h"
#include "Aql/QueryList.h"
#include "Auth/Common.h"
#include "Basics/Exceptions.h"
#include "Basics/HybridLogicalClock.h"
#include "Basics/Locking.h"
#include "Basics/NumberUtils.h"
#include "Basics/ReadLocker.h"
#include "Basics/RecursiveLocker.h"
#include "Basics/ScopeGuard.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Basics/debugging.h"
#include "Basics/error.h"
#include "Basics/system-functions.h"
#include "Basics/voc-errors.h"
#include "Containers/Helpers.h"
#include "Cluster/ServerState.h"
#include "Indexes/Index.h"
#include "Logger/LogContextKeys.h"
#include "Logger/LogMacros.h"
#include "Replication/DatabaseReplicationApplier.h"
#include "Replication/ReplicationClients.h"
#include "Replication2/LoggerContext.h"
#include "Replication2/ReplicatedLog/ILogParticipant.h"
#include "Replication2/ReplicatedLog/LogCore.h"
#include "Replication2/ReplicatedLog/LogFollower.h"
#include "Replication2/ReplicatedLog/LogLeader.h"
#include "Replication2/ReplicatedLog/LogStatus.h"
#include "Replication2/ReplicatedLog/PersistedLog.h"
#include "Replication2/ReplicatedLog/ReplicatedLog.h"
#include "Replication2/ReplicatedLog/ReplicatedLogFeature.h"
#include "Replication2/ReplicatedLog/ReplicatedLogMetrics.h"
#include "Replication2/Version.h"
#include "Metrics/Counter.h"
#include "Metrics/Gauge.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "Scheduler/SchedulerFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/PhysicalCollection.h"
#include "StorageEngine/StorageEngine.h"
#include "Transaction/ClusterUtils.h"
#include "Utils/CursorRepository.h"
#include "Utils/Events.h"
#include "Utils/ExecContext.h"
#include "Utils/VersionTracker.h"
#include "Utilities/NameValidator.h"
#include "V8Server/v8-user-structures.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalDataSource.h"
#include "VocBase/LogicalView.h"

#include <thread>

using namespace arangodb;
using namespace arangodb::basics;

struct arangodb::VocBaseLogManager {
  explicit VocBaseLogManager(application_features::ApplicationServer& server, DatabaseID database)
      : _server(server),
        _logContext(LoggerContext{Logger::REPLICATION2}.with<logContextKeyDatabaseName>(
            std::move(database))) {}

  [[nodiscard]] auto getReplicatedLogById(replication2::LogId id) const
      -> std::shared_ptr<replication2::replicated_log::ReplicatedLog const> {
    auto guard = _guardedData.getLockedGuard();
    if (auto iter = guard->logs.find(id); iter != guard->logs.end()) {
      return iter->second;
    }
    THROW_ARANGO_EXCEPTION_FORMAT(TRI_ERROR_REPLICATION_REPLICATED_LOG_NOT_FOUND,
                                  "replicated log %" PRIu64 " not found", id.id());
  }

  [[nodiscard]] auto getReplicatedLogById(replication2::LogId id)
      -> std::shared_ptr<replication2::replicated_log::ReplicatedLog> {
    auto guard = _guardedData.getLockedGuard();
    if (auto iter = guard->logs.find(id); iter != guard->logs.end()) {
      return iter->second;
    }
    THROW_ARANGO_EXCEPTION_FORMAT(TRI_ERROR_REPLICATION_REPLICATED_LOG_NOT_FOUND,
                                  "replicated log %" PRIu64 " not found", id.id());
  }

  auto resignAll() {
    auto guard = _guardedData.getLockedGuard();
    for (auto&& [id, log] : guard->logs) {
      auto core = log->drop();
      core.reset();
    }
    guard->logs.clear();
  }

  [[nodiscard]] auto createReplicatedLog(TRI_vocbase_t& vocbase, replication2::LogId id,
                                         std::optional<std::string> const& collectionName)
      -> arangodb::ResultT<std::shared_ptr<replication2::replicated_log::ReplicatedLog>> {
    auto guard = _guardedData.getLockedGuard();
    return createReplicatedLogOnData(vocbase, id, collectionName, guard.get(),
                                     _logContext, _server);
  }

  [[nodiscard]] auto ensureReplicatedLog(TRI_vocbase_t& vocbase, replication2::LogId id,
                                         std::optional<std::string> const& collectionName)
      -> arangodb::ResultT<std::shared_ptr<replication2::replicated_log::ReplicatedLog>> {
    auto guard = _guardedData.getLockedGuard();

    if (auto iter = guard->logs.find(id); iter != guard->logs.end()) {
      return iter->second;
    }

    return createReplicatedLogOnData(vocbase, id, collectionName, guard.get(),
                                     _logContext, _server);
  }

  [[nodiscard]] auto dropReplicatedLog(TRI_vocbase_t& vocbase,
                                       arangodb::replication2::LogId id) -> arangodb::Result {
    LOG_CTX("658c7", DEBUG, _logContext) << "Dropping replicated log " << id;

    StorageEngine& engine = _server.getFeature<EngineSelectorFeature>().engine();

    auto result = _guardedData.doUnderLock([&](GuardedData& data) {
      if (auto iter = data.logs.find(id); iter != data.logs.end()) {
        auto core = iter->second->drop();
        auto res = engine.dropReplicatedLog(vocbase, std::move(*core).releasePersistedLog());
        if (res.fail()) {
          return res;
        }

        // Now we can drop the persisted log
        data.logs.erase(iter);
      } else {
        return Result(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
      }

      return Result();
    });

    if (result.ok()) {
      _server.getFeature<ReplicatedLogFeature>().metrics()->replicatedLogNumber->fetch_sub(1);
      _server.getFeature<ReplicatedLogFeature>()
          .metrics()
          ->replicatedLogDeletionNumber->count();
    }
    return result;
  }

  [[nodiscard]] auto getReplicatedLogs() const
      -> std::unordered_map<arangodb::replication2::LogId, arangodb::replication2::replicated_log::LogStatus> {
    std::unordered_map<arangodb::replication2::LogId, arangodb::replication2::replicated_log::LogStatus> result;
    auto guard = _guardedData.getLockedGuard();
    for (auto& [id, log] : guard->logs) {
      result.emplace(id, log->getParticipant()->getStatus());
    }
    return result;
  }

  application_features::ApplicationServer& _server;
  LoggerContext const _logContext;

  struct GuardedData {
    std::unordered_map<arangodb::replication2::LogId, std::shared_ptr<arangodb::replication2::replicated_log::ReplicatedLog>> logs;
  };
  Guarded<GuardedData> _guardedData;

 private:
  static auto createReplicatedLogOnData(TRI_vocbase_t& vocbase, replication2::LogId id,
                                        std::optional<std::string> const& collectionName,
                                        GuardedData& data, LoggerContext const& outerLogContext,
                                        application_features::ApplicationServer& server)
      -> arangodb::ResultT<std::shared_ptr<replication2::replicated_log::ReplicatedLog>> {
    LOG_CTX("04b14", DEBUG, outerLogContext) << "Creating replicated log " << id;

    StorageEngine& engine = server.getFeature<EngineSelectorFeature>().engine();

    auto const logContext = std::invoke([&] {
      if (collectionName) {
        return outerLogContext.with<logContextKeyCollectionName>(*collectionName);
      } else {
        return outerLogContext;
      }
    });

    auto result = std::invoke(
        [&]() -> arangodb::ResultT<std::shared_ptr<replication2::replicated_log::ReplicatedLog>> {
          if (auto iter = data.logs.find(id); iter == data.logs.end()) {
            auto&& persistedLog = engine.createReplicatedLog(vocbase, id);
            if (persistedLog.fail()) {
              return persistedLog.result();
            }
            auto&& logCore = std::make_unique<replication2::replicated_log::LogCore>(
                std::move(persistedLog.get()));
            auto it = data.logs.try_emplace(
                id, std::make_shared<arangodb::replication2::replicated_log::ReplicatedLog>(
                        std::move(logCore),
                        server.getFeature<ReplicatedLogFeature>().metrics(),
                        server.getFeature<ReplicatedLogFeature>().options(), logContext));

            return it.first->second;
          } else {
            return Result(TRI_ERROR_ARANGO_DUPLICATE_IDENTIFIER);
          }
        });

    if (result.ok()) {
      server.getFeature<ReplicatedLogFeature>().metrics()->replicatedLogNumber->fetch_add(1);
      server.getFeature<ReplicatedLogFeature>()
          .metrics()
          ->replicatedLogCreationNumber->count();
    }
    return result;
  }
};

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
    if (_refCount.compare_exchange_weak(expected, updated, std::memory_order_release,
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
void TRI_vocbase_t::release() noexcept {
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

bool TRI_vocbase_t::isSystem() const {
  return _info.getName() == StaticStrings::SystemDatabase;
}

void TRI_vocbase_t::checkCollectionInvariants() const {
  TRI_ASSERT(_dataSourceByName.size() == _dataSourceById.size());
  TRI_ASSERT(_dataSourceByUuid.size() == _dataSourceById.size());
}

/// @brief adds a new collection
/// caller must hold _dataSourceLock in write mode or set doLock
void TRI_vocbase_t::registerCollection(bool doLock,
                                       std::shared_ptr<arangodb::LogicalCollection> const& collection) {
  std::string name = collection->name();
  DataSourceId cid = collection->id();
  {
    RECURSIVE_WRITE_LOCKER_NAMED(writeLocker, _dataSourceLock,
                                 _dataSourceLockWriteOwner, doLock);

    checkCollectionInvariants();
    auto sg = arangodb::scopeGuard([&]() noexcept { checkCollectionInvariants(); });

    // check name
    auto it = _dataSourceByName.try_emplace(name, collection);

    if (!it.second) {
      std::string msg;
      msg.append(std::string("duplicate entry for collection name '") + name +
                 "'. collection id " + std::to_string(cid.id()) +
                 " has same name as already added collection " +
                 std::to_string(_dataSourceByName[name]->id().id()));
      LOG_TOPIC("405f9", ERR, arangodb::Logger::FIXME) << msg;

      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DUPLICATE_NAME, msg);
    }

    // check collection identifier
    try {
      auto it2 = _dataSourceById.try_emplace(cid, collection);

      if (!it2.second) {
        std::string msg;
        msg.append(std::string("duplicate collection identifier ") +
                   std::to_string(collection->id().id()) + " for name '" +
                   name + "'");
        LOG_TOPIC("0ef12", ERR, arangodb::Logger::FIXME) << msg;

        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DUPLICATE_IDENTIFIER, msg);
      }
    } catch (...) {
      _dataSourceByName.erase(name);
      throw;
    }

    try {
      auto it2 = _dataSourceByUuid.try_emplace(collection->guid(), collection);

      if (!it2.second) {
        std::string msg;
        msg.append(std::string("duplicate entry for collection uuid '") +
                   collection->guid() + "'");
        LOG_TOPIC("d4958", ERR, arangodb::Logger::FIXME) << msg;

        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DUPLICATE_IDENTIFIER, msg);
      }
    } catch (...) {
      _dataSourceByName.erase(name);
      _dataSourceById.erase(cid);
      throw;
    }

    try {
      _collections.emplace_back(collection);
    } catch (...) {
      _dataSourceByName.erase(name);
      _dataSourceById.erase(cid);
      _dataSourceByUuid.erase(collection->guid());
      throw;
    }

    collection->setStatus(TRI_VOC_COL_STATUS_LOADED);
  }
}

/// @brief removes a collection name from the global list of collections
/// This function is called when a collection is dropped.
/// NOTE: You need a writelock on _dataSourceLock
void TRI_vocbase_t::unregisterCollection(arangodb::LogicalCollection& collection) {
  // pre-condition
  checkCollectionInvariants();

  auto itr = _dataSourceById.find(collection.id());

  if (itr == _dataSourceById.end() ||
      itr->second->category() != LogicalCollection::category()) {
    return;  // no such collection
  }

  TRI_ASSERT(std::dynamic_pointer_cast<arangodb::LogicalCollection>(itr->second));

  // only if we find the collection by its id, we can delete it by name
  _dataSourceById.erase(itr);

  // this is because someone else might have created a new collection with the
  // same name, but with a different id
  _dataSourceByName.erase(collection.name());
  _dataSourceByUuid.erase(collection.guid());

  // post-condition
  checkCollectionInvariants();
}

/// @brief adds a new view
/// caller must hold _viewLock in write mode or set doLock
void TRI_vocbase_t::registerView(bool doLock,
                                 std::shared_ptr<arangodb::LogicalView> const& view) {
  TRI_ASSERT(false == !view);
  auto& name = view->name();
  auto id = view->id();

  {
    RECURSIVE_WRITE_LOCKER_NAMED(writeLocker, _dataSourceLock,
                                 _dataSourceLockWriteOwner, doLock);

    // check name
    auto it = _dataSourceByName.emplace(name, view);

    if (!it.second) {
      LOG_TOPIC("560a6", ERR, arangodb::Logger::FIXME)
          << "duplicate entry for view name '" << name << "'";
      LOG_TOPIC("0168f", ERR, arangodb::Logger::FIXME)
          << "view id " << id << " has same name as already added view "
          << _dataSourceByName[name]->id();

      THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DUPLICATE_NAME);
    }

    // check id
    try {
      auto it2 = _dataSourceById.emplace(id, view);

      if (!it2.second) {
        _dataSourceByName.erase(name);

        LOG_TOPIC("cb53a", ERR, arangodb::Logger::FIXME)
            << "duplicate view identifier '" << view->id() << "' for name '"
            << name << "'";

        THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DUPLICATE_IDENTIFIER);
      }
    } catch (...) {
      _dataSourceByName.erase(name);
      throw;
    }

    try {
      auto it2 = _dataSourceByUuid.emplace(view->guid(), view);

      if (!it2.second) {
        LOG_TOPIC("a30ae", ERR, arangodb::Logger::FIXME)
            << "duplicate view globally-unique identifier '" << view->guid()
            << "' for name '" << name << "'";

        THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DUPLICATE_IDENTIFIER);
      }
    } catch (...) {
      _dataSourceByName.erase(name);
      _dataSourceById.erase(id);
      throw;
    }

    checkCollectionInvariants();
  }
}

/// @brief removes a views name from the global list of views
/// This function is called when a view is dropped.
/// NOTE: You need a writelock on _dataSourceLock
bool TRI_vocbase_t::unregisterView(arangodb::LogicalView const& view) {
  // pre-condition
  checkCollectionInvariants();

  auto itr = _dataSourceById.find(view.id());

  if (itr == _dataSourceById.end() || itr->second->category() != LogicalView::category()) {
    return true;  // no such view
  }

  TRI_ASSERT(std::dynamic_pointer_cast<arangodb::LogicalView>(itr->second));

  // only if we find the collection by its id, we can delete it by name
  _dataSourceById.erase(itr);

  // this is because someone else might have created a new view with the
  // same name, but with a different id
  _dataSourceByName.erase(view.name());
  _dataSourceByUuid.erase(view.guid());

  // post-condition
  checkCollectionInvariants();

  return true;
}

/// @brief drops a collection
/*static */ bool TRI_vocbase_t::dropCollectionCallback(arangodb::LogicalCollection& collection) {
  {
    WRITE_LOCKER_EVENTUAL(statusLock, collection.statusLock());

    if (TRI_VOC_COL_STATUS_DELETED != collection.status()) {
      LOG_TOPIC("57377", ERR, arangodb::Logger::FIXME)
          << "someone resurrected the collection '" << collection.name() << "'";
      return false;
    }
  }  // release status lock

  // remove from list of collections
  auto& vocbase = collection.vocbase();

  {
    RECURSIVE_WRITE_LOCKER(vocbase._dataSourceLock, vocbase._dataSourceLockWriteOwner);
    auto it = vocbase._collections.begin();

    for (auto end = vocbase._collections.end(); it != end; ++it) {
      if (it->get() == &collection) {
        break;
      }
    }

    if (it != vocbase._collections.end()) {
      auto col = *it;

      vocbase._collections.erase(it);

      // we need to clean up the pointers later so we insert it into this vector
      try {
        vocbase._deadCollections.emplace_back(col);
      } catch (...) {
      }
    }
  }

  collection.drop();

  return true;
}

/// @brief creates a new collection, worker function
std::shared_ptr<arangodb::LogicalCollection> TRI_vocbase_t::createCollectionWorker(VPackSlice parameters) {
  std::string const name =
      arangodb::basics::VelocyPackHelper::getStringValue(parameters, StaticStrings::DataSourceName, "");
  TRI_ASSERT(!name.empty());

  // Try to create a new collection. This is not registered yet
  auto collection =
      std::make_shared<arangodb::LogicalCollection>(*this, parameters, false);

  return persistCollection(collection);
}

std::shared_ptr<arangodb::LogicalCollection> TRI_vocbase_t::persistCollection(std::shared_ptr<arangodb::LogicalCollection>& collection){
  RECURSIVE_WRITE_LOCKER(_dataSourceLock, _dataSourceLockWriteOwner);

  // reserve room for the new collection
  arangodb::containers::Helpers::reserveSpace(_collections, 8);
  arangodb::containers::Helpers::reserveSpace(_deadCollections, 8);

  auto it = _dataSourceByName.find(collection->name());

  if (it != _dataSourceByName.end()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DUPLICATE_NAME);
  }

  registerCollection(basics::ConditionalLocking::DoNotLock, collection);

  try {
    collection->setStatus(TRI_VOC_COL_STATUS_LOADED);
    // set collection version to latest version, as the collection is just created
    collection->setVersion(LogicalCollection::currentVersion());

    // Let's try to persist it.
    collection->persistPhysicalCollection();

    return collection;
  } catch (...) {
    unregisterCollection(*collection);
    throw;
  }
}

/// @brief loads an existing collection
/// Note that this will READ lock the collection. You have to release the
/// collection lock by yourself.
arangodb::Result TRI_vocbase_t::loadCollection(arangodb::LogicalCollection& collection,
                                               bool checkPermissions) {
  TRI_ASSERT(collection.id().isSet());

  if (checkPermissions) {
    std::string const& dbName = _info.getName();
    if (!ExecContext::current().canUseCollection(dbName, collection.name(), auth::Level::RO)) {
      return {TRI_ERROR_FORBIDDEN, std::string("cannot access collection '") +
                                       collection.name() + "'"};
    }
  }

  // read lock
  // check if the collection is already loaded
  READ_LOCKER_EVENTUAL(locker, collection.statusLock());
  TRI_vocbase_col_status_e status = collection.status();

  if (status == TRI_VOC_COL_STATUS_LOADED) {
    // DO NOT release the lock
    locker.steal();
    return {};
  }

  if (status == TRI_VOC_COL_STATUS_DELETED) {
    return {TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
            std::string("collection '") + collection.name() + "' not found"};
  }

  if (status == TRI_VOC_COL_STATUS_CORRUPTED) {
    return {TRI_ERROR_ARANGO_CORRUPTED_COLLECTION};
  }

  TRI_ASSERT(false);
  return {TRI_ERROR_INTERNAL, "unknwon collection status"};
}

/// @brief drops a collection, worker function
ErrorCode TRI_vocbase_t::dropCollectionWorker(arangodb::LogicalCollection* collection,
                                              DropState& state, double timeout) {
  state = DROP_EXIT;
  std::string const colName(collection->name());

  StorageEngine& engine = server().getFeature<EngineSelectorFeature>().engine();
  engine.prepareDropCollection(*this, *collection);

  double endTime = TRI_microtime() + timeout;

  // do not acquire these locks instantly
  RECURSIVE_WRITE_LOCKER_NAMED(writeLocker, _dataSourceLock, _dataSourceLockWriteOwner,
                               basics::ConditionalLocking::DoNotLock);
  CONDITIONAL_WRITE_LOCKER(locker, collection->statusLock(),
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

    if (timeout > 0.0 && TRI_microtime() > endTime) {
      events::DropCollection(name(), colName, TRI_ERROR_LOCK_TIMEOUT);
      return TRI_ERROR_LOCK_TIMEOUT;
    }

    if (_server.isStopping()) {
      return TRI_ERROR_SHUTTING_DOWN;
    }

    engine.prepareDropCollection(*this, *collection);

    // sleep for a while
    std::this_thread::yield();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  TRI_ASSERT(writeLocker.isLocked());
  TRI_ASSERT(locker.isLocked());

#if USE_PLAN_CACHE
  arangodb::aql::PlanCache::instance()->invalidate(this);
#endif
  arangodb::aql::QueryCache::instance()->invalidate(this);
  std::string const& dbName = _info.getName();

  switch (collection->status()) {
    case TRI_VOC_COL_STATUS_DELETED: {
      // collection already deleted
      // mark collection as deleted
      unregisterCollection(*collection);
      break;
    }
    case TRI_VOC_COL_STATUS_LOADED: {
      // collection is loaded
      collection->deleted(true);

      VPackBuilder builder;

      engine.getCollectionInfo(*this, collection->id(), builder, false, 0);

      auto res = collection->properties(builder.slice().get("parameters"),
                                        false);  // always a full-update

      if (!res.ok()) {
        // TODO: Here we're only returning the errorNumber. The errorMessage is being ignored here. Needs refactor.
        events::DropCollection(dbName, colName, res.errorNumber());
        return res.errorNumber();
      }

      collection->setStatus(TRI_VOC_COL_STATUS_DELETED);
      unregisterCollection(*collection);

      locker.unlock();
      writeLocker.unlock();

      engine.dropCollection(*this, *collection);
      state = DROP_PERFORM;
      break;
    }
    default: {
      // unknown status
      events::DropCollection(dbName, colName, TRI_ERROR_INTERNAL);
      return TRI_ERROR_INTERNAL;
    }
  }
  events::DropCollection(dbName, colName, TRI_ERROR_NO_ERROR);
  return TRI_ERROR_NO_ERROR;
}

/// @brief stop operations in this vocbase. must be called prior to
/// shutdown to clean things up
void TRI_vocbase_t::stop() {
  try {
    // stop replication
    if (_replicationApplier != nullptr) {
      _replicationApplier->stopAndJoin();
    }

    // mark all cursors as deleted so underlying collections can be freed soon
    _cursorRepository->garbageCollect(true);

    // mark all collection keys as deleted so underlying collections can be
    // freed soon, we have to retry, since some of these collection keys might
    // currently still being in use:
  } catch (...) {
    // we are calling this on shutdown, and always want to go on from here
  }
}

/// @brief closes a database and all collections
void TRI_vocbase_t::shutdown() {
  this->stop();

  std::vector<std::shared_ptr<arangodb::LogicalCollection>> collections;

  {
    RECURSIVE_READ_LOCKER(_dataSourceLock, _dataSourceLockWriteOwner);
    collections = _collections;
  }

  // from here on, the vocbase is unusable, i.e. no collections can be
  // created/loaded etc.

  // starts unloading of collections
  for (auto& collection : collections) {
    WRITE_LOCKER_EVENTUAL(locker, collection->statusLock());
    collection->close();  // required to release indexes
  }

  {
    RECURSIVE_WRITE_LOCKER(_dataSourceLock, _dataSourceLockWriteOwner);

    checkCollectionInvariants();
    _dataSourceByName.clear();
    _dataSourceById.clear();
    _dataSourceByUuid.clear();
    checkCollectionInvariants();
  }

  _deadCollections.clear();

  // free collections
  for (auto& collection : _collections) {
    collection->getPhysical()->close();
  }

  _collections.clear();
  _logManager->resignAll();
}

/// @brief returns names of all known (document) collections
std::vector<std::string> TRI_vocbase_t::collectionNames() {
  std::vector<std::string> result;

  RECURSIVE_READ_LOCKER(_dataSourceLock, _dataSourceLockWriteOwner);

  result.reserve(_dataSourceByName.size());

  for (auto& entry : _dataSourceByName) {
    TRI_ASSERT(entry.second);

    if (entry.second->category() != LogicalCollection::category()) {
      continue;
    }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    auto view = std::dynamic_pointer_cast<arangodb::LogicalCollection>(entry.second);
    TRI_ASSERT(view);
#else
    auto view = std::static_pointer_cast<arangodb::LogicalCollection>(entry.second);
#endif

    result.emplace_back(entry.first);
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all known (document) collections with their parameters
/// and indexes, up to a specific tick value
/// while the collections are iterated over, there will be a global lock so
/// that there will be consistent view of collections & their properties
/// The list of collections will be sorted by type and then name
////////////////////////////////////////////////////////////////////////////////

void TRI_vocbase_t::inventory(VPackBuilder& result, TRI_voc_tick_t maxTick,
                              std::function<bool(arangodb::LogicalCollection const*)> const& nameFilter) {
  TRI_ASSERT(result.isOpenObject());

  std::vector<std::shared_ptr<arangodb::LogicalCollection>> collections;
  std::unordered_map<DataSourceId, std::shared_ptr<LogicalDataSource>> dataSourceById;

  // cycle on write-lock
  WRITE_LOCKER_EVENTUAL(writeLock, _inventoryLock);

  // copy collection pointers into vector so we can work with the copy without
  // the global lock
  {
    RECURSIVE_READ_LOCKER(_dataSourceLock, _dataSourceLockWriteOwner);
    collections = _collections;
    dataSourceById = _dataSourceById;
  }

  if (collections.size() > 1) {
    // sort by type first and then only name
    // sorting by type ensures that document collections are reported before
    // edge collections
    std::sort(collections.begin(), collections.end(),
              [](std::shared_ptr<LogicalCollection> const& lhs,
                 std::shared_ptr<LogicalCollection> const& rhs) -> bool {
                if (lhs->type() != rhs->type()) {
                  return lhs->type() < rhs->type();
                }

                return lhs->name() < rhs->name();
              });
  }

  ExecContext const& exec = ExecContext::current();

  result.add(VPackValue(arangodb::StaticStrings::Properties));
  result.openObject();
  _info.toVelocyPack(result);
  result.close();

  result.add("collections", VPackValue(VPackValueType::Array));
  std::string const& dbName = _info.getName();
  for (auto& collection : collections) {
    READ_LOCKER(readLocker, collection->statusLock());

    if (collection->status() == TRI_VOC_COL_STATUS_DELETED ||
        collection->status() == TRI_VOC_COL_STATUS_CORRUPTED) {
      // we do not need to care about deleted or corrupted collections
      continue;
    }

    // In cluster case cids are not created by ticks but by cluster uniqIds
    if (!ServerState::instance()->isRunningInCluster() && collection->id().id() > maxTick) {
      // collection is too new
      continue;
    }

    // check if we want this collection
    if (!nameFilter(collection.get())) {
      continue;
    }

    if (!exec.canUseCollection(dbName, collection->name(), auth::Level::RO)) {
      continue;
    }

    if (collection->id().id() <= maxTick) {
      collection->toVelocyPackForInventory(result);
    }
  }
  result.close();  // </collections>

  result.add("views", VPackValue(VPackValueType::Array, true));
  LogicalView::enumerate(*this, [&result](LogicalView::ptr const& view) -> bool {
    if (view) {
      result.openObject();
      view->properties(result, LogicalDataSource::Serialization::Inventory);
      result.close();
    }

    return true;
  });
  result.close();  // </views>
}

/// @brief looks up a collection by identifier
std::shared_ptr<arangodb::LogicalCollection> TRI_vocbase_t::lookupCollection(DataSourceId id) const noexcept {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  return std::dynamic_pointer_cast<arangodb::LogicalCollection>(lookupDataSource(id));
#else
  auto dataSource = lookupDataSource(id);

  return dataSource && dataSource->category() == LogicalCollection::category()
             ? std::static_pointer_cast<LogicalCollection>(dataSource)
             : nullptr;
#endif
}

/// @brief looks up a collection by name or stringified cid or uuid
std::shared_ptr<arangodb::LogicalCollection> TRI_vocbase_t::lookupCollection(
    std::string const& nameOrId) const noexcept {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  return std::dynamic_pointer_cast<arangodb::LogicalCollection>(lookupDataSource(nameOrId));
#else
  auto dataSource = lookupDataSource(nameOrId);

  return dataSource && dataSource->category() == LogicalCollection::category()
             ? std::static_pointer_cast<LogicalCollection>(dataSource)
             : nullptr;
#endif
}

/// @brief looks up a collection by uuid
std::shared_ptr<arangodb::LogicalCollection> TRI_vocbase_t::lookupCollectionByUuid(
    std::string const& uuid) const noexcept {
  // otherwise we'll look up the collection by name
  RECURSIVE_READ_LOCKER(_dataSourceLock, _dataSourceLockWriteOwner);
  auto itr = _dataSourceByUuid.find(uuid);

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  return itr == _dataSourceByUuid.end()
             ? nullptr
             : std::dynamic_pointer_cast<arangodb::LogicalCollection>(itr->second);
#else
  return itr == _dataSourceByUuid.end() ||
                 itr->second->category() != LogicalCollection::category()
             ? nullptr
             : std::static_pointer_cast<LogicalCollection>(itr->second);
#endif
}

/// @brief looks up a data-source by identifier
std::shared_ptr<arangodb::LogicalDataSource> TRI_vocbase_t::lookupDataSource(DataSourceId id) const noexcept {
  RECURSIVE_READ_LOCKER(_dataSourceLock, _dataSourceLockWriteOwner);
  auto itr = _dataSourceById.find(id);

  return itr == _dataSourceById.end() ? nullptr : itr->second;
}

/// @brief looks up a data-source by name
std::shared_ptr<arangodb::LogicalDataSource> TRI_vocbase_t::lookupDataSource(
    std::string const& nameOrId) const noexcept {
  if (nameOrId.empty()) {
    return nullptr;
  }

  // lookup by id if the data-source name is passed as a stringified id
  bool success = false;
  DataSourceId id{arangodb::NumberUtils::atoi<DataSourceId::BaseType>(
      nameOrId.data(), nameOrId.data() + nameOrId.size(), success)};

  if (success) {
    return lookupDataSource(id);
  }

  RECURSIVE_READ_LOCKER(_dataSourceLock, _dataSourceLockWriteOwner);

  // otherwise look up the data-source by name
  auto itr = _dataSourceByName.find(nameOrId);

  if (itr != _dataSourceByName.end()) {
    return itr->second;
  }

  // otherwise look up the data-source by UUID
  auto itrUuid = _dataSourceByUuid.find(nameOrId);

  return itrUuid == _dataSourceByUuid.end() ? nullptr : itrUuid->second;
}

/// @brief looks up a view by identifier
std::shared_ptr<arangodb::LogicalView> TRI_vocbase_t::lookupView(DataSourceId id) const {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  return std::dynamic_pointer_cast<arangodb::LogicalView>(lookupDataSource(id));
#else
  auto dataSource = lookupDataSource(id);

  return dataSource && dataSource->category() == LogicalView::category()
             ? std::static_pointer_cast<LogicalView>(dataSource)
             : nullptr;
#endif
}

/// @brief looks up a view by name or stringified cid or uuid
std::shared_ptr<arangodb::LogicalView> TRI_vocbase_t::lookupView(std::string const& nameOrId) const {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  return std::dynamic_pointer_cast<arangodb::LogicalView>(lookupDataSource(nameOrId));
#else
  auto dataSource = lookupDataSource(nameOrId);

  return dataSource && dataSource->category() == LogicalView::category()
             ? std::static_pointer_cast<LogicalView>(dataSource)
             : nullptr;
#endif
}

/// @brief creates a new collection from parameter set
/// collection id (cid) is normally passed with a value of 0
/// this means that the system will assign a new collection id automatically
/// using a cid of > 0 is supported to import dumps from other servers etc.
/// but the functionality is not advertised
std::shared_ptr<arangodb::LogicalCollection> TRI_vocbase_t::createCollection(
    arangodb::velocypack::Slice parameters) {
  // check that the name does not contain any strange characters
  std::string const& dbName = _info.getName();

  std::string name;
  bool valid = parameters.isObject();
  if (valid) {
    name = VelocyPackHelper::getStringValue(parameters,
                                            StaticStrings::DataSourceName, "");
    bool isSystem = VelocyPackHelper::getBooleanValue(parameters, StaticStrings::DataSourceSystem, false);
    bool extendedNames = server().getFeature<DatabaseFeature>().extendedNamesForCollections();
    valid = CollectionNameValidator::isAllowedName(isSystem, extendedNames, name);
  }

  if (!valid) {
    events::CreateCollection(dbName, name, TRI_ERROR_ARANGO_ILLEGAL_NAME);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_ILLEGAL_NAME);
  }

  // augment creation parameters
  StorageEngine& engine = server().getFeature<EngineSelectorFeature>().engine();

  VPackBuilder merge;
  merge.openObject();
  engine.addParametersForNewCollection(merge, parameters);
  merge.close();

  merge = velocypack::Collection::merge(parameters, merge.slice(), true, false);
  parameters = merge.slice();

  try {
    READ_LOCKER(readLocker, _inventoryLock);
    auto collection = createCollectionWorker(parameters);
    readLocker.unlock();

    TRI_ASSERT(collection != nullptr);
    events::CreateCollection(dbName, name, TRI_ERROR_NO_ERROR);

    auto& df = server().getFeature<DatabaseFeature>();
    if (df.versionTracker() != nullptr) {
      df.versionTracker()->track("create collection");
    }

    return collection;
  } catch (basics::Exception const& ex) {
    events::CreateCollection(dbName, name, ex.code());
    throw;
  } catch (std::exception const&) {
    events::CreateCollection(dbName, name, TRI_ERROR_INTERNAL);
    throw;
  }
}

/// @brief drops a collection
arangodb::Result TRI_vocbase_t::dropCollection(DataSourceId cid, bool allowDropSystem,
                                               double timeout) {
  auto collection = lookupCollection(cid);
  std::string const& dbName = _info.getName();

  if (!collection) {
    events::DropCollection(dbName, "", TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
    return TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND;
  }

  StorageEngine& engine = server().getFeature<EngineSelectorFeature>().engine();

  if (!allowDropSystem && collection->system() && !engine.inRecovery()) {
    // prevent dropping of system collections
    events::DropCollection(dbName, collection->name(), TRI_ERROR_FORBIDDEN);
    return TRI_set_errno(TRI_ERROR_FORBIDDEN);
  }

  if (ServerState::instance()->isDBServer()) {  // maybe unconditionally ?
    // hack to avoid busy looping on DBServers
    try {
      transaction::cluster::abortTransactions(*collection);
    } catch (...) { /* ignore */
    }
  }

  while (true) {
    DropState state = DROP_EXIT;
    ErrorCode res = TRI_ERROR_NO_ERROR;
    {
      READ_LOCKER(readLocker, _inventoryLock);
      res = dropCollectionWorker(collection.get(), state, timeout);
    }

    if (state == DROP_PERFORM) {
      if (engine.inRecovery()) {
        dropCollectionCallback(*collection);
      } else {
        collection->deferDropCollection(dropCollectionCallback);
      }

      auto& df = server().getFeature<DatabaseFeature>();
      if (df.versionTracker() != nullptr) {
        df.versionTracker()->track("drop collection");
      }
    }

    if (state == DROP_PERFORM || state == DROP_EXIT) {
      events::DropCollection(dbName, collection->name(), res);
      return res;
    }

    if (_server.isStopping()) {
      return TRI_ERROR_SHUTTING_DOWN;
    }

    // try again in next iteration
    TRI_ASSERT(state == DROP_AGAIN);
    std::this_thread::sleep_for(std::chrono::microseconds(collectionStatusPollInterval()));
  }
}

/// @brief renames a view
arangodb::Result TRI_vocbase_t::renameView(DataSourceId cid, std::string const& oldName) {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  auto const view = lookupView(cid);
  std::string const& dbName = _info.getName();

  if (!view) {
    return TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND;
  }

  if (!server().hasFeature<DatabaseFeature>()) {
    return Result(
        TRI_ERROR_INTERNAL,
        std::string("failed to find feature 'Database' while renaming view '") +
            view->name() + "' in database '" + dbName + "'");
  }
  auto& databaseFeature = server().getFeature<DatabaseFeature>();

  if (!server().hasFeature<EngineSelectorFeature>() ||
      !server().getFeature<EngineSelectorFeature>().selected()) {
    return arangodb::Result(
        TRI_ERROR_INTERNAL,
        std::string("failed to find StorageEngine while renaming view '") +
            view->name() + "' in database '" + dbName + "'");
  }
  auto& engine = server().getFeature<EngineSelectorFeature>().engine();

  // lock collection because we are going to copy its current name
  auto newName = view->name();

  // old name should be different

  // check if names are actually different
  if (oldName == newName) {
    return TRI_ERROR_NO_ERROR;
  }

  bool extendedNames = databaseFeature.extendedNamesForViews();
  if (!ViewNameValidator::isAllowedName(/*allowSystem*/ false, extendedNames, newName)) {
    return TRI_set_errno(TRI_ERROR_ARANGO_ILLEGAL_NAME);
  }

  READ_LOCKER(readLocker, _inventoryLock);

  RECURSIVE_WRITE_LOCKER(_dataSourceLock, _dataSourceLockWriteOwner);

  // Check for duplicate name
  auto it = _dataSourceByName.find(newName);

  if (it != _dataSourceByName.end()) {
    // new name already in use
    return TRI_ERROR_ARANGO_DUPLICATE_NAME;
  }

  // get the original pointer and ensure it's a LogicalView
  auto itr1 = _dataSourceByName.find(oldName);

  if (itr1 == _dataSourceByName.end() ||
      LogicalView::category() != itr1->second->category()) {
    return TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND;
  }

  TRI_ASSERT(std::dynamic_pointer_cast<arangodb::LogicalView>(itr1->second));

  auto doSync = databaseFeature.forceSyncProperties();
  auto res = engine.inRecovery()
                 ? arangodb::Result()  // skip persistence while in recovery
                                       // since definition already from engine
                 : engine.changeView(*this, *view, doSync);

  if (!res.ok()) {
    return res;
  }

  // stores the parameters on disk
  auto itr2 = _dataSourceByName.emplace(newName, itr1->second);

  TRI_ASSERT(itr2.second);
  _dataSourceByName.erase(itr1);

  checkCollectionInvariants();

  // invalidate all entries in the plan and query cache now
  arangodb::aql::PlanCache::instance()->invalidate(this);
  arangodb::aql::QueryCache::instance()->invalidate(this);

  return TRI_ERROR_NO_ERROR;
}

/// @brief renames a collection
arangodb::Result TRI_vocbase_t::renameCollection(DataSourceId cid, std::string const& newName) {
  auto collection = lookupCollection(cid);

  if (!collection) {
    return TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND;
  }

  if (collection->system()) {
    return TRI_ERROR_FORBIDDEN;
  }

  // lock collection because we are going to copy its current name
  std::string oldName = collection->name();

  // old name should be different

  // check if names are actually different
  if (oldName == newName) {
    return TRI_ERROR_NO_ERROR;
  }

  READ_LOCKER(readLocker, _inventoryLock);

  RECURSIVE_WRITE_LOCKER_NAMED(writeLocker, _dataSourceLock, _dataSourceLockWriteOwner, false);
  CONDITIONAL_WRITE_LOCKER(locker, collection->statusLock(), false);

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
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  TRI_ASSERT(writeLocker.isLocked());
  TRI_ASSERT(locker.isLocked());

  // Check for duplicate name
  auto itr = _dataSourceByName.find(newName);

  if (itr != _dataSourceByName.end()) {
    return TRI_ERROR_ARANGO_DUPLICATE_NAME;
  }

  // get the original pointer and ensure it's a LogicalCollection
  auto itr1 = _dataSourceByName.find(oldName);

  if (itr1 == _dataSourceByName.end() ||
      LogicalCollection::category() != itr1->second->category()) {
    return TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND;
  }

  TRI_ASSERT(std::dynamic_pointer_cast<arangodb::LogicalCollection>(itr1->second));

  auto res = itr1->second->rename(std::string(newName));

  if (!res.ok()) {
    // rename failed
    return res;
  }

  auto& engine = server().getFeature<EngineSelectorFeature>().engine();

  res = engine.renameCollection(*this, *collection, oldName);  // tell the engine

  if (!res.ok()) {
    // rename failed
    return res;
  }

  // The collection is renamed. Now swap cache entries.
  auto it2 = _dataSourceByName.emplace(newName, itr1->second);
  TRI_ASSERT(it2.second);
  _dataSourceByName.erase(itr1);

  checkCollectionInvariants();
  locker.unlock();
  writeLocker.unlock();

  auto& df = server().getFeature<DatabaseFeature>();
  if (df.versionTracker() != nullptr) {
    df.versionTracker()->track("rename collection");
  }

  // invalidate all entries for the two collections
  arangodb::aql::PlanCache::instance()->invalidate(this);

  return TRI_ERROR_NO_ERROR;
}

/// @brief locks a (document) collection for usage by id
std::shared_ptr<arangodb::LogicalCollection> TRI_vocbase_t::useCollection(DataSourceId cid,
                                                                          bool checkPermissions) {
  return useCollectionInternal(lookupCollection(cid), checkPermissions);
}

/// @brief locks a collection for usage by name
std::shared_ptr<arangodb::LogicalCollection> TRI_vocbase_t::useCollection(
    std::string const& name, bool checkPermissions) {
  // check that we have an existing name
  std::shared_ptr<arangodb::LogicalCollection> collection;
  {
    RECURSIVE_READ_LOCKER(_dataSourceLock, _dataSourceLockWriteOwner);

    auto it = _dataSourceByName.find(name);
    if (it != _dataSourceByName.end() &&
        it->second->category() == LogicalCollection::category()) {
      TRI_ASSERT(std::dynamic_pointer_cast<LogicalCollection>(it->second));
      collection = std::static_pointer_cast<LogicalCollection>(it->second);
    }
  }

  return useCollectionInternal(std::move(collection), checkPermissions);
}

std::shared_ptr<arangodb::LogicalCollection> TRI_vocbase_t::useCollectionInternal(
    std::shared_ptr<arangodb::LogicalCollection> const& coll, bool checkPermissions) {
  if (!coll) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
  }

  // try to load the collection
  arangodb::Result res = loadCollection(*coll, checkPermissions);
  if (res.fail()) {
    THROW_ARANGO_EXCEPTION(res);
  }
  return coll;
}

/// @brief releases a collection from usage
void TRI_vocbase_t::releaseCollection(arangodb::LogicalCollection* collection) noexcept {
  collection->statusLock().unlock();
}

/// @brief creates a new view from parameter set
/// view id is normally passed with a value of 0
/// this means that the system will assign a new id automatically
/// using a cid of > 0 is supported to import dumps from other servers etc.
/// but the functionality is not advertised
std::shared_ptr<arangodb::LogicalView> TRI_vocbase_t::createView(arangodb::velocypack::Slice parameters) {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  auto& engine = server().getFeature<EngineSelectorFeature>().engine();
  std::string const& dbName = _info.getName();
  
  std::string name;
  bool valid = parameters.isObject();
  if (valid) {
    name = VelocyPackHelper::getStringValue(parameters,
                                            StaticStrings::DataSourceName, "");
  
    bool extendedNames = server().getFeature<DatabaseFeature>().extendedNamesForCollections();
    valid &= ViewNameValidator::isAllowedName(/*allowSystem*/ false, extendedNames, name);
  }

  if (!valid) {
    events::CreateView(dbName, name, TRI_ERROR_ARANGO_ILLEGAL_NAME);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_ILLEGAL_NAME);
  }

  arangodb::LogicalView::ptr view;
  auto res = LogicalView::instantiate(view, *this, parameters);

  if (!res.ok() || !view) {
    events::CreateView(dbName, name, res.errorNumber());
    THROW_ARANGO_EXCEPTION_MESSAGE(
        res.errorNumber(),
        res.errorMessage().empty()
            ? std::string("failed to instantiate view from definition: ") +
                  parameters.toString()
            : std::string{res.errorMessage()});
  }

  READ_LOCKER(readLocker, _inventoryLock);
  RECURSIVE_WRITE_LOCKER(_dataSourceLock, _dataSourceLockWriteOwner);
  auto itr = _dataSourceByName.find(view->name());

  if (itr != _dataSourceByName.end()) {
    events::CreateView(dbName, view->name(), TRI_ERROR_ARANGO_DUPLICATE_NAME);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DUPLICATE_NAME);
  }

  registerView(basics::ConditionalLocking::DoNotLock, view);

  try {
    auto res = engine.createView(view->vocbase(), view->id(), *view);

    if (!res.ok()) {
      unregisterView(*view);
      THROW_ARANGO_EXCEPTION(res);
    }
  } catch (...) {
    unregisterView(*view);
    events::CreateView(dbName, view->name(), TRI_ERROR_INTERNAL);
    throw;
  }

  events::CreateView(dbName, view->name(), TRI_ERROR_NO_ERROR);

  auto& df = server().getFeature<DatabaseFeature>();
  if (df.versionTracker() != nullptr) {
    df.versionTracker()->track("create view");
  }

  view->open();  // And lets open it.

  return view;
}

/// @brief drops a view
arangodb::Result TRI_vocbase_t::dropView(DataSourceId cid, bool allowDropSystem) {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  auto const view = lookupView(cid);
  std::string const& dbName = _info.getName();

  if (!view) {
    events::DropView(dbName, "", TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
    return TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND;
  }

  StorageEngine& engine = server().getFeature<EngineSelectorFeature>().engine();

  if (!allowDropSystem && view->system() && !engine.inRecovery()) {
    events::DropView(dbName, view->name(), TRI_ERROR_FORBIDDEN);
    return TRI_ERROR_FORBIDDEN;  // prevent dropping of system views
  }

  READ_LOCKER(readLocker, _inventoryLock);

  // do not acquire these locks instantly
  RECURSIVE_WRITE_LOCKER_NAMED(writeLocker, _dataSourceLock, _dataSourceLockWriteOwner,
                               basics::ConditionalLocking::DoNotLock);
  CONDITIONAL_WRITE_LOCKER(locker, view->_lock, basics::ConditionalLocking::DoNotLock);

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
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  TRI_ASSERT(writeLocker.isLocked());
  TRI_ASSERT(locker.isLocked());

  auto res = engine.dropView(*this, *view);

  if (!res.ok()) {
    events::DropView(dbName, view->name(), res.errorNumber());
    return res;
  }

  // invalidate all entries in the plan and query cache now
#if USE_PLAN_CACHE
  arangodb::aql::PlanCache::instance()->invalidate(this);
#endif
  arangodb::aql::QueryCache::instance()->invalidate(this);

  unregisterView(*view);

  locker.unlock();
  writeLocker.unlock();

  events::DropView(dbName, view->name(), TRI_ERROR_NO_ERROR);

  auto& df = server().getFeature<DatabaseFeature>();
  if (df.versionTracker() != nullptr) {
    df.versionTracker()->track("drop view");
  }

  return TRI_ERROR_NO_ERROR;
}

TRI_vocbase_t::TRI_vocbase_t(TRI_vocbase_type_e type, arangodb::CreateDatabaseInfo&& info)
    : _server(info.server()),
      _info(std::move(info)),
      _type(type),
      _refCount(0),
      _isOwnAppsDirectory(true),
      _deadlockDetector(false) {
  TRI_ASSERT(_info.valid());

  if (_info.server().hasFeature<QueryRegistryFeature>()) {
    QueryRegistryFeature& feature = _info.server().getFeature<QueryRegistryFeature>();
    _queries = std::make_unique<arangodb::aql::QueryList>(feature);
  }
  _cursorRepository = std::make_unique<arangodb::CursorRepository>(*this);
  _replicationClients = std::make_unique<arangodb::ReplicationClientsProgressTracker>();

  // init collections
  _collections.reserve(32);
  _deadCollections.reserve(32);

  _cacheData = std::make_unique<arangodb::DatabaseJavaScriptCache>();
  _logManager = std::make_shared<VocBaseLogManager>(_info.server(), name());
}

/// @brief destroy a vocbase object
TRI_vocbase_t::~TRI_vocbase_t() {
  // do a final cleanup of collections
  for (std::shared_ptr<arangodb::LogicalCollection>& coll : _collections) {
    try {  // simon: this status lock is terrible software design
      transaction::cluster::abortTransactions(*coll);
    } catch (...) { /* ignore */
    }
    WRITE_LOCKER_EVENTUAL(locker, coll->statusLock());
    coll->close();  // required to release indexes
  }

  _collections.clear();  // clear vector before deallocating TRI_vocbase_t members
  _deadCollections.clear();  // clear vector before deallocating TRI_vocbase_t members
  _dataSourceById.clear();  // clear map before deallocating TRI_vocbase_t members
  _dataSourceByName.clear();  // clear map before deallocating TRI_vocbase_t members
  _dataSourceByUuid.clear();  // clear map before deallocating TRI_vocbase_t members
}

std::string TRI_vocbase_t::path() const {
  StorageEngine& engine = server().getFeature<EngineSelectorFeature>().engine();
  return engine.databasePath(this);
}

std::string const& TRI_vocbase_t::sharding() const { return _info.sharding(); }

bool TRI_vocbase_t::isOneShard() const {
  return _info.sharding() == StaticStrings::ShardingSingle;
}

std::uint32_t TRI_vocbase_t::replicationFactor() const {
  return _info.replicationFactor();
}

std::uint32_t TRI_vocbase_t::writeConcern() const {
  return _info.writeConcern();
}

replication::Version TRI_vocbase_t::replicationVersion() const {
  return _info.replicationVersion();
}

void TRI_vocbase_t::addReplicationApplier() {
  TRI_ASSERT(_type != TRI_VOCBASE_TYPE_COORDINATOR);
  auto* applier = DatabaseReplicationApplier::create(*this);
  _replicationApplier.reset(applier);
}

void TRI_vocbase_t::toVelocyPack(VPackBuilder& result) const {
  VPackObjectBuilder b(&result);
  _info.toVelocyPack(result);
  if (ServerState::instance()->isCoordinator()) {
    result.add("path", VPackValue(path()));
  } else {
    result.add("path", VPackValue("none"));
  }
}

/// @brief sets prototype collection for sharding (_users or _graphs)
void TRI_vocbase_t::setShardingPrototype(ShardingPrototype type) {
  _info.shardingPrototype(type);
}

/// @brief gets prototype collection for sharding (_users or _graphs)
ShardingPrototype TRI_vocbase_t::shardingPrototype() const {
  return _info.shardingPrototype();
}

/// @brief gets name of prototype collection for sharding (_users or _graphs)
std::string const& TRI_vocbase_t::shardingPrototypeName() const {
  return _info.shardingPrototype() == ShardingPrototype::Users
             ? StaticStrings::UsersCollection
             : StaticStrings::GraphCollection;
}

std::vector<std::shared_ptr<arangodb::LogicalView>> TRI_vocbase_t::views() {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  std::vector<std::shared_ptr<arangodb::LogicalView>> views;

  {
    RECURSIVE_READ_LOCKER(_dataSourceLock, _dataSourceLockWriteOwner);
    views.reserve(_dataSourceById.size());

    for (auto& entry : _dataSourceById) {
      TRI_ASSERT(entry.second);

      if (entry.second->category() != LogicalView::category()) {
        continue;
      }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
      auto view = std::dynamic_pointer_cast<arangodb::LogicalView>(entry.second);
      TRI_ASSERT(view);
#else
      auto view = std::static_pointer_cast<arangodb::LogicalView>(entry.second);
#endif

      views.emplace_back(view);
    }
  }

  return views;
}

void TRI_vocbase_t::processCollectionsOnShutdown(std::function<void(LogicalCollection*)> const& cb) {
  std::vector<std::shared_ptr<arangodb::LogicalCollection>> collections; 
  
  // make a copy of _collections, so we can call the callback function without the lock
  {
    RECURSIVE_READ_LOCKER(_dataSourceLock, _dataSourceLockWriteOwner);
    collections = _collections;
  }

  for (auto const& it : collections) {
    cb(it.get());
  }
}

void TRI_vocbase_t::processCollections(std::function<void(LogicalCollection*)> const& cb) {
  RECURSIVE_READ_LOCKER(_dataSourceLock, _dataSourceLockWriteOwner);

  for (auto& entry : _dataSourceById) {
    TRI_ASSERT(entry.second);

    if (entry.second->category() != LogicalCollection::category()) {
      continue;
    }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    auto collection =
        std::dynamic_pointer_cast<arangodb::LogicalCollection>(entry.second);
    TRI_ASSERT(collection);
#else
    auto collection =
        std::static_pointer_cast<arangodb::LogicalCollection>(entry.second);
#endif

    cb(collection.get());
  }
}

std::vector<std::shared_ptr<arangodb::LogicalCollection>> TRI_vocbase_t::collections(bool includeDeleted) {
  RECURSIVE_READ_LOCKER(_dataSourceLock, _dataSourceLockWriteOwner);

  if (includeDeleted) {
    return _collections;  // create copy
  }

  std::vector<std::shared_ptr<arangodb::LogicalCollection>> collections;

  collections.reserve(_dataSourceById.size());

  for (auto& entry : _dataSourceById) {
    TRI_ASSERT(entry.second);

    if (entry.second->category() != LogicalCollection::category()) {
      continue;
    }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    auto collection =
        std::dynamic_pointer_cast<arangodb::LogicalCollection>(entry.second);
    TRI_ASSERT(collection);
#else
    auto collection =
        std::static_pointer_cast<arangodb::LogicalCollection>(entry.second);
#endif

    collections.emplace_back(collection);
  }

  return collections;
}

bool TRI_vocbase_t::visitDataSources(dataSourceVisitor const& visitor) {
  TRI_ASSERT(visitor);
  
  std::vector<std::shared_ptr<arangodb::LogicalDataSource>> dataSources;

  RECURSIVE_WRITE_LOCKER(_dataSourceLock, _dataSourceLockWriteOwner);

  dataSources.reserve(_dataSourceById.size());

  // create a copy of all the datasource in case 'visitor' modifies
  // '_dataSourceById'
  for (auto& entry : _dataSourceById) {
    dataSources.emplace_back(entry.second);
  }

  for (auto& dataSource : dataSources) {
    if (dataSource && !visitor(*dataSource)) {
      return false;
    }
  }

  return true;
}

auto TRI_vocbase_t::getReplicatedLogById(arangodb::replication2::LogId id) const
    -> std::shared_ptr<arangodb::replication2::replicated_log::ReplicatedLog> {
  return _logManager->getReplicatedLogById(id);
}

[[nodiscard]] auto TRI_vocbase_t::getReplicatedLogLeaderById(arangodb::replication2::LogId id) const
    -> std::shared_ptr<arangodb::replication2::replicated_log::LogLeader> {
  return getReplicatedLogById(id)->getLeader();
}

[[nodiscard]] auto TRI_vocbase_t::getReplicatedLogFollowerById(arangodb::replication2::LogId id) const
    -> std::shared_ptr<replication2::replicated_log::LogFollower> {
  return getReplicatedLogById(id)->getFollower();
}

[[nodiscard]] auto TRI_vocbase_t::getReplicatedLogs() const
    -> std::unordered_map<arangodb::replication2::LogId, arangodb::replication2::replicated_log::LogStatus> {
  return _logManager->getReplicatedLogs();
}

using namespace arangodb::replication2;

auto TRI_vocbase_t::createReplicatedLog(LogId id, std::optional<std::string> const& collectionName)
    -> arangodb::ResultT<std::shared_ptr<arangodb::replication2::replicated_log::ReplicatedLog>> {
  return _logManager->createReplicatedLog(*this, id, collectionName);
}

auto TRI_vocbase_t::dropReplicatedLog(arangodb::replication2::LogId id) -> arangodb::Result {
  return _logManager->dropReplicatedLog(*this, id);
}

/// @brief sanitize an object, given as slice, builder must contain an
/// open object which will remain open
/// the result is the object excluding _id, _key and _rev
void TRI_SanitizeObject(VPackSlice const slice, VPackBuilder& builder) {
  TRI_ASSERT(slice.isObject());
  VPackObjectIterator it(slice);
  while (it.valid()) {
    std::string_view key(it.key().stringView());
    // _id, _key, _rev. minimum size here is 3
    if (key.size() < 3 || key[0] != '_' ||
        (key != StaticStrings::KeyString && key != StaticStrings::IdString &&
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
  VPackObjectIterator it(slice, true);
  while (it.valid()) {
    std::string_view key(it.key().stringView());
    // _id, _key, _rev, _from, _to. minimum size here is 3
    if (key.size() < 3 || key[0] != '_' ||
        (key != StaticStrings::KeyString && key != StaticStrings::IdString &&
         key != StaticStrings::RevString && key != StaticStrings::FromString &&
         key != StaticStrings::ToString)) {
      builder.add(key.data(), key.length(), it.value());
    }
    it.next();
  }
}

void TRI_vocbase_t::registerReplicatedLog(
    arangodb::replication2::LogId logId,
    std::shared_ptr<arangodb::replication2::replicated_log::PersistedLog> persistedLog) {
  _logManager->_guardedData.doUnderLock([&](VocBaseLogManager::GuardedData& data) {
    auto core = std::make_unique<arangodb::replication2::replicated_log::LogCore>(
        std::move(persistedLog));
    auto& feature = server().getFeature<ReplicatedLogFeature>();
    auto [iter, inserted] =
        data.logs.try_emplace(logId,
                              std::make_shared<replication2::replicated_log::ReplicatedLog>(
                                  std::move(core), feature.metrics(), feature.options(),
                                  LoggerContext(Logger::REPLICATION2)));
    server().getFeature<ReplicatedLogFeature>().metrics()->replicatedLogNumber->fetch_add(1);
    TRI_ASSERT(inserted);
  });
}

void TRI_vocbase_t::unregisterReplicatedLog(arangodb::replication2::LogId id) {
  _logManager->_guardedData.doUnderLock([&](VocBaseLogManager::GuardedData& data) {
    if (auto iter = data.logs.find(id); iter != data.logs.end()) {
      data.logs.erase(iter);
      server().getFeature<ReplicatedLogFeature>().metrics()->replicatedLogNumber->fetch_sub(1);
    }
  });
}

auto TRI_vocbase_t::ensureReplicatedLog(arangodb::replication2::LogId id,
                                        std::optional<std::string> const& collectionName)
    -> std::shared_ptr<arangodb::replication2::replicated_log::ReplicatedLog> {
  auto res = _logManager->ensureReplicatedLog(*this, id, collectionName);
  if (res.fail()) {
    THROW_ARANGO_EXCEPTION(res.result());
  }
  return res.get();
}

std::shared_ptr<arangodb::replication2::replicated_log::ILogParticipant> TRI_vocbase_t::lookupLog(
    arangodb::replication2::LogId id) const noexcept {
  try {
    if (auto log = getReplicatedLogById(id)) {
      return log->getParticipant();
    }
  } catch (...) {
  }
  return nullptr;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
