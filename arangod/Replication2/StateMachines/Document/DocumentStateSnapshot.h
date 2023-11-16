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

#pragma once

#include "Basics/Identifier.h"
#include "Basics/Guarded.h"
#include "Basics/StaticStrings.h"
#include "Inspection/Status.h"
#include "Inspection/Transformers.h"
#include "Inspection/VPack.h"
#include "Cluster/ClusterTypes.h"
#include "Replication2/LoggerContext.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/StateMachines/Document/ReplicatedOperation.h"

#include <memory>
#include <optional>
#include <variant>
#include <mutex>

namespace arangodb {
class LogicalCollection;
}

namespace arangodb::replication2::replicated_state::document {
struct ICollectionReader;
struct IDatabaseSnapshot;

/*
 * Unique ID used for identifying a snapshot between the leader and the
 * follower.
 */
class SnapshotId : public arangodb::basics::Identifier {
 public:
  using arangodb::basics::Identifier::Identifier;

  static auto fromString(std::string_view) noexcept -> ResultT<SnapshotId>;

  static SnapshotId create();

  [[nodiscard]] explicit operator velocypack::Value() const noexcept;

#ifdef _WIN32
  explicit SnapshotId(BaseType id) noexcept
      : arangodb::basics::Identifier(id) {}
#endif
};

auto to_string(SnapshotId snapshotId) -> std::string;

}  // namespace arangodb::replication2::replicated_state::document

template<>
struct fmt::formatter<
    arangodb::replication2::replicated_state::document::SnapshotId>
    : fmt::formatter<arangodb::basics::Identifier> {};

template<>
struct std::hash<
    arangodb::replication2::replicated_state::document::SnapshotId> {
  [[nodiscard]] auto operator()(
      arangodb::replication2::replicated_state::document::SnapshotId const& v)
      const noexcept -> std::size_t {
    return std::hash<arangodb::basics::Identifier>{}(v);
  }
};

namespace arangodb::replication2::replicated_state::document {
/*
 * Indicates what type of action is expected from the leader.
 */
struct SnapshotParams {
  // Initiate a new snapshot.
  struct Start {
    ServerID serverId{};
    RebootId rebootId{0};
  };

  // Fetch the next batch of an existing snapshot.
  struct Next {
    SnapshotId id{};
  };

  // Delete an existing snapshot.
  struct Finish {
    SnapshotId id{};
  };

  // Retrieve the current state of an existing snapshot.
  struct Status {
    std::optional<SnapshotId> id{std::nullopt};
  };

  using ParamsType = std::variant<Start, Next, Finish, Status>;
  ParamsType params;
};

/*
 * Additional traffic from leader to follower is done in batches.
 */
struct SnapshotBatch {
  SnapshotId snapshotId;
  bool hasMore{false};
  std::vector<ReplicatedOperation>
      operations;  // operations to be applied on the follower
};

std::ostream& operator<<(std::ostream& os, SnapshotBatch const& batch);

/*
 * Used to retrieve debug information about a snapshot.
 */
struct SnapshotStatistics {
  struct ShardStatistics {
    std::optional<uint64_t> totalDocs{std::nullopt};
    uint64_t docsSent{0};
  };

  std::unordered_map<ShardID, ShardStatistics> shards;
  std::size_t batchesSent{0};
  std::size_t bytesSent{0};
  std::chrono::system_clock::time_point startTime{
      std::chrono::system_clock::now()};
  std::chrono::system_clock::time_point lastUpdated{startTime};
  std::optional<std::chrono::system_clock::time_point> lastBatchSent{
      std::nullopt};
};

/*
 * This namespace encloses the different states a snapshot can be in.
 */
namespace state {
// Actively ongoing snapshot, although it may have had run out of documents.
struct Ongoing {};

// Snapshot that has been aborted due to inactivity.
struct Aborted {};

// Snapshot that has been explicitly marked as finished by the follower.
struct Finished {};
}  // namespace state

using SnapshotState =
    std::variant<state::Ongoing, state::Aborted, state::Finished>;

struct SnapshotStatus {
  SnapshotStatus(SnapshotState const& state, SnapshotStatistics statistics);
  std::string state;
  SnapshotStatistics statistics;
};

/*
 * Used when we want to retrieve information about all snapshots taken by a
 * leader.
 */
struct AllSnapshotsStatus {
  std::unordered_map<SnapshotId, SnapshotStatus> snapshots;
};

/*
 * This is what the leader uses to keep track of current snapshots.
 */
class Snapshot {
 public:
  static inline constexpr std::size_t kBatchSizeLimit{16 * 1024 *
                                                      1024};  // 16MB

  explicit Snapshot(SnapshotId id, GlobalLogIdentifier gid,
                    std::vector<std::shared_ptr<LogicalCollection>> shards,
                    std::unique_ptr<IDatabaseSnapshot> databaseSnapshot,
                    LoggerContext loggerContext);

  Snapshot(Snapshot const&) = delete;
  Snapshot(Snapshot&&) = delete;
  Snapshot const& operator=(Snapshot const&) = delete;
  Snapshot const& operator=(Snapshot&&) = delete;

  auto fetch() -> ResultT<SnapshotBatch>;
  auto finish() -> Result;
  void abort();

  [[nodiscard]] auto status() const -> SnapshotStatus;

  auto getId() const -> SnapshotId;

  // If a shard is dropped, we free up the resources associated with it.
  auto giveUpOnShard(ShardID const& shardId) -> Result;

  auto isInactive() const -> bool;

 private:
  static auto generateDocumentBatch(ShardID shardId,
                                    velocypack::SharedSlice slice)
      -> std::vector<ReplicatedOperation>;

  auto generateBatch(state::Ongoing const&) -> ResultT<SnapshotBatch>;
  auto generateBatch(state::Finished const&) -> ResultT<SnapshotBatch>;
  auto generateBatch(state::Aborted const&) -> ResultT<SnapshotBatch>;

 private:
  struct GuardedData {
    explicit GuardedData(
        std::unique_ptr<IDatabaseSnapshot> databaseSnapshot,
        std::vector<std::shared_ptr<LogicalCollection>> shards);

    std::unique_ptr<IDatabaseSnapshot> databaseSnapshot;
    SnapshotStatistics statistics;
    std::vector<std::pair<std::shared_ptr<LogicalCollection>,
                          std::unique_ptr<ICollectionReader>>>
        shards;
  };

  const SnapshotId _id;
  const GlobalLogIdentifier _gid;
  Guarded<SnapshotState> _state;
  Guarded<GuardedData> _guardedData;

 public:
  LoggerContext const loggerContext;
};
}  // namespace arangodb::replication2::replicated_state::document
