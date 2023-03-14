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

#include "Cluster/ClusterTypes.h"
#include "Replication2/StateMachines/Document/CollectionReader.h"

namespace arangodb::replication2::replicated_state::document {
DocumentStateSnapshotHandler::DocumentStateSnapshotHandler(
    std::unique_ptr<IDatabaseSnapshotFactory> databaseSnapshotFactory)
    : _databaseSnapshotFactory(std::move(databaseSnapshotFactory)) {}

auto DocumentStateSnapshotHandler::create(ShardMap shards)
    -> ResultT<std::weak_ptr<Snapshot>> {
  try {
    auto snapshot = _databaseSnapshotFactory->createSnapshot();
    auto id = SnapshotId::create();

    std::unique_lock lock(_snapshotsMap.mutex);
    auto emplacement = _snapshotsMap.snapshots.emplace(
        id,
        std::make_shared<Snapshot>(id, std::move(shards), std::move(snapshot)));

    TRI_ASSERT(emplacement.second);
    return ResultT<std::weak_ptr<Snapshot>>::success(emplacement.first->second);
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

auto DocumentStateSnapshotHandler::find(SnapshotId const& id)
    -> ResultT<std::weak_ptr<Snapshot>> {
  std::shared_lock lock(_snapshotsMap.mutex);
  if (auto it = _snapshotsMap.snapshots.find(id);
      it != _snapshotsMap.snapshots.end()) {
    return ResultT<std::weak_ptr<Snapshot>>::success(it->second);
  }
  return ResultT<std::weak_ptr<Snapshot>>::error(
      TRI_ERROR_INTERNAL, fmt::format("Snapshot {} not found", id));
}

auto DocumentStateSnapshotHandler::abort(SnapshotId const& id) -> Result {
  std::unique_lock lock(_snapshotsMap.mutex);
  if (auto it = _snapshotsMap.snapshots.find(id);
      it != _snapshotsMap.snapshots.end()) {
    auto res = it->second->abort();
    _snapshotsMap.snapshots.erase(it);
    return res;
  }
  return Result{TRI_ERROR_INTERNAL, fmt::format("Snapshot {} not found", id)};
}

auto DocumentStateSnapshotHandler::finish(SnapshotId const& id) -> Result {
  std::unique_lock lock(_snapshotsMap.mutex);
  if (auto it = _snapshotsMap.snapshots.find(id);
      it != _snapshotsMap.snapshots.end()) {
    auto res = it->second->finish();
    _snapshotsMap.snapshots.erase(it);
    return res;
  }
  return Result{TRI_ERROR_INTERNAL, fmt::format("Snapshot {} not found", id)};
}

auto DocumentStateSnapshotHandler::status() const -> AllSnapshotsStatus {
  AllSnapshotsStatus result;
  std::shared_lock lock(_snapshotsMap.mutex);
  for (auto const& it : _snapshotsMap.snapshots) {
    result.snapshots.emplace(it.first, it.second->status());
  }
  return result;
}

void DocumentStateSnapshotHandler::clear() {
  std::unique_lock lock(_snapshotsMap.mutex);
  _snapshotsMap.snapshots.clear();
}
}  // namespace arangodb::replication2::replicated_state::document
