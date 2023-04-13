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

#include "Assertions/ProdAssert.h"
#include "Replication2/StateMachines/Document/CollectionReader.h"
#include "Replication2/StateMachines/Document/DocumentStateMachine.h"
#include "Logger/LogContextKeys.h"
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

std::ostream& operator<<(std::ostream& os, SnapshotConfig const& config) {
  return os << velocypack::serialize(config).toJson();
}

Snapshot::GuardedData::GuardedData(std::unique_ptr<IDatabaseSnapshot> dbs,
                                   ShardMap const& shardsConfig)
    : databaseSnapshot(std::move(dbs)), state{state::Ongoing{}} {
  for (auto const& [shardId, properties] : shardsConfig) {
    auto reader = databaseSnapshot->createCollectionReader(shardId);
    statistics.shards.emplace(
        shardId, SnapshotStatistics::ShardStatistics{reader->getDocCount()});
    shards.emplace_back(shardId, std::move(reader));
  }
}

Snapshot::Snapshot(SnapshotId id, ShardMap shardsConfig,
                   std::unique_ptr<IDatabaseSnapshot> databaseSnapshot)
    : logContext(LoggerContext(Logger::REPLICATED_STATE)
                     .with<logContextKeyStateImpl>(DocumentState::NAME)
                     .with<logContextKeySnapshotId>(id)),
      _config{id, ShardMap{std::move(shardsConfig)}},
      _guardedData(std::move(databaseSnapshot), _config.shards) {
  LOG_CTX("d6c7f", DEBUG, logContext)
      << "Created snapshot with config " << _config;
}

auto Snapshot::config() -> SnapshotConfig { return _config; }

auto Snapshot::fetch() -> ResultT<SnapshotBatch> {
  return _guardedData.doUnderLock([&](auto& data) {
    return std::visit(
        overload{
            [&](state::Ongoing& ongoing) -> ResultT<SnapshotBatch> {
              LOG_CTX("f9226", DEBUG, logContext) << "Reading next batch";

              ongoing.builder.clear();
              if (data.shards.empty()) {
                auto batch = SnapshotBatch{.snapshotId = getId(),
                                           .shardId = std::nullopt,
                                           .hasMore = false};
                LOG_CTX("ca1cb", DEBUG, logContext)
                    << "No more shards to read from. Returning empty batch.";
                return ResultT<SnapshotBatch>::success(std::move(batch));
              }

              TRI_IF_FAILURE("DocumentStateSnapshot::infiniteSnapshot") {
                // Keep the snapshot alive by returning empty batches.
                VPackBuilder fakePayload;
                { VPackArrayBuilder{&fakePayload}; }

                auto batch =
                    SnapshotBatch{.snapshotId = getId(),
                                  .shardId = data.shards.back().first,
                                  .hasMore = true,
                                  .payload = fakePayload.sharedSlice()};

                // Sleep here, so the follower doesn't go into a busy loop.
                std::this_thread::sleep_for(std::chrono::seconds(1));
                return ResultT<SnapshotBatch>::success(std::move(batch));
              }

              auto& shard = data.shards.back();
              auto& reader = *shard.second;
              reader.read(ongoing.builder, kBatchSizeLimit);
              auto readerHasMore = reader.hasMore();
              auto batch = SnapshotBatch{
                  .snapshotId = getId(),
                  .shardId = shard.first,
                  .hasMore = readerHasMore || data.shards.size() > 1,
                  .payload = ongoing.builder.sharedSlice()};
              ++data.statistics.batchesSent;
              data.statistics.bytesSent += batch.payload.byteSize();
              data.statistics.shards[shard.first].docsSent +=
                  batch.payload.length();
              data.statistics.lastBatchSent = data.statistics.lastUpdated =
                  std::chrono::system_clock::now();

              LOG_CTX("9d1b4", DEBUG, logContext)
                  << "Read " << batch.payload.length() << " documents from "
                  << batch.shardId.value() << " in batch "
                  << data.statistics.batchesSent << " with "
                  << batch.payload.byteSize() << " bytes. There is "
                  << (readerHasMore ? "more" : "no more")
                  << " data to read from this shard.";

              if (!readerHasMore) {
                data.shards.pop_back();
                LOG_CTX("c00b1", DEBUG, logContext)
                    << "Reading from shard " << batch.shardId.value()
                    << " completed. " << data.shards.size() << " shards to go."
                    << (data.shards.empty()
                            ? ""
                            : " Next shard is " + data.shards.back().first);
              }
              return ResultT<SnapshotBatch>::success(std::move(batch));
            },
            [&](state::Finished const&) -> ResultT<SnapshotBatch> {
              LOG_CTX("fe02b", WARN, logContext)
                  << "Trying to fetch data from a finished snapshot!";
              return ResultT<SnapshotBatch>::error(
                  TRI_ERROR_INTERNAL,
                  fmt::format("Snapshot {} was finished!", getId()));
            },
            [&](state::Aborted const&) -> ResultT<SnapshotBatch> {
              LOG_CTX("d4253", WARN, logContext)
                  << "Trying to fetch data from an aborted snapshot!";
              return ResultT<SnapshotBatch>::error(
                  TRI_ERROR_INTERNAL,
                  fmt::format("Snapshot {} was aborted!", getId()));
            },
        },
        data.state);
  });
}

auto Snapshot::finish() -> Result {
  return _guardedData.doUnderLock([&](auto& data) {
    return std::visit(
        overload{
            [&](state::Ongoing& ongoing) -> Result {
              if (!data.shards.empty()) {
                LOG_CTX("23913", INFO, logContext)
                    << "Snapshot is being finished, but still has more data!";
              }
              data.state = state::Finished{};
              LOG_CTX("9e190", DEBUG, logContext) << "Snapshot finished!";
              return {};
            },
            [&](state::Finished const&) -> Result {
              LOG_CTX("16d04", WARN, logContext)
                  << "Trying to finish snapshot, but it appears to be already "
                     "finished!";
              return {};
            },
            [&](state::Aborted const&) -> Result {
              LOG_CTX("83e35", WARN, logContext)
                  << "Trying to finish snapshot, but it appears to be aborted!";
              return Result{TRI_ERROR_INTERNAL,
                            fmt::format("Snapshot {} was aborted!", getId())};
            },
        },
        data.state);
  });
}

auto Snapshot::abort() -> Result {
  return _guardedData.doUnderLock([&](auto& data) {
    return std::visit(
        overload{
            [&](state::Ongoing& ongoing) -> Result {
              if (!data.shards.empty()) {
                LOG_CTX("5ce86", INFO, logContext)
                    << "Snapshot is being aborted, but still has more data!";
              }
              data.state = state::Aborted{};
              LOG_CTX("a862c", DEBUG, logContext) << "Snapshot aborted!";
              return {};
            },
            [&](state::Finished const&) -> Result {
              LOG_CTX("81ea0", WARN, logContext)
                  << "Trying to abort snapshot, but it appears to be finished!";
              return Result{TRI_ERROR_INTERNAL,
                            fmt::format("Snapshot {} was finished!", getId())};
            },
            [&](state::Aborted const&) -> Result {
              LOG_CTX("4daf1", WARN, logContext)
                  << "Trying to abort snapshot, but it appears to be already "
                     "aborted!";
              return {};
            },
        },
        data.state);
  });
}

[[nodiscard]] auto Snapshot::status() const -> SnapshotStatus {
  return _guardedData.doUnderLock([&](auto& data) -> SnapshotStatus {
    return {data.state, data.statistics};
  });
}

auto Snapshot::getId() const -> SnapshotId { return _config.snapshotId; }

auto Snapshot::giveUpOnShard(ShardID const& shardId) -> Result {
  return _guardedData.doUnderLock([&](auto& data) -> Result {
    if (auto res = data.databaseSnapshot->resetTransaction(); res.fail()) {
      LOG_CTX("38d54", WARN, logContext)
          << "Failed to reset transaction: " << res.errorMessage();
      return res;
    }

    data.shards.erase(std::remove_if(data.shards.begin(), data.shards.end(),
                                     [&shardId](auto const& shard) {
                                       return shard.first == shardId;
                                     }),
                      data.shards.end());

    for (auto& it : data.shards) {
      auto reader = data.databaseSnapshot->createCollectionReader(it.first);
      it.second = std::move(reader);
    }

    LOG_CTX("89271", DEBUG, logContext) << "Gave up on shard: " << shardId;
    return {};
  });
}

auto Snapshot::isInactive() const -> bool {
  return _guardedData.doUnderLock([&](auto& data) {
    return std::holds_alternative<state::Finished>(data.state) ||
           std::holds_alternative<state::Aborted>(data.state);
  });
}

}  // namespace arangodb::replication2::replicated_state::document
