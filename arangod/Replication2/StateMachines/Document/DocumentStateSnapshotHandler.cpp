////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022-2022 ArangoDB GmbH, Cologne, Germany
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

#include "Replication2/StateMachines/Document/CollectionReader.h"

namespace arangodb::replication2::replicated_state::document {
DocumentStateSnapshotHandler::DocumentStateSnapshotHandler(
    std::unique_ptr<ICollectionReaderFactory> collectionReaderFactory)
    : _collectionReaderFactory(std::move(collectionReaderFactory)) {}

auto DocumentStateSnapshotHandler::create(std::string_view const& shardId)
    -> ResultT<Snapshot*> {
  auto readerRes = _collectionReaderFactory->createCollectionReader(shardId);
  if (readerRes.fail()) {
    return ResultT<Snapshot*>::error(readerRes.result());
  }

  auto reader = std::move(readerRes.get());
  auto id = SnapshotId::create();
  auto emplacement = _snapshots.emplace(
      id, Snapshot{id, std::string{shardId}, std::move(reader)});
  TRI_ASSERT(emplacement.second);

  return &emplacement.first->second;
}

auto DocumentStateSnapshotHandler::find(SnapshotId const& id)
    -> ResultT<Snapshot*> {
  auto it = _snapshots.find(id);
  if (it == _snapshots.end()) {
    return ResultT<Snapshot*>::error(TRI_ERROR_INTERNAL,
                                     fmt::format("Snapshot {} not found", id));
  }
  return ResultT<Snapshot*>::success(&it->second);
}

auto DocumentStateSnapshotHandler::status() const -> AllSnapshotsStatus {
  AllSnapshotsStatus result;
  for (auto const& it : _snapshots) {
    result.snapshots.emplace(it.first, it.second.status());
  }
  return result;
}

void DocumentStateSnapshotHandler::clear() { _snapshots.clear(); }

void DocumentStateSnapshotHandler::checkSnapshots() {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}
}  // namespace arangodb::replication2::replicated_state::document
