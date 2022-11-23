////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
#include "Inspection/Status.h"
#include "Inspection/Transformers.h"
#include "Inspection/VPack.h"
#include "Cluster/ClusterTypes.h"
#include "Replication2/ReplicatedLog/LogCommon.h"

#include <memory>
#include <optional>
#include <variant>

namespace arangodb::replication2::replicated_state::document {
struct ICollectionReader;

/*
 * Unique ID used to identify a snapshot between the leader and the follower.
 */
class SnapshotId : public arangodb::basics::Identifier {
 public:
  using arangodb::basics::Identifier::Identifier;

  static auto fromString(std::string_view) noexcept
      -> std::optional<SnapshotId>;

  static SnapshotId create();

  [[nodiscard]] explicit operator velocypack::Value() const noexcept;
};

template<class Inspector>
auto inspect(Inspector& f, SnapshotId& x) {
  return f.apply(static_cast<basics::Identifier&>(x));
}

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
inline constexpr auto kStringSnapshotId = std::string_view{"snapshotId"};
inline constexpr auto kStringShardId = std::string_view{"shardId"};
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
    LogIndex waitForIndex;
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
  ShardID shardId;
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

struct SnapshotStateTransformer {
  using SerializedType = std::string;
  auto toSerialized(SnapshotState const& source, SerializedType& target) const
      -> inspection::Status {
    target = std::string{
        std::visit(overload{
                       [](state::Ongoing const&) { return kStringOngoing; },
                       [](state::Finished const&) { return kStringFinished; },
                       [](state::Aborted const&) { return kStringAborted; },
                   },
                   source)};
    return {};
  }
};

/*
 * Used to retrieve information about the current state of a snapshot.
 */
struct SnapshotStatus {
  SnapshotState const& state;
  ShardID shardId{};
  std::optional<uint64_t> totalDocs{std::nullopt};
  uint64_t docsSent{0};
  std::size_t batchesSent{0};
  std::size_t bytesSent{0};
  std::chrono::system_clock::time_point startTime{
      std::chrono::system_clock::now()};
  std::chrono::system_clock::time_point lastUpdated{startTime};
  std::optional<std::chrono::system_clock::time_point> lastBatchSent{
      std::nullopt};

  template<class Inspector>
  inline friend auto inspect(Inspector& f, SnapshotStatus& s) {
    return f.object(s).fields(
        f.field(kStringState, s.state)
            .transformWith(SnapshotStateTransformer{}),
        f.field(kStringShardId, s.shardId),
        f.field(kStringTotalDocsToBeSent, s.totalDocs),
        f.field(kStringDocsSent, s.docsSent),
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

/*
 * Used when we want to retrieve information about all snapshots taken by a
 * leader.
 */
struct AllSnapshotsStatus {
  std::unordered_map<SnapshotId, SnapshotStatus> snapshots;

  template<class Inspector>
  inline friend auto inspect(Inspector& f, AllSnapshotsStatus& s) {
    return f.object(s).fields(f.field(kStringSnapshots, s.snapshots));
  }
};

/*
 * This is what the leader uses to keep track of current snapshots.
 */
class Snapshot {
 public:
  static inline constexpr std::size_t kBatchSizeLimit{16 * 1024 *
                                                      1024};  // 16MB

  explicit Snapshot(SnapshotId id, ShardID shardId,
                    std::unique_ptr<ICollectionReader> reader);

  Snapshot(Snapshot const&) = delete;
  Snapshot(Snapshot&&) = delete;
  Snapshot const& operator=(Snapshot const&) = delete;
  Snapshot const& operator=(Snapshot&&) = delete;

  auto fetch() -> ResultT<SnapshotBatch>;
  auto finish() -> Result;
  auto abort() -> Result;
  [[nodiscard]] auto status() const -> SnapshotStatus;

 private:
  SnapshotId _id;
  std::unique_ptr<ICollectionReader> _reader;
  SnapshotState _state;
  SnapshotStatus _status;
};
}  // namespace arangodb::replication2::replicated_state::document
