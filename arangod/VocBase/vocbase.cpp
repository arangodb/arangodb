////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/QueryCache.h"
#include "Aql/QueryList.h"
#include "Auth/Common.h"
#include "Basics/application-exit.h"
#include "Basics/Exceptions.h"
#include "Basics/Exceptions.tpp"
#include "Basics/HybridLogicalClock.h"
#include "Basics/Locking.h"
#include "Basics/NumberUtils.h"
#include "Basics/DownCast.h"
#include "Basics/ReadLocker.h"
#include "Basics/RecursiveLocker.h"
#include "Basics/Result.h"
#include "Basics/Result.tpp"
#include "Basics/ScopeGuard.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/Utf8Helper.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Basics/debugging.h"
#include "Basics/error.h"
#include "Basics/system-functions.h"
#include "Basics/voc-errors.h"
#include "Containers/Helpers.h"
#include "Cluster/FailureOracleFeature.h"
#include "Cluster/ServerState.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Indexes/Index.h"
#include "Logger/LogContextKeys.h"
#include "Logger/LogMacros.h"
#include "Replication/DatabaseReplicationApplier.h"
#include "Replication/ReplicationClients.h"
#include "Replication2/LoggerContext.h"
#include "Replication2/ReplicatedLog/ILogInterfaces.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/ReplicatedLog/LogCore.h"
#include "Replication2/ReplicatedLog/LogFollower.h"
#include "Replication2/ReplicatedLog/LogLeader.h"
#include "Replication2/ReplicatedLog/LogStatus.h"
#include "Replication2/ReplicatedLog/NetworkAttachedFollower.h"
#include "Replication2/ReplicatedLog/PersistedLog.h"
#include "Replication2/ReplicatedLog/ReplicatedLog.h"
#include "Replication2/ReplicatedLog/ReplicatedLogFeature.h"
#include "Replication2/ReplicatedLog/ReplicatedLogMetrics.h"
#include "Replication2/ReplicatedState/ReplicatedState.h"
#include "Replication2/ReplicatedState/ReplicatedStateFeature.h"
#include "Replication2/ReplicatedState/PersistedStateInfo.h"
#include "Replication2/Version.h"
#include "Metrics/Counter.h"
#include "Metrics/Gauge.h"
#include "Network/ConnectionPool.h"
#include "Network/NetworkFeature.h"
#include "Replication/ReplicationFeature.h"
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
#include "VocBase/Properties/DatabaseConfiguration.h"
#include "VocBase/Properties/UserInputCollectionProperties.h"

#include <thread>
#include <absl/strings/str_cat.h>

using namespace arangodb;
using namespace arangodb::basics;

struct arangodb::VocBaseLogManager {
  explicit VocBaseLogManager(TRI_vocbase_t& vocbase, DatabaseID database)
      : _server(vocbase.server()),
        _vocbase(vocbase),
        _logContext(
            LoggerContext{Logger::REPLICATION2}.with<logContextKeyDatabaseName>(
                std::move(database))) {}

  [[nodiscard]] auto getReplicatedStateById(replication2::LogId id) -> ResultT<
      std::shared_ptr<replication2::replicated_state::ReplicatedStateBase>> {
    auto guard = _guardedData.getLockedGuard();
    if (auto iter = guard->statesAndLogs.find(id);
        iter != guard->statesAndLogs.end()) {
      return {iter->second.state};
    } else {
      return Result(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                    fmt::format("replicated state {} not found", id.id()));
    }
  }

  void registerReplicatedState(
      replication2::LogId id,
      std::unique_ptr<replication2::replicated_state::IStorageEngineMethods>) {
    ADB_PROD_ASSERT(false) << "register not yet implemented";
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  auto resignAll() {
    auto guard = _guardedData.getLockedGuard();
    for (auto&& [id, val] : guard->statesAndLogs) {
      auto&& log = val.log;
      auto core = std::move(*log).resign();
      core.reset();
    }
    guard->statesAndLogs.clear();
  }

  auto updateReplicatedState(
      replication2::LogId id,
      replication2::agency::LogPlanTermSpecification const& term,
      replication2::agency::ParticipantsConfig const& config) -> Result {
    auto guard = _guardedData.getLockedGuard();
    if (auto iter = guard->statesAndLogs.find(id);
        iter != guard->statesAndLogs.end()) {
      iter->second.log->updateConfig(term, config)
          .thenFinal([&voc = _vocbase](auto&&) {
            voc.server().getFeature<ClusterFeature>().addDirty(voc.name());
          });
      return {};
    } else {
      return {TRI_ERROR_REPLICATION_REPLICATED_LOG_NOT_FOUND};
    }
  }

  [[nodiscard]] auto dropReplicatedState(replication2::LogId id) -> Result {
    // TODO handle exceptions, maybe terminate the process if this fails.
    //      also make sure that leftovers are cleaned up during startup!
    LOG_CTX("658c6", DEBUG, _logContext) << "Dropping replicated state " << id;
    StorageEngine& engine =
        _server.getFeature<EngineSelectorFeature>().engine();
    auto result = _guardedData.doUnderLock([&](GuardedData& data) {
      if (auto iter = data.statesAndLogs.find(id);
          iter != data.statesAndLogs.end()) {
        auto& state = iter->second.state;
        auto& log = iter->second.log;
        auto& storage = iter->second.storage;

        auto metadata = storage->readMetadata();
        if (metadata.fail()) {
          return std::move(metadata).result();  // state untouched after this
        }

        // Get the state handle so we can drop the state later
        auto stateHandle = log->disconnect(std::move(iter->second.connection));

        // resign the log now, before we update the metadata to avoid races
        // on the storage.
        auto core = std::move(*log).resign();

        // Invalidate the snapshot in persistent storage.
        metadata->snapshot.updateStatus(
            replication2::replicated_state::SnapshotStatus::kInvalidated);
        // TODO check return value
        // TODO make sure other methods working on the state, probably meaning
        //      configuration updates, handle an invalidated snapshot correctly.
        if (auto res = storage->updateMetadata(*metadata); res.fail()) {
          LOG_CTX("6e6f0", ERR, _logContext)
              << "failed to drop replicated log " << res.errorMessage();
          THROW_ARANGO_EXCEPTION(res);
        }

        // Drop the replicated state. This will also remove its associated
        // resources, e.g. the shard/collection will be dropped.
        // This must happen only after the snapshot is persistently marked as
        // failed.
        std::move(*state).drop(std::move(stateHandle));

        // Now we may delete the persistent metadata.
        auto res = engine.dropReplicatedState(_vocbase, storage);

        if (res.fail()) {
          TRI_ASSERT(storage != nullptr);
          LOG_CTX("998cc", ERR, _logContext)
              << "failed to drop replicated log " << res.errorMessage();
          return res;
        }
        TRI_ASSERT(storage == nullptr);
        data.statesAndLogs.erase(iter);

        return Result();
      } else {
        return Result(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
      }

      return Result();
    });

    if (result.ok()) {
      auto& feature = _server.getFeature<ReplicatedLogFeature>();
      feature.metrics()->replicatedLogNumber->fetch_sub(1);
      feature.metrics()->replicatedLogDeletionNumber->count();
    }

    return result;
  }

  [[nodiscard]] auto getReplicatedLogsQuickStatus() const
      -> std::unordered_map<replication2::LogId,
                            replication2::replicated_log::QuickLogStatus> {
    std::unordered_map<replication2::LogId,
                       replication2::replicated_log::QuickLogStatus>
        result;
    auto guard = _guardedData.getLockedGuard();
    for (auto& [id, value] : guard->statesAndLogs) {
      result.emplace(id, value.log->getQuickStatus());
    }
    return result;
  }

  [[nodiscard]] auto getReplicatedStatesStatus() const
      -> std::unordered_map<replication2::LogId,
                            replication2::replicated_log::LogStatus> {
    std::unordered_map<replication2::LogId,
                       replication2::replicated_log::LogStatus>
        result;
    auto guard = _guardedData.getLockedGuard();
    for (auto& [id, value] : guard->statesAndLogs) {
      result.emplace(id, value.log->getStatus());
    }
    return result;
  }

  auto createReplicatedState(replication2::LogId id, std::string_view type,
                             VPackSlice parameter)
      -> ResultT<std::shared_ptr<
          replication2::replicated_state::ReplicatedStateBase>> {
    auto& feature = _server.getFeature<
        replication2::replicated_state::ReplicatedStateAppFeature>();
    return _guardedData.doUnderLock(
        [&](GuardedData& data)
            -> ResultT<std::shared_ptr<
                replication2::replicated_state::ReplicatedStateBase>> {
          auto state = data.buildReplicatedState(
              id, type, parameter, feature,
              _logContext.withTopic(Logger::REPLICATED_STATE), _server,
              _vocbase);
          LOG_CTX("2bf8d", DEBUG, _logContext)
              << "Created replicated state " << id << " impl = " << type
              << " result = " << state.errorNumber();

          return state;
        });
  }

  ArangodServer& _server;
  TRI_vocbase_t& _vocbase;
  LoggerContext const _logContext;

  struct GuardedData {
    struct StateAndLog {
      cluster::CallbackGuard rebootTrackerGuard;
      std::shared_ptr<replication2::replicated_log::ReplicatedLog> log;
      std::shared_ptr<replication2::replicated_state::ReplicatedStateBase>
          state;
      std::unique_ptr<replication2::replicated_state::IStorageEngineMethods>
          storage;
      replication2::replicated_log::ReplicatedLogConnection connection;
    };
    absl::flat_hash_map<replication2::LogId, StateAndLog> statesAndLogs;

    void registerRebootTracker(
        replication2::LogId id,
        replication2::agency::ServerInstanceReference myself,
        ArangodServer& server, TRI_vocbase_t& vocbase) {
      auto& rbt =
          server.getFeature<ClusterFeature>().clusterInfo().rebootTracker();

      if (auto iter = statesAndLogs.find(id); iter != statesAndLogs.end()) {
        iter->second.rebootTrackerGuard = rbt.callMeOnChange(
            {myself.serverId, myself.rebootId},
            [log = iter->second.log, id, &vocbase, &server] {
              auto myself = replication2::agency::ServerInstanceReference(
                  ServerState::instance()->getId(),
                  ServerState::instance()->getRebootId());
              log->updateMyInstanceReference(myself);
              vocbase._logManager->_guardedData.getLockedGuard()
                  ->registerRebootTracker(id, myself, server, vocbase);
            },
            "update reboot id in replicated log");
      }
    }

    auto buildReplicatedState(
        replication2::LogId const id, std::string_view type,
        VPackSlice parameters,
        replication2::replicated_state::ReplicatedStateAppFeature& feature,
        LoggerContext const& logContext, ArangodServer& server,
        TRI_vocbase_t& vocbase)
        -> ResultT<std::shared_ptr<
            replication2::replicated_state::ReplicatedStateBase>> try {
      // TODO Make this atomic without crashing on errors if possible
      using namespace replication2;
      using namespace replication2::replicated_state;

      if (auto iter = statesAndLogs.find(id); iter != std::end(statesAndLogs)) {
        return {TRI_ERROR_ARANGO_DUPLICATE_IDENTIFIER};
      }

      // 1. Allocate all memory
      //    1.1 allocate node in map.
      //    1.2 allocate state (and log)
      //    1.2 allocate state (and log)
      // 2. write to storage engine
      // ---- from now on no errors are allowed
      // 3. forward storage interface to replicated state
      // 4. start up with initial configuration
      StorageEngine& engine =
          server.getFeature<EngineSelectorFeature>().engine();

      LOG_CTX("ef73d", DEBUG, logContext)
          << "building new replicated state " << id << " impl = " << type;

      // prepare map
      auto stateAndLog = StateAndLog{};

      {
        VPackBufferUInt8 buffer;
        buffer.append(parameters.start(), parameters.byteSize());
        auto parametersCopy = velocypack::SharedSlice(std::move(buffer));

        auto metadata = PersistedStateInfo{
            .stateId = id,
            .snapshot = {.status = replicated_state::SnapshotStatus::kCompleted,
                         .timestamp = {},
                         .error = {}},
            .generation = {},
            .specification = {.type = std::string(type),
                              .parameters = std::move(parametersCopy)},
        };
        auto maybeStorage = engine.createReplicatedState(vocbase, id, metadata);

        if (maybeStorage.fail()) {
          return std::move(maybeStorage).result();
        }
        stateAndLog.storage = std::move(*maybeStorage);
      }

      struct NetworkFollowerFactory
          : replication2::replicated_log::IAbstractFollowerFactory {
        NetworkFollowerFactory(TRI_vocbase_t& vocbase, replication2::LogId id)
            : vocbase(vocbase), id(id) {}
        auto constructFollower(const ParticipantId& participantId)
            -> std::shared_ptr<
                replication2::replicated_log::AbstractFollower> override {
          auto* pool = vocbase.server().getFeature<NetworkFeature>().pool();

          return std::make_shared<
              replication2::replicated_log::NetworkAttachedFollower>(
              pool, participantId, vocbase.name(), id);
        }

        auto constructLeaderCommunicator(const ParticipantId& participantId)
            -> std::shared_ptr<replicated_log::ILeaderCommunicator> override {
          auto* pool = vocbase.server().getFeature<NetworkFeature>().pool();

          return std::make_shared<
              replication2::replicated_log::NetworkLeaderCommunicator>(
              pool, participantId, vocbase.name(), id);
        }

        TRI_vocbase_t& vocbase;
        replication2::LogId id;
      };

      auto myself = replication2::agency::ServerInstanceReference(
          ServerState::instance()->getId(),
          ServerState::instance()->getRebootId());

      auto& log = stateAndLog.log = std::invoke([&]() {
        auto&& logCore =
            std::make_unique<replication2::replicated_log::LogCore>(
                *stateAndLog.storage);
        return std::make_shared<replication2::replicated_log::ReplicatedLog>(
            std::move(logCore),
            server.getFeature<ReplicatedLogFeature>().metrics(),
            server.getFeature<ReplicatedLogFeature>().options(),
            std::make_shared<replicated_log::DefaultParticipantsFactory>(
                std::make_shared<NetworkFollowerFactory>(vocbase, id)),
            logContext, myself);
      });

      auto& state = stateAndLog.state = feature.createReplicatedState(
          type, vocbase.name(), id, log, logContext);

      auto maybeMetadata = stateAndLog.storage->readMetadata();
      if (!maybeMetadata) {
        throw basics::Exception(std::move(maybeMetadata).result(), ADB_HERE);
      }
      auto const& stateParams = maybeMetadata->specification.parameters;
      auto&& stateHandle = state->createStateHandle(stateParams);

      stateAndLog.connection = log->connect(std::move(stateHandle));

      auto iter = statesAndLogs.emplace(id, std::move(stateAndLog));
      registerRebootTracker(id, myself, server, vocbase);
      auto metrics = server.getFeature<ReplicatedLogFeature>().metrics();
      metrics->replicatedLogNumber->fetch_add(1);
      metrics->replicatedLogCreationNumber->count();

      return iter.first->second.state;
    } catch (std::exception& ex) {
      // If we created the state on-disk, but failed to add it to the map, we
      // cannot continue safely.
      LOG_TOPIC("35daf", FATAL, Logger::REPLICATION2)
          << "Failed to create replicated state: " << ex.what();
      std::abort();
    } catch (...) {
      std::abort();
    }
  };
  Guarded<GuardedData> _guardedData;
};

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

  StorageEngine& engine = server().getFeature<EngineSelectorFeature>().engine();
  engine.prepareDropCollection(*this, collection);

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

    engine.prepareDropCollection(*this, collection);

    // sleep for a while
    std::this_thread::yield();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  TRI_ASSERT(writeLocker.isLocked());
  TRI_ASSERT(locker.isLocked());

  aql::QueryCache::instance()->invalidate(this);

  collection.setDeleted();

  VPackBuilder builder;
  engine.getCollectionInfo(*this, collection.id(), builder, false, 0);

  res = collection.properties(builder.slice().get("parameters"));

  if (res.ok()) {
    unregisterCollection(collection);

    locker.unlock();
    writeLocker.unlock();

    engine.dropCollection(*this, collection);
  }
  return res;
}

void TRI_vocbase_t::stop() {
  try {
    _logManager->resignAll();

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
    _dataSourceByName.clear();
    _dataSourceById.clear();
    _dataSourceByUuid.clear();
    checkCollectionInvariants();
  }

  _deadCollections.clear();
  _collections.clear();
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
  StorageEngine& engine = server().getFeature<EngineSelectorFeature>().engine();

  VPackBuilder merged;
  merged.openObject();
  engine.addParametersForNewCollection(merged, parameters);
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

  auto& df = server().getFeature<DatabaseFeature>();
  if (df.versionTracker() != nullptr) {
    df.versionTracker()->track("create collection");
  }

  return collections;
}

Result TRI_vocbase_t::dropCollection(DataSourceId cid, bool allowDropSystem) {
  auto collection = lookupCollection(cid);
  auto const& dbName = _info.getName();

  if (!collection) {
    events::DropCollection(dbName, "", TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
    return TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND;
  }

  StorageEngine& engine = server().getFeature<EngineSelectorFeature>().engine();

  if (!allowDropSystem && collection->system() && !engine.inRecovery()) {
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

    auto& df = server().getFeature<DatabaseFeature>();
    if (df.versionTracker() != nullptr) {
      df.versionTracker()->track("drop collection");
    }
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
  bool extendedNames = server().getFeature<DatabaseFeature>().extendedNames();
  if (auto res =
          CollectionNameValidator::validateName(isSystem, extendedNames, name);
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
    return Result(
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

  bool extendedNames = databaseFeature.extendedNames();
  if (auto res = ViewNameValidator::validateName(/*allowSystem*/ false,
                                                 extendedNames, newName);
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
  if (!engine.inRecovery()) {
    velocypack::Builder build;
    build.openObject();
    auto r = view->properties(
        build, LogicalDataSource::Serialization::PersistenceWithInProgress);
    if (!r.ok()) {
      return r;
    }
    r = engine.changeView(*view, build.close().slice());
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

  bool extendedNames = server().getFeature<DatabaseFeature>().extendedNames();
  if (auto res = CollectionNameValidator::validateName(/*allowSystem*/ false,
                                                       extendedNames, newName);
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

  auto& engine = server().getFeature<EngineSelectorFeature>().engine();

  res =
      engine.renameCollection(*this, *collection, oldName);  // tell the engine

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

  auto& df = server().getFeature<DatabaseFeature>();
  if (df.versionTracker() != nullptr) {
    df.versionTracker()->track("rename collection");
  }

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
  auto& engine = server().getFeature<EngineSelectorFeature>().engine();
  std::string const& dbName = _info.getName();

  std::string name;
  bool valid = parameters.isObject();
  if (valid) {
    name = VelocyPackHelper::getStringValue(parameters,
                                            StaticStrings::DataSourceName, "");

    bool extendedNames = server().getFeature<DatabaseFeature>().extendedNames();
    valid &= ViewNameValidator::validateName(/*allowSystem*/ false,
                                             extendedNames, name)
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
    res = engine.createView(view->vocbase(), view->id(), *view);

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

Result TRI_vocbase_t::dropView(DataSourceId cid, bool allowDropSystem) {
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
  RECURSIVE_WRITE_LOCKER_NAMED(writeLocker, _dataSourceLock,
                               _dataSourceLockWriteOwner,
                               basics::ConditionalLocking::DoLock);

  auto res = engine.dropView(*this, *view);

  if (!res.ok()) {
    events::DropView(dbName, view->name(), res.errorNumber());
    return res;
  }

  // invalidate all entries in the query cache now
  aql::QueryCache::instance()->invalidate(this);

  unregisterView(*view);

  writeLocker.unlock();

  events::DropView(dbName, view->name(), TRI_ERROR_NO_ERROR);

  auto& df = server().getFeature<DatabaseFeature>();
  if (df.versionTracker() != nullptr) {
    df.versionTracker()->track("drop view");
  }

  return TRI_ERROR_NO_ERROR;
}

TRI_vocbase_t::TRI_vocbase_t(CreateDatabaseInfo&& info)
    : _server(info.server()), _info(std::move(info)), _deadlockDetector(false) {
  TRI_ASSERT(_info.valid());

  if (_info.server().hasFeature<QueryRegistryFeature>()) {
    QueryRegistryFeature& feature =
        _info.server().getFeature<QueryRegistryFeature>();
    _queries = std::make_unique<aql::QueryList>(feature);
  }
  _cursorRepository = std::make_unique<CursorRepository>(*this);

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

  _cacheData = std::make_unique<DatabaseJavaScriptCache>();
  _logManager = std::make_shared<VocBaseLogManager>(*this, name());
}

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
  // clear before deallocating TRI_vocbase_t members
  _collections.clear();
  _deadCollections.clear();
  _dataSourceById.clear();
  _dataSourceByName.clear();
  _dataSourceByUuid.clear();
}

std::string TRI_vocbase_t::path() const {
  StorageEngine& engine = server().getFeature<EngineSelectorFeature>().engine();
  return engine.databasePath();
}

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

ShardingPrototype TRI_vocbase_t::shardingPrototype() const {
  return _info.shardingPrototype();
}

std::string const& TRI_vocbase_t::shardingPrototypeName() const {
  return _info.shardingPrototype() == ShardingPrototype::Users
             ? StaticStrings::UsersCollection
             : StaticStrings::GraphCollection;
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

auto TRI_vocbase_t::updateReplicatedState(
    LogId id, agency::LogPlanTermSpecification const& term,
    agency::ParticipantsConfig const& config) -> Result {
  return _logManager->updateReplicatedState(id, term, config);
}

auto TRI_vocbase_t::getReplicatedLogLeaderById(LogId id)
    -> std::shared_ptr<replicated_log::LogLeader> {
  auto log = getReplicatedLogById(id);
  auto participant = std::dynamic_pointer_cast<replicated_log::LogLeader>(
      log->getParticipant());
  if (participant == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_REPLICATION_REPLICATED_LOG_NOT_THE_LEADER);
  }
  return participant;
}

auto TRI_vocbase_t::getReplicatedLogFollowerById(LogId id)
    -> std::shared_ptr<replicated_log::LogFollower> {
  auto log = getReplicatedLogById(id);
  auto participant = std::dynamic_pointer_cast<replicated_log::LogFollower>(
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
    THROW_ARANGO_EXCEPTION(TRI_ERROR_REPLICATION_REPLICATED_LOG_NOT_FOUND);
  }
}

auto TRI_vocbase_t::getReplicatedStatesQuickStatus() const
    -> std::unordered_map<LogId, replicated_log::QuickLogStatus> {
  return _logManager->getReplicatedLogsQuickStatus();
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
    std::unique_ptr<replication2::replicated_state::IStorageEngineMethods>
        methods) {
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
  auto& db = server().getFeature<DatabaseFeature>();

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
                                "Collection not found: " + name +
                                    " in database " + this->name()};
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
                                "Collection not found: " + name +
                                    " in database " + this->name()};
                }
                return c->getCollectionProperties();
              }};
    }
  });

  config.maxNumberOfShards = cl.maxNumberOfShards();
  config.allowExtendedNames = db.extendedNames();
  config.shouldValidateClusterSettings = true;
  config.minReplicationFactor = cl.minReplicationFactor();
  config.maxReplicationFactor = cl.maxReplicationFactor();
  config.enforceReplicationFactor = true;
  config.defaultNumberOfShards = 1;
  config.defaultReplicationFactor =
      std::max(replicationFactor(), cl.systemReplicationFactor());
  config.defaultWriteConcern = writeConcern();

  config.isOneShardDB = cl.forceOneShard() || isOneShard();
  if (config.isOneShardDB) {
    config.defaultDistributeShardsLike = shardingPrototypeName();
  } else {
    config.defaultDistributeShardsLike = "";
  }

  return config;
}
