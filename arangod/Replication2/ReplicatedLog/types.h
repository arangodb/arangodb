////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Replication2/ReplicatedLog/Common.h"

#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace arangodb::futures {
template <typename T>
class Future;
}

namespace arangodb::velocypack {
class Builder;
class Slice;
}  // namespace arangodb::velocypack

namespace arangodb::replication2::replicated_log {

struct AppendEntriesRequest;
struct AppendEntriesResult;

enum class AppendEntriesErrorReason {
  NONE,
  INVALID_LEADER_ID,
  LOST_LOG_CORE,
  MESSAGE_OUTDATED,
  WRONG_TERM,
  NO_PREV_LOG_MATCH,
  PERSISTENCE_FAILURE,
  COMMUNICATION_ERROR,
};


struct LogStatistics {
  TermIndexPair spearHead{};
  LogIndex commitIndex{};

  void toVelocyPack(velocypack::Builder& builder) const;
  static auto fromVelocyPack(velocypack::Slice slice) -> LogStatistics;
};

struct LeaderStatus {
  struct FollowerStatistics : LogStatistics {
    AppendEntriesErrorReason lastErrorReason;
    double lastRequestLatencyMS;
    void toVelocyPack(velocypack::Builder& builder) const;
    static auto fromVelocyPack(velocypack::Slice slice) -> FollowerStatistics;
  };

  LogStatistics local;
  LogTerm term;
  std::unordered_map<ParticipantId, FollowerStatistics> follower;

  void toVelocyPack(velocypack::Builder& builder) const;
  static auto fromVelocyPack(velocypack::Slice slice) -> LeaderStatus;
};

struct FollowerStatus {
  LogStatistics local;
  std::optional<ParticipantId> leader;
  LogTerm term;

  void toVelocyPack(velocypack::Builder& builder) const;
  static auto fromVelocyPack(velocypack::Slice slice) -> FollowerStatus;
};

struct UnconfiguredStatus {
  void toVelocyPack(velocypack::Builder& builder) const;
  static auto fromVelocyPack(velocypack::Slice slice) -> UnconfiguredStatus;
};

using LogStatus = std::variant<UnconfiguredStatus, LeaderStatus, FollowerStatus>;

auto statusFromVelocyPack(VPackSlice slice) -> LogStatus;

auto getCurrentTerm(LogStatus const&) noexcept -> std::optional<LogTerm>;
auto getLocalStatistics(LogStatus const&) noexcept -> std::optional<LogStatistics>;

struct AbstractFollower {
  virtual ~AbstractFollower() = default;
  [[nodiscard]] virtual auto getParticipantId() const noexcept -> ParticipantId const& = 0;
  virtual auto appendEntries(AppendEntriesRequest)
      -> futures::Future<AppendEntriesResult> = 0;
};

struct QuorumData {
  QuorumData(const LogIndex& index, LogTerm term, std::vector<ParticipantId> quorum);
  explicit QuorumData(velocypack::Slice slice);

  LogIndex index;
  LogTerm term;
  std::vector<ParticipantId> quorum;

  void toVelocyPack(velocypack::Builder& builder) const;
};

}  // namespace arangodb::replication2::replicated_log
