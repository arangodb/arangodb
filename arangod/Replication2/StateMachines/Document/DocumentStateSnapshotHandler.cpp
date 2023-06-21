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
/// @author Alexandru Petenchea
////////////////////////////////////////////////////////////////////////////////

#include "Replication2/StateMachines/Document/DocumentStateSnapshotHandler.h"

#include "Basics/ScopeGuard.h"
#include "Cluster/ClusterTypes.h"
#include "Replication2/StateMachines/Document/CollectionReader.h"

namespace arangodb::replication2::replicated_state::document {
DocumentStateSnapshotHandler::DocumentStateSnapshotHandler(
    std::unique_ptr<IDatabaseSnapshotFactory> databaseSnapshotFactory,
    cluster::RebootTracker& rebootTracker)
    : _databaseSnapshotFactory(std::move(databaseSnapshotFactory)),
      _rebootTracker(rebootTracker) {}

auto DocumentStateSnapshotHandler::create(ShardMap shards,
                                          SnapshotParams::Start const& params)
    -> ResultT<std::weak_ptr<Snapshot>> {
  try {
    auto databaseSnapshot = _databaseSnapshotFactory->createSnapshot();
    auto id = SnapshotId::create();

    SnapshotGuard guard{};
    guard.snapshot = std::make_shared<Snapshot>(id, std::move(shards),
                                                std::move(databaseSnapshot));
    auto emplacement = _snapshots.emplace(id, std::move(guard));
    TRI_ASSERT(emplacement.second);
    ScopeGuard sc{[&]() noexcept { abort(id); }};

    // We must register the callback only after the emplacement, because the
    // rebootID of the follower might have changed in the meantime. Also, in
    // that case, the emplacement iterator will be invalidated, so we create the
    // result here.
    auto& guardRef = emplacement.first->second;
    auto res = ResultT<std::weak_ptr<Snapshot>>::success(guardRef.snapshot);
    guardRef.cbGuard = _rebootTracker.callMeOnChange(
        cluster::RebootTracker::PeerState{params.serverId, params.rebootId},
        [id, params, weak = weak_from_this()]() {
          if (auto self = weak.lock(); self != nullptr) {
            self->abort(id);
          }
        },
        fmt::format("Snapshot {} aborted because the follower rebooted", id));

    sc.cancel();
    return res;
  } catch (basics::Exception const& ex) {
    return ResultT<std::weak_ptr<Snapshot>>::error(ex.code(), ex.what());
  } catch (std::exception const& ex) {
    return ResultT<std::weak_ptr<Snapshot>>::error(TRI_ERROR_INTERNAL,
                                                   ex.what());
  } catch (...) {
    return ResultT<std::weak_ptr<Snapshot>>::error(TRI_ERROR_INTERNAL,
                                                   "unknown exception");
  }
}

auto DocumentStateSnapshotHandler::find(SnapshotId const& id) noexcept
    -> ResultT<std::weak_ptr<Snapshot>> {
  if (auto it = _snapshots.find(id); it != _snapshots.end()) {
    return ResultT<std::weak_ptr<Snapshot>>::success(it->second.snapshot);
  }
  return ResultT<std::weak_ptr<Snapshot>>::error(
      TRI_ERROR_INTERNAL, fmt::format("Snapshot {} not found", id));
}

auto DocumentStateSnapshotHandler::abort(SnapshotId const& id) -> Result {
  if (auto it = _snapshots.find(id); it != _snapshots.end()) {
    auto res = it->second->abort();
    _snapshots.erase(it);
    return res;
  }
  return Result{TRI_ERROR_INTERNAL, fmt::format("Snapshot {} not found", id)};
}

auto DocumentStateSnapshotHandler::finish(SnapshotId const& id) -> Result {
  if (auto it = _snapshots.find(id); it != _snapshots.end()) {
    auto res = it->second->finish();
    _snapshots.erase(it);
    return res;
  }
  return Result{TRI_ERROR_INTERNAL, fmt::format("Snapshot {} not found", id)};
}

auto DocumentStateSnapshotHandler::status() const -> AllSnapshotsStatus {
  AllSnapshotsStatus result;
  for (auto const& it : _snapshots) {
    result.snapshots.emplace(it.first, it.second->status());
  }
  return result;
}

void DocumentStateSnapshotHandler::clear() {
  for (auto& [_, snapshot] : _snapshots) {
    snapshot->abort();
  }
  _snapshots.clear();
}

void DocumentStateSnapshotHandler::giveUpOnShard(ShardID const& shardId) {
  std::erase_if(_snapshots, [&](auto& it) {
    if (auto res = it.second->giveUpOnShard(shardId); res.fail()) {
      LOG_TOPIC("b08ba", ERR, Logger::REPLICATION2)
          << "Failed to reset snapshot " << it.first << " containing shard "
          << shardId << ", the snapshot will be aborted: " << res;
      it.second->abort();
      return true;
    }
    return false;
  });
}

DocumentStateSnapshotHandler::SnapshotGuard::~SnapshotGuard() {
  if (snapshot != nullptr && !snapshot->isInactive()) {
    LOG_TOPIC("6eb3f", WARN, Logger::REPLICATION2)
        << "Active snapshot " << snapshot->config().snapshotId
        << " destroyed, current state is: " << snapshot->status().state;
  }
}

}  // namespace arangodb::replication2::replicated_state::document
