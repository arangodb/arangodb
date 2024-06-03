////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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
#include <exception>
#include <memory>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include <velocypack/Collection.h>
#include <velocypack/Slice.h>
#include <velocypack/Value.h>
#include <velocypack/ValueType.h>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/QueryCache.h"
#include "Aql/QueryList.h"
#include "Auth/Common.h"
#include "Basics/Exceptions.h"
#include "Basics/Exceptions.tpp"
#include "Basics/Locking.h"
#include "Basics/DownCast.h"
#include "Basics/ReadLocker.h"
#include "Basics/RecursiveLocker.h"
#include "Basics/Result.h"
#include "Basics/ScopeGuard.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Basics/debugging.h"
#include "Basics/voc-errors.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "Containers/Helpers.h"
#include "Logger/LogMacros.h"
#include "Metrics/Counter.h"
#include "Metrics/Gauge.h"
#include "Metrics/MetricsFeature.h"
#include "Network/ConnectionPool.h"
#include "Network/NetworkFeature.h"
#include "Replication/DatabaseReplicationApplier.h"
#include "Replication/ReplicationClients.h"
#include "Replication/ReplicationFeature.h"
#include "Replication2/ReplicatedLog/ILogInterfaces.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/ReplicatedLog/LogLeader.h"
#include "Replication2/ReplicatedLog/LogStatus.h"
#include "Replication2/ReplicatedLog/ReplicatedLog.h"
#include "Replication2/ReplicatedLog/ReplicatedLogFeature.h"
#include "Replication2/ReplicatedState/ReplicatedState.h"
#include "Replication2/ReplicatedState/ReplicatedStateFeature.h"
#include "Replication2/Storage/IStorageEngineMethods.h"
#include "Replication2/Version.h"
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
#ifdef USE_V8
#include "V8Server/v8-user-structures.h"
#endif
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalDataSource.h"
#include "VocBase/LogicalView.h"
#include "VocBase/Properties/CreateCollectionBody.h"
#include "VocBase/Properties/DatabaseConfiguration.h"
#include "VocBase/Properties/UserInputCollectionProperties.h"
#include "VocBase/VocBaseLogManager.h"
#include "VocBase/VocbaseMetrics.h"

#include <thread>
#include <absl/strings/str_cat.h>

using namespace arangodb;
using namespace arangodb::basics;

bool TRI_vocbase_t::use() noexcept {
  auto v = _refCount.load(std::memory_order_relaxed);
  do {
    if ((v & 1) != 0) {
      return false;
    }
    // increase the reference counter by 2.
    // this is because we use odd values to indicate that the database has been
    // marked as deleted
  } while (!_refCount.compare_exchange_weak(v, v + 2, std::memory_order_acquire,
                                            std::memory_order_relaxed));
  return true;
}

void TRI_vocbase_t::forceUse() noexcept {
  _refCount.fetch_add(2, std::memory_order_relaxed);
}

void TRI_vocbase_t::release() noexcept {
  [[maybe_unused]] auto v = _refCount.fetch_sub(2, std::memory_order_release);
  TRI_ASSERT(v >= 2);
}

bool TRI_vocbase_t::isDangling() const noexcept {
  auto const v = _refCount.load(std::memory_order_acquire);
  TRI_ASSERT((v & 1) == 0 || !isSystem());
  return v == 1;
}

bool TRI_vocbase_t::isDropped() const noexcept {
  auto const v = _refCount.load(std::memory_order_acquire);
  TRI_ASSERT((v & 1) == 0 || !isSystem());
  return (v & 1) != 0;
}

bool TRI_vocbase_t::markAsDropped() noexcept {
  TRI_ASSERT(!isSystem());
  auto const v = _refCount.fetch_or(1, std::memory_order_acq_rel);
  return (v & 1) == 0;
}

bool TRI_vocbase_t::isSystem() const noexcept {
  return _info.getName() == StaticStrings::SystemDatabase;
}

void TRI_vocbase_t::checkCollectionInvariants() const noexcept {
  TRI_ASSERT(_dataSourceByName.size() == _dataSourceById.size());
  TRI_ASSERT(_dataSourceByUuid.size() == _dataSourceById.size());
}

void TRI_vocbase_t::registerCollection(
    bool doLock, std::shared_ptr<LogicalCollection> const& collection) {
  auto const& name = collection->name();
  auto const id = collection->id();
  auto const& guid = collection->guid();
  {
    RECURSIVE_WRITE_LOCKER_NAMED(writeLocker, _dataSourceLock,
                                 _dataSourceLockWriteOwner, doLock);
    checkCollectionInvariants();
    ScopeGuard guard0{[&]() noexcept { checkCollectionInvariants(); }};

    if (auto it = _dataSourceByName.try_emplace(name, collection); !it.second) {
      auto const& ptr = it.first->second;
      TRI_ASSERT(ptr);
      auto msg =
          absl::StrCat("collection name '", name, "' already exist with id '",
                       ptr->id().id(), "', guid '", ptr->guid(), "'");
      LOG_TOPIC("405f9", ERR, Logger::FIXME) << msg;
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DUPLICATE_NAME,
                                     std::move(msg));
    }
    ScopeGuard guard1{[&]() noexcept { _dataSourceByName.erase(name); }};

    if (auto it = _dataSourceById.try_emplace(id, collection); !it.second) {
      auto const& ptr = it.first->second;
      TRI_ASSERT(ptr);
      auto msg = absl::StrCat("collection id '", id.id(),
                              "' already exist with name '", ptr->name(),
                              "', guid '", ptr->guid(), "'");
      LOG_TOPIC("0ef12", ERR, Logger::FIXME) << msg;
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DUPLICATE_IDENTIFIER,
                                     std::move(msg));
    }
    ScopeGuard guard2{[&]() noexcept { _dataSourceById.erase(id); }};

    if (auto it = _dataSourceByUuid.try_emplace(guid, collection); !it.second) {
      auto const& ptr = it.first->second;
      TRI_ASSERT(ptr);
      auto msg =
          absl::StrCat("collection guid '", guid, "' already exist with name '",
                       ptr->name(), "', id '", ptr->id().id(), "'");
      LOG_TOPIC("d4958", ERR, Logger::FIXME) << msg;
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DUPLICATE_IDENTIFIER,
                                     std::move(msg));
    }
    ScopeGuard guard3{[&]() noexcept { _dataSourceByUuid.erase(guid); }};

    _collections.emplace_back(collection);
    guard1.cancel();
    guard2.cancel();
    guard3.cancel();
  }
}

void TRI_vocbase_t::unregisterCollection(LogicalCollection& collection) {
  // pre-condition
  checkCollectionInvariants();

  auto itr = _dataSourceById.find(collection.id());

  if (itr == _dataSourceById.end() ||
      itr->second->category() != LogicalDataSource::Category::kCollection) {
    return;  // no such collection
  }

  TRI_ASSERT(std::dynamic_pointer_cast<LogicalCollection>(itr->second));

  // only if we find the collection by its id, we can delete it by name
  _dataSourceById.erase(itr);

  // this is because someone else might have created a new collection with the
  // same name, but with a different id
  _dataSourceByName.erase(collection.name());
  _dataSourceByUuid.erase(collection.guid());

  // post-condition
  checkCollectionInvariants();
}

void TRI_vocbase_t::registerView(bool doLock,
                                 std::shared_ptr<LogicalView> const& view) {
  TRI_ASSERT(view);
  auto const& name = view->name();
  auto id = view->id();
  auto const& guid = view->guid();
  {
    RECURSIVE_WRITE_LOCKER_NAMED(writeLocker, _dataSourceLock,
                                 _dataSourceLockWriteOwner, doLock);
    checkCollectionInvariants();
    ScopeGuard guard0{[&]() noexcept { checkCollectionInvariants(); }};

    if (auto it = _dataSourceByName.try_emplace(name, view); !it.second) {
      auto const& ptr = it.first->second;
      TRI_ASSERT(ptr);
      auto msg = absl::StrCat("view name '", name, "' already exist with id '",
                              ptr->id().id(), "', guid '", ptr->guid(), "'");
      LOG_TOPIC("560a6", ERR, Logger::FIXME) << msg;
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DUPLICATE_NAME,
                                     std::move(msg));
    }
    ScopeGuard guard1{[&]() noexcept { _dataSourceByName.erase(name); }};

    if (auto it = _dataSourceById.try_emplace(id, view); !it.second) {
      auto const& ptr = it.first->second;
      TRI_ASSERT(ptr);
      auto msg =
          absl::StrCat("view id '", id.id(), "' already exist with name '",
                       ptr->name(), "', guid '", ptr->guid(), "'");
      LOG_TOPIC("cb53a", ERR, Logger::FIXME) << msg;
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DUPLICATE_IDENTIFIER,
                                     std::move(msg));
    }
    ScopeGuard guard2{[&]() noexcept { _dataSourceById.erase(id); }};

    if (auto it = _dataSourceByUuid.try_emplace(guid, view); !it.second) {
      auto const& ptr = it.first->second;
      TRI_ASSERT(ptr);
      auto msg =
          absl::StrCat("view guid '", guid, "' already exist with name '",
                       ptr->name(), "', id '", ptr->id().id(), "'");
      LOG_TOPIC("a30ae", ERR, Logger::FIXME) << msg;
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DUPLICATE_IDENTIFIER,
                                     std::move(msg));
    }
    guard1.cancel();
    guard2.cancel();
  }
}

bool TRI_vocbase_t::unregisterView(LogicalView const& view) {
  // pre-condition
  checkCollectionInvariants();

  auto itr = _dataSourceById.find(view.id());

  if (itr == _dataSourceById.end() ||
      itr->second->category() != LogicalDataSource::Category::kView) {
    return true;  // no such view
  }

  TRI_ASSERT(std::dynamic_pointer_cast<LogicalView>(itr->second));

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

bool TRI_vocbase_t::dropCollectionCallback(LogicalCollection& collection) {
  // remove from list of collections
  auto& vocbase = collection.vocbase();

  {
    RECURSIVE_WRITE_LOCKER(vocbase._dataSourceLock,
                           vocbase._dataSourceLockWriteOwner);
    auto it = vocbase._collections.begin();

    for (auto end = vocbase._collections.end(); it != end; ++it) {
      if (it->get() == &collection) {
        break;
      }
    }

    if (it != vocbase._collections.end()) {
      auto col = *it;

      vocbase._collections.erase(it);

      // we need to clean up the pointers later so we insert it into this
      // vector
      try {
        vocbase._deadCollections.emplace_back(col);
      } catch (...) {
      }
    }
  }

  collection.drop();

  return true;
}

#ifndef USE_ENTERPRISE
std::shared_ptr<LogicalCollection> TRI_vocbase_t::createCollectionObject(
    velocypack::Slice data, bool isAStub) {
  // every collection object on coordinators must be a stub
  TRI_ASSERT(!ServerState::instance()->isCoordinator() || isAStub);
  // collection objects on single servers must not be stubs
  TRI_ASSERT(!ServerState::instance()->isSingleServer() || !isAStub);

  return std::make_shared<LogicalCollection>(*this, data, isAStub);
}
#endif

void TRI_vocbase_t::persistCollection(
    std::shared_ptr<LogicalCollection> const& collection) {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());

  RECURSIVE_WRITE_LOCKER(_dataSourceLock, _dataSourceLockWriteOwner);

  // reserve room for the new collection
  containers::Helpers::reserveSpace(_collections, 8);
  containers::Helpers::reserveSpace(_deadCollections, 8);

  auto it = _dataSourceByName.find(collection->name());

  if (it != _dataSourceByName.end()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DUPLICATE_NAME);
  }

  registerCollection(basics::ConditionalLocking::DoNotLock, collection);

  try {
    // set collection version to latest version, as the collection is just
    // created
    collection->setVersion(LogicalCollection::currentVersion());

    // Let's try to persist it.
    collection->persistPhysicalCollection();
  } catch (...) {
    unregisterCollection(*collection);
    throw;
  }
}

Result TRI_vocbase_t::loadCollection(LogicalCollection& collection,
                                     bool checkPermissions) {
  TRI_ASSERT(collection.id().isSet());

  if (checkPermissions) {
    std::string const& dbName = _info.getName();
    if (!ExecContext::current().canUseCollection(dbName, collection.name(),
                                                 auth::Level::RO)) {
      return {TRI_ERROR_FORBIDDEN, std::string("cannot access collection '") +
                                       collection.name() + "'"};
    }
  }

  // read lock
  READ_LOCKER_EVENTUAL(locker, collection.statusLock());

  if (collection.deleted()) {
    return {TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
            std::string("collection '") + collection.name() + "' not found"};
  }

  // DO NOT release the lock
  locker.steal();
  return {};
}

Result TRI_vocbase_t::dropCollectionWorker(LogicalCollection& collection) {
  std::string const colName(collection.name());
  std::string const& dbName = _info.getName();

  // intentionally set the deleted flag of the collection already
  // here, without holding the lock. this is thread-safe, because
  // setDeleted() only modifies an atomic boolean value.
  // we want to switch the flag here already so that any other
  // actions can observe the deletion and abort prematurely.
  // these other actions may have acquired the collection's status
  // lock in read mode, and if we ourselves try to acquire the
  // collection's status lock in write mode, we would just block
  // here.
  collection.setDeleted();

  Result res;

  auto guard = scopeGuard([&res, &colName, &dbName]() noexcept {
    try {
      events::DropCollection(dbName, colName, res.errorNumber());
    } catch (...) {
    }
  });

  _engine.prepareDropCollection(*this, collection);

  // do not acquire these locks instantly
  RECURSIVE_WRITE_LOCKER_NAMED(writeLocker, _dataSourceLock,
                               _dataSourceLockWriteOwner,
                               basics::ConditionalLocking::DoNotLock);
  CONDITIONAL_WRITE_LOCKER(locker, collection.statusLock(),
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

    if (_server.isStopping()) {
      return res.reset(TRI_ERROR_SHUTTING_DOWN);
    }

    _engine.prepareDropCollection(*this, collection);

    // sleep for a while
    std::this_thread::yield();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  TRI_ASSERT(writeLocker.isLocked());
  TRI_ASSERT(locker.isLocked());

  aql::QueryCache::instance()->invalidate(this);

  collection.setDeleted();

  VPackBuilder builder;
  _engine.getCollectionInfo(*this, collection.id(), builder, false, 0);

  res = collection.properties(builder.slice().get("parameters"));

  if (res.ok()) {
    unregisterCollection(collection);

    locker.unlock();
    writeLocker.unlock();

    _engine.dropCollection(*this, collection);
  }
  return res;
}

void TRI_vocbase_t::stop() {
  try {
    shutdownReplicatedLogs();

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

void TRI_vocbase_t::shutdown() {
  stop();

  std::vector<std::shared_ptr<LogicalCollection>> collections;

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
    _dataSourceByName = {};
    _dataSourceById = {};
    _dataSourceByUuid = {};
    checkCollectionInvariants();
  }

  _deadCollections = {};
  _collections = {};
}

std::vector<std::string> TRI_vocbase_t::collectionNames() const {
  std::vector<std::string> result;

  RECURSIVE_READ_LOCKER(_dataSourceLock, _dataSourceLockWriteOwner);

  result.reserve(_dataSourceByName.size());

  for (auto& entry : _dataSourceByName) {
    TRI_ASSERT(entry.second);

    if (entry.second->category() != LogicalDataSource::Category::kCollection) {
      continue;
    }

    result.emplace_back(entry.first);
  }

  return result;
}

void TRI_vocbase_t::inventory(
    VPackBuilder& result, TRI_voc_tick_t maxTick,
    std::function<bool(LogicalCollection const*)> const& nameFilter) {
  TRI_ASSERT(result.isOpenObject());

  decltype(_collections) collections;

  // cycle on write-lock
  WRITE_LOCKER_EVENTUAL(writeLock, _inventoryLock);

  // copy collection pointers into vector so we can work with the copy without
  // the global lock
  {
    RECURSIVE_READ_LOCKER(_dataSourceLock, _dataSourceLockWriteOwner);
    collections = _collections;
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

  result.add(VPackValue(StaticStrings::Properties));
  result.openObject();
  _info.toVelocyPack(result);
  result.close();

  result.add("collections", VPackValue(VPackValueType::Array));
  std::string const& dbName = _info.getName();
  for (auto& collection : collections) {
    READ_LOCKER(readLocker, collection->statusLock());

    if (collection->deleted()) {
      // we do not need to care about deleted collections
      continue;
    }

    // In cluster case cids are not created by ticks but by cluster uniqIds
    if (!ServerState::instance()->isRunningInCluster() &&
        collection->id().id() > maxTick) {
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
  LogicalView::enumerate(
      *this, [&result](LogicalView::ptr const& view) -> bool {
        if (view) {
          result.openObject();
          view->properties(result, LogicalDataSource::Serialization::Inventory);
          result.close();
        }

        return true;
      });
  result.close();  // </views>
}

std::shared_ptr<LogicalCollection> TRI_vocbase_t::lookupCollection(
    DataSourceId id) const noexcept {
  auto ptr = lookupDataSource(id);
  if (!ptr || ptr->category() != LogicalDataSource::Category::kCollection) {
    return nullptr;
  }
  return basics::downCast<LogicalCollection>(std::move(ptr));
}

std::shared_ptr<LogicalCollection> TRI_vocbase_t::lookupCollection(
    std::string_view nameOrId) const noexcept {
  auto ptr = lookupDataSource(nameOrId);
  if (!ptr || ptr->category() != LogicalDataSource::Category::kCollection) {
    return nullptr;
  }
  return basics::downCast<LogicalCollection>(std::move(ptr));
}

std::shared_ptr<LogicalCollection> TRI_vocbase_t::lookupCollectionByUuid(
    std::string_view uuid) const noexcept {
  // otherwise we'll look up the collection by name
  RECURSIVE_READ_LOCKER(_dataSourceLock, _dataSourceLockWriteOwner);
  auto it = _dataSourceByUuid.find(uuid);
  if (it == _dataSourceByUuid.end()) {
    return nullptr;
  }
  TRI_ASSERT(it->second);
  if (it->second->category() != LogicalDataSource::Category::kCollection) {
    return nullptr;
  }
  return basics::downCast<LogicalCollection>(it->second);
}

std::shared_ptr<LogicalDataSource> TRI_vocbase_t::lookupDataSource(
    DataSourceId id) const noexcept {
  RECURSIVE_READ_LOCKER(_dataSourceLock, _dataSourceLockWriteOwner);
  auto itr = _dataSourceById.find(id);

  return itr == _dataSourceById.end() ? nullptr : itr->second;
}

std::shared_ptr<LogicalDataSource> TRI_vocbase_t::lookupDataSource(
    std::string_view nameOrId) const noexcept {
  if (nameOrId.empty()) {
    return nullptr;
  }

  // lookup by id if the data-source name is passed as a stringified id
  DataSourceId::BaseType id;
  if (absl::SimpleAtoi(nameOrId, &id)) {
    return lookupDataSource(DataSourceId{id});
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

std::shared_ptr<LogicalView> TRI_vocbase_t::lookupView(DataSourceId id) const {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  auto ptr = lookupDataSource(id);
  if (!ptr || ptr->category() != LogicalDataSource::Category::kView) {
    return nullptr;
  }
  return basics::downCast<LogicalView>(std::move(ptr));
}

std::shared_ptr<LogicalView> TRI_vocbase_t::lookupView(
    std::string_view nameOrId) const {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  auto ptr = lookupDataSource(nameOrId);
  if (!ptr || ptr->category() != LogicalDataSource::Category::kView) {
    return nullptr;
  }
  return basics::downCast<LogicalView>(std::move(ptr));
}

std::shared_ptr<LogicalCollection>
TRI_vocbase_t::createCollectionObjectForStorage(velocypack::Slice parameters) {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());

  // augment collection parameters with storage-engine specific data
  VPackBuilder merged;
  merged.openObject();
  _engine.addParametersForNewCollection(merged, parameters);
  merged.close();

  merged =
      velocypack::Collection::merge(parameters, merged.slice(), true, false);
  parameters = merged.slice();

  // Try to create a new collection. This is not registered yet
  // This is always a new and empty collection.
  return createCollectionObject(parameters, /*isAStub*/ false);
}

std::shared_ptr<LogicalCollection> TRI_vocbase_t::createCollection(
    velocypack::Slice parameters) {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());

  auto const& dbName = _info.getName();
  std::string name = VelocyPackHelper::getStringValue(
      parameters, StaticStrings::DataSourceName, "");

  // validate collection parameters
  Result res = validateCollectionParameters(parameters);
  if (res.fail()) {
    events::CreateCollection(dbName, name, res.errorNumber());
    THROW_ARANGO_EXCEPTION(res);
  }

  try {
    // Try to create a new collection. This is not registered yet
    auto collection = createCollectionObjectForStorage(parameters);

    {
      READ_LOCKER(readLocker, _inventoryLock);
      persistCollection(collection);
    }

    events::CreateCollection(dbName, name, TRI_ERROR_NO_ERROR);

    _versionTracker.track("create collection");

    return collection;
  } catch (basics::Exception const& ex) {
    events::CreateCollection(dbName, name, ex.code());
    throw;
  } catch (std::exception const&) {
    events::CreateCollection(dbName, name, TRI_ERROR_INTERNAL);
    throw;
  }
}

ResultT<std::vector<std::shared_ptr<arangodb::LogicalCollection>>>
TRI_vocbase_t::createCollections(
    std::vector<arangodb::CreateCollectionBody> const& collections,
    bool allowEnterpriseCollectionsOnSingleServer) {
  /// Code from here is copy pasted from original create and
  /// has not been refacored yet.
  VPackBuilder builder =
      CreateCollectionBody::toCreateCollectionProperties(collections);
  VPackSlice infoSlice = builder.slice();

  TRI_ASSERT(infoSlice.isArray());
  TRI_ASSERT(infoSlice.length() >= 1);
  TRI_ASSERT(infoSlice.length() == collections.size());
  try {
    // Here we do have a single server setup, or we're either on a DBServer
    // / Agency. In that case, we're not batching collection creating.
    // Therefore, we need to iterate over the infoSlice and create each
    // collection one by one.
    return {
        createCollections(infoSlice, allowEnterpriseCollectionsOnSingleServer)};

  } catch (basics::Exception const& ex) {
    return Result(ex.code(), ex.what());
  } catch (std::exception const& ex) {
    return Result(TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    return Result(TRI_ERROR_INTERNAL, "cannot create collection");
  }
}

std::vector<std::shared_ptr<LogicalCollection>>
TRI_vocbase_t::createCollections(
    velocypack::Slice infoSlice,
    bool allowEnterpriseCollectionsOnSingleServer) {
  TRI_ASSERT(!allowEnterpriseCollectionsOnSingleServer ||
             ServerState::instance()->isSingleServer());

#ifndef USE_ENTERPRISE
  if (allowEnterpriseCollectionsOnSingleServer) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_NOT_IMPLEMENTED,
        "creating SmartGraph collections is not supported in this version");
  }
#endif

  auto const& dbName = _info.getName();

  // first validate all collections
  for (auto slice : VPackArrayIterator(infoSlice)) {
    Result res = validateCollectionParameters(slice);
    if (res.fail()) {
      std::string name = VelocyPackHelper::getStringValue(
          slice, StaticStrings::DataSourceName, "");
      events::CreateCollection(dbName, name, res.errorNumber());
      THROW_ARANGO_EXCEPTION(res);
    }
  }

  std::vector<std::shared_ptr<LogicalCollection>> collections;
  collections.reserve(infoSlice.length());

  // now create all collection objects
  for (auto slice : VPackArrayIterator(infoSlice)) {
    // collection object to be created
    std::shared_ptr<LogicalCollection> col;

    if (ServerState::instance()->isCoordinator()) {
      // create a non-augmented collection object. on coordinators,
      // we do not persist any data, so we can get away with a lightweight
      // object (isAStub = true).
      // This is always a new and empty collection.
      col = createCollectionObject(slice, /*isAStub*/ true);
    } else {
      // if we are not on a coordinator, we want to store the collection,
      // so we augment the collection data with some storage-engine
      // specific values
      col = createCollectionObjectForStorage(slice);
    }

    TRI_ASSERT(col != nullptr);
    collections.emplace_back(col);

    // add SmartGraph sub-collections to collections if col is a
    // SmartGraph edge collection that requires it.
    addSmartGraphCollections(col, collections);
  }

  if (!ServerState::instance()->isCoordinator()) {
    // if we are not on a coordinator, we want to store the collection
    // objects for later lookups by name, guid etc. on a coordinator, this
    // is not necessary here, because the collections are first created via
    // the agency and stored there. they will later find their way to the
    // coordinator again via the AgencyCache and ClusterInfo, which will
    // create and register them using a separate codepath.
    READ_LOCKER(readLocker, _inventoryLock);
    for (auto& col : collections) {
      persistCollection(col);
    }
  }

  // audit-log all collections
  for (auto& col : collections) {
    events::CreateCollection(dbName, col->name(), TRI_ERROR_NO_ERROR);
  }

  _versionTracker.track("create collection");
  return collections;
}

Result TRI_vocbase_t::dropCollection(DataSourceId cid, bool allowDropSystem) {
  auto collection = lookupCollection(cid);
  auto const& dbName = _info.getName();

  if (!collection) {
    events::DropCollection(dbName, "", TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
    return TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND;
  }

  if (!allowDropSystem && collection->system() && !_engine.inRecovery()) {
    // prevent dropping of system collections
    events::DropCollection(dbName, collection->name(), TRI_ERROR_FORBIDDEN);
    return TRI_ERROR_FORBIDDEN;
  }

  if (ServerState::instance()->isDBServer()) {  // maybe unconditionally ?
    // hack to avoid busy looping on DBServers
    try {
      transaction::cluster::abortTransactions(*collection);
    } catch (...) { /* ignore */
    }
  }

  Result res;
  {
    READ_LOCKER(readLocker, _inventoryLock);
    res = dropCollectionWorker(*collection);
  }

  if (res.ok()) {
    collection->deferDropCollection(dropCollectionCallback);
    _versionTracker.track("drop collection");
  }

  return res;
}

Result TRI_vocbase_t::validateCollectionParameters(
    velocypack::Slice parameters) {
  if (!parameters.isObject()) {
    return {TRI_ERROR_BAD_PARAMETER,
            "collection parameters should be an object"};
  }
  // check that the name does not contain any strange characters
  std::string name = VelocyPackHelper::getStringValue(
      parameters, StaticStrings::DataSourceName, "");
  bool isSystem = VelocyPackHelper::getBooleanValue(
      parameters, StaticStrings::DataSourceSystem, false);
  if (auto res =
          CollectionNameValidator::validateName(isSystem, _extendedNames, name);
      res.fail()) {
    return res;
  }

  TRI_col_type_e collectionType =
      VelocyPackHelper::getNumericValue<TRI_col_type_e, int>(
          parameters, StaticStrings::DataSourceType, TRI_COL_TYPE_DOCUMENT);

  if (collectionType != TRI_col_type_e::TRI_COL_TYPE_DOCUMENT &&
      collectionType != TRI_col_type_e::TRI_COL_TYPE_EDGE) {
    return {TRI_ERROR_ARANGO_COLLECTION_TYPE_INVALID,
            "invalid collection type for collection '" + name + "'"};
  }

  // needed for EE
  return validateExtendedCollectionParameters(parameters);
}

#ifndef USE_ENTERPRISE
void TRI_vocbase_t::addSmartGraphCollections(
    std::shared_ptr<LogicalCollection> const& /*collection*/,
    std::vector<std::shared_ptr<LogicalCollection>>& /*collections*/) const {
  // nothing to be done here. more in EE version
}

Result TRI_vocbase_t::validateExtendedCollectionParameters(velocypack::Slice) {
  // nothing to be done here. more in EE version
  return {};
}
#endif

Result TRI_vocbase_t::renameView(DataSourceId cid, std::string_view oldName) {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  auto const view = lookupView(cid);
  if (!view) {
    return TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND;
  }

  // lock collection because we are going to copy its current name
  auto newName = view->name();

  // old name should be different

  // check if names are actually different
  if (oldName == newName) {
    return TRI_ERROR_NO_ERROR;
  }

  if (auto res = ViewNameValidator::validateName(/*allowSystem*/ false,
                                                 _extendedNames, newName);
      res.fail()) {
    return res;
  }

  READ_LOCKER(readLocker, _inventoryLock);

  RECURSIVE_WRITE_LOCKER(_dataSourceLock, _dataSourceLockWriteOwner);

  // Check for duplicate name
  if (_dataSourceByName.contains(newName)) {
    // new name already in use
    return TRI_ERROR_ARANGO_DUPLICATE_NAME;
  }

  // get the original pointer and ensure it's a LogicalView
  auto it1 = _dataSourceByName.find(oldName);
  if (it1 == _dataSourceByName.end() ||
      LogicalDataSource::Category::kView != it1->second->category()) {
    return TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND;
  }
  // Important to save it here, before emplace in map
  auto dataSource = it1->second;
  TRI_ASSERT(std::dynamic_pointer_cast<LogicalView>(dataSource));
  // skip persistence while in recovery since definition already from engine
  if (!_engine.inRecovery()) {
    velocypack::Builder build;
    build.openObject();
    auto r = view->properties(
        build, LogicalDataSource::Serialization::PersistenceWithInProgress);
    if (!r.ok()) {
      return r;
    }
    r = _engine.changeView(*view, build.close().slice());
    if (!r.ok()) {
      return r;
    }
  }

  // stores the parameters on disk
  auto it2 = _dataSourceByName.emplace(newName, std::move(dataSource));
  TRI_ASSERT(it2.second);
  _dataSourceByName.erase(oldName);

  checkCollectionInvariants();

  // invalidate all entries in the query cache now
  aql::QueryCache::instance()->invalidate(this);

  return TRI_ERROR_NO_ERROR;
}

Result TRI_vocbase_t::renameCollection(DataSourceId cid,
                                       std::string_view newName) {
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

  if (auto res = CollectionNameValidator::validateName(/*allowSystem*/ false,
                                                       _extendedNames, newName);
      res.fail()) {
    return res;
  }

  READ_LOCKER(readLocker, _inventoryLock);

  RECURSIVE_WRITE_LOCKER_NAMED(writeLocker, _dataSourceLock,
                               _dataSourceLockWriteOwner, false);
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
  if (_dataSourceByName.contains(newName)) {
    return TRI_ERROR_ARANGO_DUPLICATE_NAME;
  }

  // get the original pointer and ensure it's a LogicalCollection
  auto it1 = _dataSourceByName.find(oldName);
  if (it1 == _dataSourceByName.end() ||
      LogicalDataSource::Category::kCollection != it1->second->category()) {
    return TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND;
  }
  // Important to save it here, before emplace in map
  auto dataSource = it1->second;
  TRI_ASSERT(std::dynamic_pointer_cast<LogicalCollection>(dataSource));

  auto res = it1->second->rename(std::string(newName));

  if (!res.ok()) {
    // rename failed
    return res;
  }

  res =
      _engine.renameCollection(*this, *collection, oldName);  // tell the engine

  if (!res.ok()) {
    // rename failed
    return res;
  }

  // The collection is renamed. Now swap cache entries.
  auto it2 = _dataSourceByName.emplace(newName, std::move(dataSource));
  TRI_ASSERT(it2.second);
  _dataSourceByName.erase(oldName);

  checkCollectionInvariants();
  locker.unlock();
  writeLocker.unlock();
  _versionTracker.track("rename collection");

  return TRI_ERROR_NO_ERROR;
}

std::shared_ptr<LogicalCollection> TRI_vocbase_t::useCollection(
    DataSourceId cid, bool checkPermissions) {
  return useCollectionInternal(lookupCollection(cid), checkPermissions);
}

std::shared_ptr<LogicalCollection> TRI_vocbase_t::useCollection(
    std::string_view name, bool checkPermissions) {
  // check that we have an existing name
  std::shared_ptr<LogicalCollection> collection;
  {
    RECURSIVE_READ_LOCKER(_dataSourceLock, _dataSourceLockWriteOwner);

    auto it = _dataSourceByName.find(name);
    if (it != _dataSourceByName.end() &&
        it->second->category() == LogicalDataSource::Category::kCollection) {
      TRI_ASSERT(std::dynamic_pointer_cast<LogicalCollection>(it->second));
      collection = std::static_pointer_cast<LogicalCollection>(it->second);
    }
  }

  return useCollectionInternal(collection, checkPermissions);
}

std::shared_ptr<LogicalCollection> TRI_vocbase_t::useCollectionInternal(
    std::shared_ptr<LogicalCollection> const& collection,
    bool checkPermissions) {
  if (!collection) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
  }

  // try to load the collection
  Result res = loadCollection(*collection, checkPermissions);
  if (res.fail()) {
    THROW_ARANGO_EXCEPTION(res);
  }
  return collection;
}

void TRI_vocbase_t::releaseCollection(LogicalCollection* collection) noexcept {
  collection->statusLock().unlock();
}

std::shared_ptr<LogicalView> TRI_vocbase_t::createView(
    velocypack::Slice parameters, bool isUserRequest) {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  std::string const& dbName = _info.getName();

  std::string name;
  bool valid = parameters.isObject();
  if (valid) {
    name = VelocyPackHelper::getStringValue(parameters,
                                            StaticStrings::DataSourceName, "");

    valid &= ViewNameValidator::validateName(/*allowSystem*/ false,
                                             _extendedNames, name)
                 .ok();
  }

  if (!valid) {
    events::CreateView(dbName, name, TRI_ERROR_ARANGO_ILLEGAL_NAME);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_ILLEGAL_NAME);
  }

  LogicalView::ptr view;
  auto res = LogicalView::instantiate(view, *this, parameters, isUserRequest);

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
    res = _engine.createView(view->vocbase(), view->id(), *view);

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
  _versionTracker.track("create view");

  view->open();  // And lets open it.

  return view;
}

Result TRI_vocbase_t::dropView(DataSourceId cid, bool allowDropSystem) {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  auto const view = lookupView(cid);
  std::string const& dbName = _info.getName();

  if (!view) {
    events::DropView(dbName, "", TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
    return TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND;
  }

  if (!allowDropSystem && view->system() && !_engine.inRecovery()) {
    events::DropView(dbName, view->name(), TRI_ERROR_FORBIDDEN);
    return TRI_ERROR_FORBIDDEN;  // prevent dropping of system views
  }

  READ_LOCKER(readLocker, _inventoryLock);

  // do not acquire these locks instantly
  RECURSIVE_WRITE_LOCKER_NAMED(writeLocker, _dataSourceLock,
                               _dataSourceLockWriteOwner,
                               basics::ConditionalLocking::DoLock);

  auto res = _engine.dropView(*this, *view);

  if (!res.ok()) {
    events::DropView(dbName, view->name(), res.errorNumber());
    return res;
  }

  // invalidate all entries in the query cache now
  aql::QueryCache::instance()->invalidate(this);

  unregisterView(*view);

  writeLocker.unlock();

  events::DropView(dbName, view->name(), TRI_ERROR_NO_ERROR);
  _versionTracker.track("drop view");

  return TRI_ERROR_NO_ERROR;
}

TRI_vocbase_t::TRI_vocbase_t(arangodb::CreateDatabaseInfo&& info)
    : TRI_vocbase_t(
          std::move(info),
          info.server().getFeature<DatabaseFeature>().versionTracker(),
          info.server().getFeature<DatabaseFeature>().extendedNames()) {}

TRI_vocbase_t::TRI_vocbase_t(CreateDatabaseInfo&& info,
                             VersionTracker& versionTracker, bool extendedNames)
    : _server(info.server()),
      _engine(_server.getFeature<arangodb::EngineSelectorFeature>().engine()),
      _versionTracker(versionTracker),
      _extendedNames(extendedNames),
      _info(std::move(info)) {
  TRI_ASSERT(_info.valid());

  metrics::Gauge<uint64_t>* numberOfCursorsMetric = nullptr;
  metrics::Gauge<uint64_t>* memoryUsageMetric = nullptr;

  if (_info.server().hasFeature<metrics::MetricsFeature>()) {
    metrics::MetricsFeature& feature =
        _info.server().getFeature<metrics::MetricsFeature>();
    _metrics = VocbaseMetrics::create(feature, _info.getName());
  } else {
    _metrics = std::make_unique<VocbaseMetrics>();
  }

  if (_info.server().hasFeature<QueryRegistryFeature>()) {
    QueryRegistryFeature& feature =
        _info.server().getFeature<QueryRegistryFeature>();
    _queries = std::make_unique<aql::QueryList>(feature);

    numberOfCursorsMetric = feature.cursorsMetric();
    memoryUsageMetric = feature.cursorsMemoryUsageMetric();
  }
  _cursorRepository = std::make_unique<CursorRepository>(
      *this, numberOfCursorsMetric, memoryUsageMetric);

  if (_info.server().hasFeature<ReplicationFeature>()) {
    auto& rf = _info.server().getFeature<ReplicationFeature>();
    _replicationClients =
        std::make_unique<ReplicationClientsProgressTracker>(&rf);
  } else {
    // during unit testing, there is no ReplicationFeature available
    _replicationClients =
        std::make_unique<ReplicationClientsProgressTracker>(nullptr);
  }

  // init collections
  _collections.reserve(32);
  _deadCollections.reserve(32);

#ifdef USE_V8
  _cacheData = std::make_unique<DatabaseJavaScriptCache>();
#endif
  _logManager = std::make_shared<VocBaseLogManager>(*this, name());
}

#ifdef ARANGODB_USE_GOOGLE_TESTS
TRI_vocbase_t::TRI_vocbase_t(TRI_vocbase_t::MockConstruct,
                             CreateDatabaseInfo&& info,
                             arangodb::StorageEngine& engine,
                             arangodb::VersionTracker& versionTracker,
                             bool extendedNames)
    : _server(info.server()),
      _engine(engine),
      _versionTracker(versionTracker),
      _extendedNames(extendedNames),
      _info(std::move(info)),
      _metrics(std::make_unique<VocbaseMetrics>()),
      _logManager(std::make_shared<VocBaseLogManager>(*this, name())) {}
#endif

TRI_vocbase_t::~TRI_vocbase_t() {
  // do a final cleanup of collections
  for (auto& coll : _collections) {
    try {  // simon: this status lock is terrible software design
      transaction::cluster::abortTransactions(*coll);
    } catch (...) { /* ignore */
    }
    WRITE_LOCKER_EVENTUAL(locker, coll->statusLock());
    coll->close();  // required to release indexes
  }
  TRI_ASSERT(_logManager->_guardedData.getLockedGuard()->statesAndLogs.empty());

  // clear before deallocating TRI_vocbase_t members
  _collections.clear();
  _deadCollections.clear();
  _dataSourceById.clear();
  _dataSourceByName.clear();
  _dataSourceByUuid.clear();
}

std::string TRI_vocbase_t::path() const { return _engine.databasePath(); }

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
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
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

void TRI_vocbase_t::setShardingPrototype(ShardingPrototype type) {
  _info.shardingPrototype(type);
}

void TRI_vocbase_t::setSharding(std::string_view sharding) {
  _info.setSharding(sharding);
}

ShardingPrototype TRI_vocbase_t::shardingPrototype() const {
  return _info.shardingPrototype();
}

std::string const& TRI_vocbase_t::shardingPrototypeName() const {
  switch (_info.shardingPrototype()) {
    case ShardingPrototype::Users:
      // Specifically set defaults should win
      return StaticStrings::UsersCollection;
    case ShardingPrototype::Graphs:
      // Specifically set defaults should win
      return StaticStrings::GraphCollection;
    case ShardingPrototype::Undefined:
      if (isSystem()) {
        // The sharding Prototype for system databases is always the users
        return StaticStrings::UsersCollection;
      } else {
        // All others should follow _graphs
        return StaticStrings::GraphCollection;
      }
  }
}

std::vector<std::shared_ptr<LogicalView>> TRI_vocbase_t::views() const {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  std::vector<std::shared_ptr<LogicalView>> views;

  {
    RECURSIVE_READ_LOCKER(_dataSourceLock, _dataSourceLockWriteOwner);
    views.reserve(_dataSourceById.size());

    for (auto& entry : _dataSourceById) {
      TRI_ASSERT(entry.second);

      if (entry.second->category() != LogicalDataSource::Category::kView) {
        continue;
      }
      auto view = basics::downCast<LogicalView>(entry.second);
      views.emplace_back(std::move(view));
    }
  }

  return views;
}

void TRI_vocbase_t::processCollectionsOnShutdown(
    std::function<void(LogicalCollection*)> const& cb) {
  std::vector<std::shared_ptr<LogicalCollection>> collections;

  // make a copy of _collections, so we can call the callback function without
  // the lock
  {
    RECURSIVE_READ_LOCKER(_dataSourceLock, _dataSourceLockWriteOwner);
    collections = _collections;
  }

  for (auto const& it : collections) {
    cb(it.get());
  }
}

void TRI_vocbase_t::processCollections(
    std::function<void(LogicalCollection*)> const& cb) {
  RECURSIVE_READ_LOCKER(_dataSourceLock, _dataSourceLockWriteOwner);

  for (auto& entry : _dataSourceById) {
    TRI_ASSERT(entry.second);

    if (entry.second->category() != LogicalDataSource::Category::kCollection) {
      continue;
    }

    auto* collection = basics::downCast<LogicalCollection>(entry.second.get());
    cb(collection);
  }
}

std::vector<std::shared_ptr<LogicalCollection>> TRI_vocbase_t::collections(
    bool includeDeleted) const {
  RECURSIVE_READ_LOCKER(_dataSourceLock, _dataSourceLockWriteOwner);

  if (includeDeleted) {
    return _collections;  // create copy
  }

  std::vector<std::shared_ptr<LogicalCollection>> collections;

  collections.reserve(_dataSourceById.size());

  for (auto const& entry : _dataSourceById) {
    TRI_ASSERT(entry.second);

    if (entry.second->category() != LogicalDataSource::Category::kCollection) {
      continue;
    }
    auto collection = basics::downCast<LogicalCollection>(entry.second);
    collections.emplace_back(std::move(collection));
  }

  return collections;
}

using namespace replication2;

void TRI_vocbase_t::shutdownReplicatedLogs() noexcept {
  _logManager->resignAll();
}

void TRI_vocbase_t::dropReplicatedLogs() noexcept {
  return _logManager->prepareDropAll();
}

auto TRI_vocbase_t::updateReplicatedState(
    LogId id, agency::LogPlanTermSpecification const& term,
    agency::ParticipantsConfig const& config) -> Result {
  return _logManager->updateReplicatedState(id, term, config);
}

auto TRI_vocbase_t::getReplicatedLogLeaderById(LogId id)
    -> std::shared_ptr<replicated_log::ILogLeader> {
  auto log = getReplicatedLogById(id);
  auto participant = std::dynamic_pointer_cast<replicated_log::ILogLeader>(
      log->getParticipant());
  if (participant == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_REPLICATION_REPLICATED_LOG_NOT_THE_LEADER);
  }
  return participant;
}

auto TRI_vocbase_t::getReplicatedLogFollowerById(LogId id)
    -> std::shared_ptr<replicated_log::ILogFollower> {
  auto log = getReplicatedLogById(id);
  auto participant = std::dynamic_pointer_cast<replicated_log::ILogFollower>(
      log->getParticipant());
  if (participant == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_REPLICATION_REPLICATED_LOG_NOT_A_FOLLOWER);
  }
  return participant;
}

auto TRI_vocbase_t::getReplicatedLogById(LogId id)
    -> std::shared_ptr<replicated_log::ReplicatedLog> {
  auto guard = _logManager->_guardedData.getLockedGuard();
  if (auto iter = guard->statesAndLogs.find(id);
      iter != guard->statesAndLogs.end()) {
    return iter->second.log;
  } else {
    throw basics::Exception::fmt(
        ADB_HERE, TRI_ERROR_REPLICATION_REPLICATED_LOG_NOT_FOUND, id);
  }
}

auto TRI_vocbase_t::getReplicatedLogsStatusMap() const
    -> std::unordered_map<LogId, maintenance::LogStatus> {
  return _logManager->getReplicatedLogsStatusMap();
}

auto TRI_vocbase_t::getReplicatedStatesStatus() const
    -> std::unordered_map<LogId, replicated_log::LogStatus> {
  return _logManager->getReplicatedStatesStatus();
}

auto TRI_vocbase_t::createReplicatedState(LogId id, std::string_view type,
                                          VPackSlice parameter)
    -> ResultT<std::shared_ptr<replicated_state::ReplicatedStateBase>> {
  return _logManager->createReplicatedState(id, type, parameter);
}

auto TRI_vocbase_t::dropReplicatedState(LogId id) noexcept -> Result {
  return _logManager->dropReplicatedState(id);
}

auto TRI_vocbase_t::getReplicatedStateById(LogId id)
    -> ResultT<std::shared_ptr<replicated_state::ReplicatedStateBase>> {
  return _logManager->getReplicatedStateById(id);
}

void TRI_vocbase_t::registerReplicatedState(
    replication2::LogId id,
    std::unique_ptr<replication2::storage::IStorageEngineMethods> methods) {
  _logManager->registerReplicatedState(id, std::move(methods));
}

void TRI_SanitizeObject(VPackSlice slice, VPackBuilder& builder) {
  TRI_ASSERT(slice.isObject());
  VPackObjectIterator it(slice);
  while (it.valid()) {
    std::string_view key(it.key().stringView());
    // _id, _key, _rev. minimum size here is 3
    if (key.size() < 3 || key[0] != '_' ||
        (key != StaticStrings::KeyString && key != StaticStrings::IdString &&
         key != StaticStrings::RevString)) {
      builder.add(key, it.value());
    }
    it.next();
  }
}

[[nodiscard]] auto TRI_vocbase_t::getDatabaseConfiguration()
    -> DatabaseConfiguration {
  auto& cl = server().getFeature<ClusterFeature>();

  auto config = std::invoke([&]() -> DatabaseConfiguration {
    if (!ServerState::instance()->isCoordinator() &&
        !ServerState::instance()->isDBServer()) {
      return {[]() { return DataSourceId(TRI_NewTickServer()); },
              [this](std::string const& name)
                  -> ResultT<UserInputCollectionProperties> {
                CollectionNameResolver resolver{*this};
                auto c = resolver.getCollection(name);
                if (c == nullptr) {
                  return Result{TRI_ERROR_CLUSTER_UNKNOWN_DISTRIBUTESHARDSLIKE,
                                absl::StrCat("Collection not found: ", name,
                                             " in database ", this->name())};
                }
                return c->getCollectionProperties();
              }};
    } else {
      auto& ci = cl.clusterInfo();
      return {[&ci]() { return DataSourceId(ci.uniqid(1)); },
              [this](std::string const& name)
                  -> ResultT<UserInputCollectionProperties> {
                CollectionNameResolver resolver{*this};
                auto c = resolver.getCollection(name);
                if (c == nullptr) {
                  return Result{TRI_ERROR_CLUSTER_UNKNOWN_DISTRIBUTESHARDSLIKE,
                                absl::StrCat("Collection not found: ", name,
                                             " in database ", this->name())};
                }
                return c->getCollectionProperties();
              }};
    }
  });

  config.isSystemDB = isSystem();
  config.maxNumberOfShards = cl.maxNumberOfShards();
  config.allowExtendedNames = _extendedNames;
  config.shouldValidateClusterSettings = true;
  config.minReplicationFactor = cl.minReplicationFactor();
  config.maxReplicationFactor = cl.maxReplicationFactor();
  config.enforceReplicationFactor = true;
  config.defaultNumberOfShards = 1;
  config.defaultReplicationFactor = replicationFactor();
  config.defaultWriteConcern = writeConcern();

  config.isOneShardDB = cl.forceOneShard() || isOneShard();
  if (config.isOneShardDB) {
    config.defaultDistributeShardsLike = shardingPrototypeName();
  } else {
    config.defaultDistributeShardsLike = "";
  }

  config.replicationVersion = replicationVersion();

  return config;
}
