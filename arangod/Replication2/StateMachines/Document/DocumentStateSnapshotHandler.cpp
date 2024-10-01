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
/// @author Alexandru Petenchea
////////////////////////////////////////////////////////////////////////////////

#include "Replication2/StateMachines/Document/DocumentStateSnapshotHandler.h"

#include "Basics/ScopeGuard.h"
#include "Cluster/ClusterTypes.h"
#include "Logger/LogContextKeys.h"
#include "Replication2/StateMachines/Document/CollectionReader.h"

namespace arangodb::replication2::replicated_state::document {
DocumentStateSnapshotHandler::DocumentStateSnapshotHandler(
    std::unique_ptr<IDatabaseSnapshotFactory> databaseSnapshotFactory,
    cluster::RebootTracker& rebootTracker, GlobalLogIdentifier gid,
    LoggerContext loggerContext)
    : _databaseSnapshotFactory(std::move(databaseSnapshotFactory)),
      _rebootTracker(rebootTracker),
      _gid(std::move(gid)),
      _loggerContext(std::move(loggerContext)) {}

auto DocumentStateSnapshotHandler::create(
    std::vector<std::shared_ptr<LogicalCollection>> shards,
    SnapshotParams::Start const& params) noexcept
    -> ResultT<std::weak_ptr<Snapshot>> {
  return basics::catchToResultT([&]() -> std::weak_ptr<Snapshot> {
    auto databaseSnapshot = _databaseSnapshotFactory->createSnapshot();
    auto id = SnapshotId::create();

    SnapshotGuard guard{};
    guard.snapshot =
        std::make_shared<Snapshot>(id, _gid, std::move(shards),
                                   std::move(databaseSnapshot), _loggerContext);
    auto emplacement = _snapshots.emplace(id, std::move(guard));
    TRI_ASSERT(emplacement.second)
        << _gid << " snapshot " << id << " already exists";

    ScopeGuard sc{[&]() noexcept { abort(id); }};

    // We must register the callback only after the emplacement, because the
    // rebootID of the follower might have changed in the meantime. Also, in
    // that case, the emplacement iterator will be invalidated, so we create the
    // result here.
    auto& guardRef = emplacement.first->second;
    guardRef.cbGuard = _rebootTracker.callMeOnChange(
        PeerState{params.serverId, params.rebootId},
        [id, weak = weak_from_this()]() {
          if (auto self = weak.lock(); self != nullptr) {
            self->abort(id);
          }
        },
        fmt::format("Snapshot {} aborted because the follower rebooted", id));

    sc.cancel();
    return guardRef.snapshot;
  });
}

auto DocumentStateSnapshotHandler::find(SnapshotId const& id) noexcept
    -> ResultT<std::weak_ptr<Snapshot>> {
  if (auto it = _snapshots.find(id); it != _snapshots.end()) {
    return ResultT<std::weak_ptr<Snapshot>>::success(it->second.snapshot);
  }
  return ResultT<std::weak_ptr<Snapshot>>::error(
      TRI_ERROR_INTERNAL, fmt::format("Snapshot {} not found", id));
}

auto DocumentStateSnapshotHandler::abort(SnapshotId const& id) noexcept
    -> Result {
  if (auto it = _snapshots.find(id); it != _snapshots.end()) {
    if (auto res = basics::catchVoidToResult([&]() { it->second->abort(); });
        res.fail()) {
      LOG_CTX("f6812", DEBUG, it->second->loggerContext)
          << "Snapshot abort failure before erasing snapshot: " << res;
    }
    _snapshots.erase(it);
    return {};
  }
  return Result{TRI_ERROR_INTERNAL, fmt::format("Snapshot {} not found", id)};
}

auto DocumentStateSnapshotHandler::finish(SnapshotId const& id) noexcept
    -> Result {
  if (auto it = _snapshots.find(id); it != _snapshots.end()) {
    auto res = basics::catchToResult([&]() { return it->second->finish(); });
    _snapshots.erase(it);
    return res;
  }
  return Result{TRI_ERROR_INTERNAL, fmt::format("Snapshot {} not found", id)};
}

void DocumentStateSnapshotHandler::clear() noexcept {
  for (auto& snapshot : _snapshots) {
    auto& snapshotGuard = snapshot.second;
    if (auto res = basics::catchVoidToResult([&]() { snapshotGuard->abort(); });
        res.fail()) {
      LOG_CTX("3a2be", DEBUG, snapshotGuard->loggerContext)
          << "Snapshot abort failure before erasing snapshot: " << res;
    }
  }
  _snapshots.clear();
}

auto DocumentStateSnapshotHandler::status() const -> AllSnapshotsStatus {
  AllSnapshotsStatus result;
  for (auto const& it : _snapshots) {
    result.snapshots.emplace(it.first, it.second->status());
  }
  return result;
}

void DocumentStateSnapshotHandler::giveUpOnShard(ShardID const& shardId) {
  std::erase_if(_snapshots, [&](auto& it) {
    if (auto res = it.second->giveUpOnShard(shardId); res.fail()) {
      LOG_CTX("b08ba", ERR, _loggerContext)
          << "Failed to reset snapshot " << it.first << " containing shard "
          << shardId << ", the snapshot will be aborted: " << res;
      it.second->abort();
      return true;
    }
    return false;
  });
}

DocumentStateSnapshotHandler::SnapshotGuard::~SnapshotGuard() {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  if (snapshot != nullptr && !snapshot->isInactive()) {
    LOG_CTX("6eb3f", WARN, snapshot->loggerContext)
        << "Active snapshot " << snapshot->getId()
        << " destroyed, current state is: " << snapshot->status().state;
  }
#endif
}

}  // namespace arangodb::replication2::replicated_state::document
