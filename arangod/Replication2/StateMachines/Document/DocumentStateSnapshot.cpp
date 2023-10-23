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
#include "Replication2/StateMachines/Document/DocumentStateSnapshotInspectors.h"

#include "Assertions/ProdAssert.h"
#include "Replication2/StateMachines/Document/CollectionReader.h"
#include "Replication2/StateMachines/Document/DocumentStateMachine.h"
#include "Logger/LogContextKeys.h"
#include "VocBase/LogicalCollection.h"
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

std::ostream& operator<<(std::ostream& os, SnapshotBatch const& batch) {
  return os << velocypack::serialize(batch).toJson();
}

SnapshotStatus::SnapshotStatus(SnapshotState const& state,
                               SnapshotStatistics statistics)
    : statistics(std::move(statistics)) {
  this->state =
      std::visit(overload{
                     [](state::Started const&) { return kStringStarted; },
                     [](state::Ongoing const&) { return kStringOngoing; },
                     [](state::Finished const&) { return kStringFinished; },
                     [](state::Aborted const&) { return kStringAborted; },
                 },
                 state);
}

Snapshot::GuardedData::GuardedData(
    std::unique_ptr<IDatabaseSnapshot> databaseSnapshot,
    std::vector<std::shared_ptr<LogicalCollection>> shards)
    : databaseSnapshot(std::move(databaseSnapshot)) {
  for (auto&& shard : shards) {
    statistics.shards.emplace(shard->name(),
                              SnapshotStatistics::ShardStatistics{});
    this->shards.emplace_back(std::move(shard), nullptr);
  }
}

Snapshot::Snapshot(SnapshotId id, GlobalLogIdentifier gid,
                   std::vector<std::shared_ptr<LogicalCollection>> shards,
                   std::unique_ptr<IDatabaseSnapshot> databaseSnapshot)
    : _id{id},
      _gid(std::move(gid)),
      _state(state::Started{}),
      _guardedData(std::move(databaseSnapshot), std::move(shards)),
      loggerContext(LoggerContext(Logger::REPLICATED_STATE)
                        .with<logContextKeyStateImpl>(DocumentState::NAME)
                        .with<logContextKeyDatabaseName>(_gid.database)
                        .with<logContextKeyLogId>(_gid.id)
                        .with<logContextKeySnapshotId>(getId())) {
  LOG_CTX("d6c7f", DEBUG, loggerContext) << "Created snapshot with id " << _id;
}

auto Snapshot::fetch() -> ResultT<SnapshotBatch> {
  return _state.doUnderLock([&](auto& state) -> ResultT<SnapshotBatch> {
    return std::visit(overload{//
                               [&](state::Started& arg) {
                                 auto res = generateBatch(arg);
                                 if (res.ok()) {
                                   state = state::Ongoing{};
                                 }
                                 return res;
                               },
                               [&](auto& arg) { return generateBatch(arg); }},
                      state);
  });
}

auto Snapshot::finish() -> Result {
  return _state.doUnderLock([&](auto& state) {
    return std::visit(
        overload{
            [&](state::Started&) -> Result {
              LOG_CTX("75dc8", DEBUG, loggerContext)
                  << "Snapshot finished, before being used!";
              return {};
            },
            [&](state::Ongoing&) -> Result {
              auto isEmpty = _guardedData.doUnderLock(
                  [&](auto& data) { return data.shards.empty(); });
              if (!isEmpty) {
                LOG_CTX("23913", WARN, loggerContext)
                    << "Snapshot is being finished, but still has more "
                       "data!";
              }
              state = state::Finished{};
              LOG_CTX("9e190", DEBUG, loggerContext) << "Snapshot finished!";
              return {};
            },
            [&](state::Finished const&) -> Result {
              LOG_CTX("16d04", INFO, loggerContext)
                  << "Trying to finish snapshot, but it appears to be already "
                     "finished!";
              return {};
            },
            [&](state::Aborted const&) -> Result {
              LOG_CTX("83e35", WARN, loggerContext)
                  << "Trying to finish snapshot, but it appears to be "
                     "aborted!";
              return Result{
                  TRI_ERROR_INTERNAL,
                  fmt::format("Snapshot {} is already aborted!", getId())};
            },
        },
        state);
  });
}

void Snapshot::abort() {
  _state.doUnderLock([&](auto& state) {
    std::visit(
        overload{
            [&](state::Started&) {
              LOG_CTX("95353", DEBUG, loggerContext)
                  << "Snapshot aborted, before being used!";
              state = state::Aborted{};
            },
            [&](state::Ongoing&) {
              auto isEmpty = _guardedData.doUnderLock(
                  [&](auto& data) { return data.shards.empty(); });
              if (!isEmpty) {
                LOG_CTX("5ce86", INFO, loggerContext)
                    << "Snapshot is being aborted, but still has more "
                       "data!";
              }
              state = state::Aborted{};
              LOG_CTX("a862c", DEBUG, loggerContext) << "Snapshot aborted!";
            },
            [&](state::Finished const&) {
              LOG_CTX("81ea0", INFO, loggerContext)
                  << "Trying to abort snapshot, but it appears to be already "
                     "finished!";
            },
            [&](state::Aborted const&) {
              LOG_CTX("4daf1", WARN, loggerContext)
                  << "Trying to abort snapshot, but it appears to be already "
                     "aborted!";
            },
        },
        state);
  });
}

[[nodiscard]] auto Snapshot::status() const -> SnapshotStatus {
  auto state = _state.doUnderLock([](auto& state) { return state; });
  auto statistics =
      _guardedData.doUnderLock([](auto& data) { return data.statistics; });
  return {state, std::move(statistics)};
}

auto Snapshot::getId() const -> SnapshotId { return _id; }

auto Snapshot::giveUpOnShard(ShardID const& shardId) -> Result {
  return _guardedData.doUnderLock([&](auto& data) -> Result {
    if (data.shards.empty()) {
      return {};
    }

    if (data.shards.back().first != nullptr &&
        data.shards.back().first->name() == shardId) {
      auto res = data.databaseSnapshot->resetTransaction();
      if (res.fail()) {
        LOG_CTX("38d54", ERR, loggerContext)
            << "Failed to reset snapshot transaction, this may prevent shard "
            << shardId << " from being dropped: " << res;
      }
      data.shards.pop_back();
      return res;
    }

    if (auto it = std::find_if(std::begin(data.shards), std::end(data.shards),
                               [&](auto const& shard) {
                                 return shard.first != nullptr &&
                                        shard.first->name() == shardId;
                               });
        it != std::end(data.shards)) {
      // Give up the shared pointer to the logical collection
      it->first = nullptr;
    }

    LOG_CTX("89271", DEBUG, loggerContext) << "Gave up on shard: " << shardId;
    return {};
  });
}

auto Snapshot::isInactive() const -> bool {
  return _state.doUnderLock([&](auto& state) {
    return std::holds_alternative<state::Finished>(state) ||
           std::holds_alternative<state::Aborted>(state);
  });
}

auto Snapshot::generateBatch(state::Started const&) -> ResultT<SnapshotBatch> {
  LOG_CTX("4e4d4", DEBUG, loggerContext) << "Reading first batch";

  TRI_IF_FAILURE("DocumentStateSnapshot::infiniteSnapshot") {
    // Sleep before returning, so the follower doesn't go into a
    // busy loop.
    std::this_thread::sleep_for(std::chrono::seconds(1));
    // Keep the snapshot alive by returning empty batches.
    return SnapshotBatch{.snapshotId = getId(), .hasMore = true};
  }

  return _guardedData.doUnderLock([&](auto& data) -> ResultT<SnapshotBatch> {
    if (data.shards.empty()) {
      return SnapshotBatch{.snapshotId = getId(), .hasMore = false};
    }

    std::vector<ReplicatedOperation> operations;
    operations.reserve(data.shards.size());

    for (auto const& [shard, _] : data.shards) {
      auto properties = VPackBuilder();
      shard->properties(properties,
                        LogicalDataSource::Serialization::Inventory);
      operations.emplace_back(ReplicatedOperation::buildCreateShardOperation(
          shard->name(), shard->type(), std::move(properties).sharedSlice()));
    }

    return SnapshotBatch{.snapshotId = getId(),
                         .hasMore = true,
                         .operations = std::move(operations)};
  });
}

auto Snapshot::generateBatch(state::Ongoing const&) -> ResultT<SnapshotBatch> {
  LOG_CTX("f9226", DEBUG, loggerContext) << "Reading next batch";

  TRI_IF_FAILURE("DocumentStateSnapshot::infiniteSnapshot") {
    // Sleep before returning, so the follower doesn't go into a
    // busy loop.
    std::this_thread::sleep_for(std::chrono::seconds(1));
    // Keep the snapshot alive by returning empty batches.
    return SnapshotBatch{.snapshotId = getId(), .hasMore = true};
  }

  return _guardedData.doUnderLock([&](auto& data) -> ResultT<SnapshotBatch> {
    while (!data.shards.empty()) {
      auto& currentShard = data.shards.back();
      auto& shard = currentShard.first;
      auto& reader = currentShard.second;

      // The collection might have been dropped in the meantime.
      if (shard == nullptr || shard->deleted()) {
        data.shards.pop_back();
        LOG_CTX("c9fba", DEBUG, loggerContext)
            << "Skipping dropped shard " << shard->name();
        continue;
      }

      if (reader == nullptr) {
        auto res = basics::catchToResultT([&]() {
          return data.databaseSnapshot->createCollectionReader(shard);
        });

        if (res.fail()) {
          if (res.is(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND)) {
            data.shards.pop_back();
            LOG_CTX("3862c", DEBUG, loggerContext)
                << "Skipping dropped shard " << shard->name();
            continue;
          }

          LOG_CTX("5532c", ERR, loggerContext)
              << "Encountered unexpected error while creating collection "
                 "reader for shard "
              << shard->name() << ": " << res.result();
          return res.result();
        }

        reader = std::move(res.get());
        data.statistics.shards.emplace(
            shard->name(),
            SnapshotStatistics::ShardStatistics{reader->getDocCount()});
      }

      break;
    }

    if (data.shards.empty()) {
      LOG_CTX("ca1cb", DEBUG, loggerContext)
          << "No more shards to read from. Returning empty "
             "batch.";
      return SnapshotBatch{.snapshotId = getId(), .hasMore = false};
    }

    auto& [shard, reader] = data.shards.back();
    VPackBuilder builder;
    reader->read(builder, kBatchSizeLimit);
    auto payload = std::move(builder).sharedSlice();

    auto readerHasMore = reader->hasMore();
    if (!readerHasMore) {
      data.shards.pop_back();
      LOG_CTX("c00b1", DEBUG, loggerContext)
          << "Reading from shard " << shard->name() << " completed. "
          << data.shards.size() << " shards to go."
          << (data.shards.empty()
                  ? ""
                  : " Next shard is " + data.shards.back().first->name());
    }

    ++data.statistics.batchesSent;
    data.statistics.bytesSent += payload.byteSize();
    data.statistics.lastBatchSent = data.statistics.lastUpdated =
        std::chrono::system_clock::now();

    LOG_CTX("9d1b4", DEBUG, loggerContext)
        << "Read " << payload.slice().length() << " documents from "
        << shard->name() << " in batch " << data.statistics.batchesSent
        << " with " << payload.byteSize() << " bytes. There is "
        << (readerHasMore ? "" : "no") << " more data to read from this shard.";

    return SnapshotBatch{
        .snapshotId = getId(),
        .hasMore = readerHasMore || data.shards.size() > 1,
        .operations = generateDocumentBatch(shard->name(), std::move(payload))};
  });
}

auto Snapshot::generateBatch(state::Finished const&) -> ResultT<SnapshotBatch> {
  LOG_CTX("fe02b", DEBUG, loggerContext)
      << "Trying to fetch data from a finished snapshot!";
  return ResultT<SnapshotBatch>::error(
      TRI_ERROR_INTERNAL,
      fmt::format("Snapshot {} is already finished!", getId()));
}

auto Snapshot::generateBatch(state::Aborted const&) -> ResultT<SnapshotBatch> {
  LOG_CTX("d4253", DEBUG, loggerContext)
      << "Trying to fetch data from an aborted snapshot!";
  return ResultT<SnapshotBatch>::error(
      TRI_ERROR_INTERNAL,
      fmt::format("Snapshot {} is already aborted!", getId()));
}

auto Snapshot::generateDocumentBatch(ShardID shardId,
                                     velocypack::SharedSlice slice)
    -> std::vector<ReplicatedOperation> {
  std::vector<ReplicatedOperation> batch;
  batch.reserve(2);
  auto tid = TransactionId::createFollower();
  batch.emplace_back(ReplicatedOperation::buildDocumentOperation(
      TRI_VOC_DOCUMENT_OPERATION_INSERT, tid, std::move(shardId),
      std::move(slice)));
  batch.emplace_back(ReplicatedOperation::buildCommitOperation(tid));
  return batch;
}
}  // namespace arangodb::replication2::replicated_state::document
