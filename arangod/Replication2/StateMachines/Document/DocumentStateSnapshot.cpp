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

#include "Replication2/StateMachines/Document/DocumentStateSnapshot.h"

#include "Replication2/StateMachines/Document/CollectionReader.h"
#include "VocBase/ticks.h"

namespace arangodb::replication2::replicated_state::document {
auto SnapshotId::fromString(std::string_view name) noexcept
    -> ResultT<SnapshotId> {
  auto id = basics::StringUtils::try_uint64(name);
  if (id.fail()) {
    return ResultT<SnapshotId>::error(id.result());
  }
  return ResultT<SnapshotId>::success(SnapshotId{id.get()});
}

[[nodiscard]] SnapshotId::operator velocypack::Value() const noexcept {
  return velocypack::Value(id());
}

SnapshotId SnapshotId::create() { return SnapshotId{TRI_HybridLogicalClock()}; }

auto to_string(SnapshotId snapshotId) -> std::string {
  return std::to_string(snapshotId.id());
}

Snapshot::Snapshot(SnapshotId id, ShardID shardId,
                   std::unique_ptr<ICollectionReader> reader)
    : _id(id),
      _reader{std::move(reader)},
      _state{state::Ongoing{}},
      _statistics{std::move(shardId), _reader->getDocCount()} {}

auto Snapshot::fetch() -> ResultT<SnapshotBatch> {
  return std::visit(
      overload{
          [&](state::Ongoing& ongoing) -> ResultT<SnapshotBatch> {
            ongoing.builder.clear();
            _reader->read(ongoing.builder, kBatchSizeLimit);
            auto batch =
                SnapshotBatch{.snapshotId = _id,
                              .shardId = _statistics.shardId,
                              .hasMore = _reader->hasMore(),
                              .payload = ongoing.builder.sharedSlice()};
            ++_statistics.batchesSent;
            _statistics.bytesSent += batch.payload.byteSize();
            _statistics.docsSent += batch.payload.length();
            _statistics.lastBatchSent = _statistics.lastUpdated =
                std::chrono::system_clock::now();
            return ResultT<SnapshotBatch>::success(std::move(batch));
          },
          [&](state::Finished const&) -> ResultT<SnapshotBatch> {
            return ResultT<SnapshotBatch>::error(
                TRI_ERROR_INTERNAL,
                fmt::format("Snapshot {} was finished!", _id));
          },
          [&](state::Aborted const&) -> ResultT<SnapshotBatch> {
            return ResultT<SnapshotBatch>::error(
                TRI_ERROR_INTERNAL,
                fmt::format("Snapshot {} was aborted!", _id));
          },
      },
      _state);
}

auto Snapshot::finish() -> Result {
  return std::visit(
      overload{
          [&](state::Ongoing& ongoing) -> Result {
            if (_reader->hasMore()) {
              LOG_TOPIC("23913", WARN, Logger::REPLICATION2)
                  << "Snapshot " << _id
                  << " is being finished, but still has more data!";
            }
            _state = state::Finished{};
            return {};
          },
          [&](state::Finished const&) -> Result {
            LOG_TOPIC("16d04", WARN, Logger::REPLICATION2)
                << "Snapshot " << _id << " appears to be already finished!";
            return {};
          },
          [&](state::Aborted const&) -> Result {
            return Result{TRI_ERROR_INTERNAL,
                          fmt::format("Snapshot {} was aborted!", _id)};
          },
      },
      _state);
}

auto Snapshot::abort() -> Result {
  return std::visit(
      overload{
          [&](state::Ongoing& ongoing) -> Result {
            if (_reader->hasMore()) {
              LOG_TOPIC("5ce86", WARN, Logger::REPLICATION2)
                  << "Snapshot " << _id
                  << " is being aborted, but still has more data!";
            }
            _state = state::Aborted{};
            return {};
          },
          [&](state::Finished const&) -> Result {
            return Result{TRI_ERROR_INTERNAL,
                          fmt::format("Snapshot {} was finished!", _id)};
          },
          [&](state::Aborted const&) -> Result {
            LOG_TOPIC("4daf1", WARN, Logger::REPLICATION2)
                << "Snapshot " << _id << " appears to be already aborted!";
            return {};
          },
      },
      _state);
}

[[nodiscard]] auto Snapshot::status() const -> SnapshotStatus {
  return SnapshotStatus(_state, _statistics);
}
}  // namespace arangodb::replication2::replicated_state::document
