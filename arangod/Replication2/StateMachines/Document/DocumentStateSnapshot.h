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
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Rest/GeneralRequest.h"
#include "Utils/SingleCollectionTransaction.h"
#include "StorageEngine/ReplicationIterator.h"

#include <velocypack/SharedSlice.h>

#include <memory>
#include <optional>
#include <variant>

namespace arangodb {
class LogicalCollection;

namespace transaction {
class Context;
}
}  // namespace arangodb

namespace arangodb::replication2::replicated_state::document {
class SnapshotId : public arangodb::basics::Identifier {
 public:
  using arangodb::basics::Identifier::Identifier;

  explicit SnapshotId() : Identifier(TRI_HybridLogicalClock()){};

  static auto fromString(std::string_view) noexcept
      -> std::optional<SnapshotId>;

  [[nodiscard]] explicit operator velocypack::Value() const noexcept;
};

template<class Inspector>
auto inspect(Inspector& f, SnapshotId& x) {
  if constexpr (Inspector::isLoading) {
    auto v = uint64_t{0};
    auto res = f.apply(v);
    if (res.ok()) {
      x = SnapshotId{v};
    }
    return res;
  } else {
    return f.apply(x.id());
  }
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
namespace {
inline constexpr auto kStringSnapshotId = std::string_view{"snapshotId"};
inline constexpr auto kStringShard = std::string_view{"shard"};
inline constexpr auto kStringHasMore = std::string_view{"hasMore"};
inline constexpr auto kStringPayload = std::string_view{"payload"};
inline constexpr auto kStringState = std::string_view{"state"};
inline constexpr auto kStringTotalBatches = std::string_view{"totalBatches"};
inline constexpr auto kStringTotalBytes = std::string_view{"totalBytes"};
inline constexpr auto kStringStartTime = std::string_view{"startTime"};
inline constexpr auto kStringLastUpdated = std::string_view{"lastUpdated"};
inline constexpr auto kStringSnapshots = std::string_view{"snapshots"};
inline constexpr auto kStringOngoing = std::string_view{"ongoing"};
inline constexpr auto kStringAborted = std::string_view{"aborted"};
inline constexpr auto kStringFinished = std::string_view{"finished"};
}  // namespace

/*
 * Indicates what type of action is expected from the leader.
 */
struct SnapshotParams {
  // Initiate a new snapshot.
  struct Start {
    LogIndex waitForIndex;
    SnapshotId id{};
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
  ShardID shard;
  bool hasMore{false};
  velocypack::SharedSlice payload{};

  template<class Inspector>
  inline friend auto inspect(Inspector& f, SnapshotBatch& s) {
    return f.object(s).fields(f.field(kStringSnapshotId, s.snapshotId),
                              f.field(kStringShard, s.shard),
                              f.field(kStringHasMore, s.hasMore),
                              f.field(kStringPayload, s.payload));
  }
};

namespace snapshot {
// Actively ongoing snapshot, although it may have had run out of documents.
class Ongoing {
 public:
  explicit Ongoing(SnapshotId id,
                   std::shared_ptr<LogicalCollection> logicalCollection);
  auto next() -> SnapshotBatch;
  [[nodiscard]] bool hasMore() const;
  [[nodiscard]] auto getTotalDocs() const -> std::optional<uint64_t>;

 private:
  static inline constexpr std::size_t kBatchSizeLimit{1024 * 1024};  // 1MB

  SnapshotId _id;
  std::shared_ptr<LogicalCollection> _logicalCollection;
  std::shared_ptr<transaction::Context> _ctx;
  std::unique_ptr<SingleCollectionTransaction> _trx;
  std::unique_ptr<ReplicationIterator> _it;
  std::optional<uint64_t> _totalDocs{std::nullopt};
};

// Snapshot that has been aborted due to inactivity.
class Aborted {
  ShardID _shard;
};

// Snapshot that has been explicitly marked as finished by the follower.
class Finished {
  ShardID _shard;
};

using State = std::variant<Ongoing, Aborted, Finished>;

struct SnapshotStateTransformer {
  using SerializedType = std::string;
  auto toSerialized(State const& source, SerializedType& target) const
      -> inspection::Status {
    target = std::string{std::visit(
        overload{
            [](snapshot::Ongoing const&) { return kStringOngoing; },
            [](snapshot::Finished const&) { return kStringFinished; },
            [](snapshot::Aborted const&) { return kStringAborted; },
        },
        source)};
    return {};
  }
};
}  // namespace snapshot

struct SnapshotStatus {
  snapshot::State const& state;
  std::optional<uint64_t> totalDocs;
  uint64_t docsSent{0};
  std::size_t batchesSent{0};
  std::size_t bytesSent{0};
  std::chrono::system_clock::time_point startTime{
      std::chrono::system_clock::now()};
  std::chrono::system_clock::time_point lastUpdated{startTime};
  std::optional<ShardID> shard{std::nullopt};

  explicit SnapshotStatus(snapshot::State const& state) : state(state) {
    totalDocs = std::visit(
        [](auto const& arg) -> decltype(totalDocs) {
          using T = std::decay_t<decltype(arg)>;
          if constexpr (std::is_same_v<T, snapshot::Ongoing>) {
            return static_cast<snapshot::Ongoing const&>(arg).getTotalDocs();
          } else {
            return std::nullopt;
          }
        },
        state);
  }

  template<class Inspector>
  inline friend auto inspect(Inspector& f, SnapshotStatus& s) {
    return f.object(s).fields(
        f.field(kStringState, s.state)
            .transformWith(snapshot::SnapshotStateTransformer{}),
        f.field(kStringTotalBatches, s.batchesSent),
        f.field(kStringTotalBytes, s.bytesSent),
        f.field(kStringStartTime, s.startTime)
            .transformWith(inspection::TimeStampTransformer{}),
        f.field(kStringLastUpdated, s.lastUpdated)
            .transformWith(inspection::TimeStampTransformer{}),
        f.field(kStringShard, s.shard));
  }
};

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
  explicit Snapshot(SnapshotId id,
                    std::shared_ptr<LogicalCollection> logicalCollection);

  Snapshot(Snapshot const&) = delete;
  Snapshot const& operator=(Snapshot const&) = delete;

  auto fetch() -> ResultT<SnapshotBatch>;
  auto finish() -> Result;
  auto abort() -> Result;
  auto status() -> SnapshotStatus;

 private:
  SnapshotId _id;
  snapshot::State _state;
  SnapshotStatus _status;
};
}  // namespace arangodb::replication2::replicated_state::document
