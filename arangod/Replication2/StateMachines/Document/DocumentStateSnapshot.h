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
#include "Replication2/StateMachines/Document/ShardProperties.h"

#include <memory>
#include <optional>
#include <variant>
#include <mutex>

namespace arangodb::replication2::replicated_state::document {
struct ICollectionReader;
struct IDatabaseSnapshot;

/*
 * Unique ID used to identify a snapshot between the leader and the follower.
 */
class SnapshotId : public arangodb::basics::Identifier {
 public:
  using arangodb::basics::Identifier::Identifier;

  static auto fromString(std::string_view) noexcept -> ResultT<SnapshotId>;

  static SnapshotId create();

  [[nodiscard]] explicit operator velocypack::Value() const noexcept;
};

auto to_string(SnapshotId snapshotId) -> std::string;

// This is serialized as a string because large 64-bit integers may not be
// represented correctly in JS.
template<class Inspector>
auto inspect(Inspector& f, SnapshotId& x) {
  if constexpr (Inspector::isLoading) {
    auto v = std::string{};
    auto res = f.apply(v);
    if (res.ok()) {
      if (auto r = SnapshotId::fromString(v); r.fail()) {
        return inspection::Status{std::string{r.result().errorMessage()}};
      } else {
        x = r.get();
      }
    }
    return res;
  } else {
    return f.apply(to_string(x));
  }
}

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
inline constexpr auto kStringSnapshotId = std::string_view{"snapshotId"};
inline constexpr auto kStringShardId = std::string_view{"shardId"};
inline constexpr auto kStringShards = std::string_view{"shards"};
inline constexpr auto kStringHasMore = std::string_view{"hasMore"};
inline constexpr auto kStringPayload = std::string_view{"payload"};
inline constexpr auto kStringState = std::string_view{"state"};
inline constexpr auto kStringTotalDocsToBeSent =
    std::string_view{"totalDocsToBeSent"};
inline constexpr auto kStringDocsSent = std::string_view{"docsSent"};
inline constexpr auto kStringTotalBatches = std::string_view{"totalBatches"};
inline constexpr auto kStringTotalBytes = std::string_view{"totalBytes"};
inline constexpr auto kStringStartTime = std::string_view{"startTime"};
inline constexpr auto kStringLastUpdated = std::string_view{"lastUpdated"};
inline constexpr auto kStringLastBatchSent = std::string_view{"lastBatchSent"};
inline constexpr auto kStringSnapshots = std::string_view{"snapshots"};
inline constexpr auto kStringOngoing = std::string_view{"ongoing"};
inline constexpr auto kStringAborted = std::string_view{"aborted"};
inline constexpr auto kStringFinished = std::string_view{"finished"};

/*
 * Indicates what type of action is expected from the leader.
 */
struct SnapshotParams {
  // Initiate a new snapshot.
  struct Start {
    ServerID serverId{};
    RebootId rebootId{0};

    template<class Inspector>
    inline friend auto inspect(Inspector& f, Start& s) {
      return f.object(s).fields(f.field(StaticStrings::ServerId, s.serverId),
                                f.field(StaticStrings::RebootId, s.rebootId));
    }
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
 * This is what the follower receives as response to a snapshot request.
 */
struct SnapshotBatch {
  SnapshotId snapshotId;
  // optional since we always have to send at least one batch, even though we
  // might not have any shards
  std::optional<ShardID> shardId;
  bool hasMore{false};
  velocypack::SharedSlice payload{};

  template<class Inspector>
  inline friend auto inspect(Inspector& f, SnapshotBatch& s) {
    return f.object(s).fields(f.field(kStringSnapshotId, s.snapshotId),
                              f.field(kStringShardId, s.shardId),
                              f.field(kStringHasMore, s.hasMore),
                              f.field(kStringPayload, s.payload));
  }
};

struct SnapshotConfig {
  SnapshotId snapshotId;
  ShardMap shards;

  template<class Inspector>
  inline friend auto inspect(Inspector& f, SnapshotConfig& s) {
    return f.object(s).fields(f.field(kStringSnapshotId, s.snapshotId),
                              f.field(kStringShards, s.shards));
  }
};

std::ostream& operator<<(std::ostream& os, SnapshotConfig const& config);

/*
 * This namespace encloses the different states a snapshot can be in.
 */
namespace state {
// Actively ongoing snapshot, although it may have had run out of documents.
struct Ongoing {
  VPackBuilder builder;
};

// Snapshot that has been aborted due to inactivity.
struct Aborted {};

// Snapshot that has been explicitly marked as finished by the follower.
struct Finished {};
}  // namespace state

using SnapshotState =
    std::variant<state::Ongoing, state::Aborted, state::Finished>;

/*
 * Used to retrieve debug information about a snapshot.
 */
struct SnapshotStatistics {
  struct ShardStatistics {
    std::optional<uint64_t> totalDocs{std::nullopt};
    uint64_t docsSent{0};

    template<class Inspector>
    inline friend auto inspect(Inspector& f, ShardStatistics& s) {
      return f.object(s).fields(f.field(kStringTotalDocsToBeSent, s.totalDocs),
                                f.field(kStringDocsSent, s.docsSent));
    }
  };

  std::unordered_map<ShardID, ShardStatistics> shards;
  std::size_t batchesSent{0};
  std::size_t bytesSent{0};
  std::chrono::system_clock::time_point startTime{
      std::chrono::system_clock::now()};
  std::chrono::system_clock::time_point lastUpdated{startTime};
  std::optional<std::chrono::system_clock::time_point> lastBatchSent{
      std::nullopt};

  template<class Inspector>
  inline friend auto inspect(Inspector& f, SnapshotStatistics& s) {
    return f.object(s).fields(
        f.field(kStringShards, s.shards),
        f.field(kStringTotalBatches, s.batchesSent),
        f.field(kStringTotalBytes, s.bytesSent),
        f.field(kStringStartTime, s.startTime)
            .transformWith(inspection::TimeStampTransformer{}),
        f.field(kStringLastUpdated, s.lastUpdated)
            .transformWith(inspection::TimeStampTransformer{}),
        f.field(kStringLastBatchSent, s.lastBatchSent)
            .transformWith(inspection::TimeStampTransformer{}));
  }
};

struct SnapshotStatus {
  SnapshotStatus(SnapshotState const& state, SnapshotStatistics statistics)
      : statistics(std::move(statistics)) {
    this->state =
        std::visit(overload{
                       [](state::Ongoing const&) { return kStringOngoing; },
                       [](state::Finished const&) { return kStringFinished; },
                       [](state::Aborted const&) { return kStringAborted; },
                   },
                   state);
  }

  std::string state;
  SnapshotStatistics statistics;

  template<class Inspector>
  inline friend auto inspect(Inspector& f, SnapshotStatus& s) {
    return f.object(s).fields(f.field(kStringState, s.state),
                              f.embedFields(s.statistics));
  }
};

/*
 * Used when we want to retrieve information about all snapshots taken by a
 * leader.
 */
struct AllSnapshotsStatus {
  std::unordered_map<SnapshotId, SnapshotStatus> snapshots;

  struct SnapshotMapTransformer {
    using MemoryType = std::unordered_map<SnapshotId, SnapshotStatus>;
    using SerializedType = std::unordered_map<std::string, SnapshotStatus>;

    static arangodb::inspection::Status toSerialized(MemoryType const& v,
                                                     SerializedType& result) {
      for (auto const& [key, value] : v) {
        result.emplace(to_string(key), value);
      }

      return {};
    }
  };

  template<class Inspector>
  inline friend auto inspect(Inspector& f, AllSnapshotsStatus& s) {
    return f.object(s).fields(f.field(kStringSnapshots, s.snapshots)
                                  .transformWith(SnapshotMapTransformer{}));
  }
};

/*
 * This is what the leader uses to keep track of current snapshots.
 */
class Snapshot {
 public:
  static inline constexpr std::size_t kBatchSizeLimit{16 * 1024 *
                                                      1024};  // 16MB

  explicit Snapshot(SnapshotId id, ShardMap shardsConfig,
                    std::unique_ptr<IDatabaseSnapshot> databaseSnapshot);
  Snapshot(Snapshot const&) = delete;
  Snapshot(Snapshot&&) = delete;
  Snapshot const& operator=(Snapshot const&) = delete;
  Snapshot const& operator=(Snapshot&&) = delete;

  auto config() -> SnapshotConfig;
  auto fetch() -> ResultT<SnapshotBatch>;
  auto finish() -> Result;
  auto abort() -> Result;
  [[nodiscard]] auto status() const -> SnapshotStatus;
  auto getId() const -> SnapshotId;
  auto giveUpOnShard(ShardID const& shardId) -> Result;
  auto isInactive() const -> bool;

  LoggerContext logContext;

 private:
  struct GuardedData {
    explicit GuardedData(std::unique_ptr<IDatabaseSnapshot> databaseSnapshot,
                         ShardMap const& shardsConfig);

    std::unique_ptr<IDatabaseSnapshot> databaseSnapshot;
    SnapshotState state;
    SnapshotStatistics statistics;
    std::vector<std::pair<ShardID, std::unique_ptr<ICollectionReader>>> shards;
  };

 private:
  const SnapshotConfig _config;
  Guarded<GuardedData> _guardedData;
};
}  // namespace arangodb::replication2::replicated_state::document
