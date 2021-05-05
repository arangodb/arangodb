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

#if (_MSC_VER >= 1)
// suppress warnings:
#pragma warning(push)
// conversion from 'size_t' to 'immer::detail::rbts::count_t', possible loss of data
#pragma warning(disable : 4267)
// result of 32-bit shift implicitly converted to 64 bits (was 64-bit shift intended?)
#pragma warning(disable : 4334)
#endif
#include <immer/map.hpp>
#include <immer/flex_vector.hpp>
#if (_MSC_VER >= 1)
#pragma warning(pop)
#endif

#include <Basics/Guarded.h>
#include <Basics/voc-errors.h>
#include <Futures/Future.h>
#include <velocypack/Builder.h>
#include <velocypack/SharedSlice.h>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>
#include "Replication2/ReplicatedLog/Common.h"
#include "Replication2/ReplicatedLog/InMemoryLog.h"
#include "Replication2/ReplicatedLog/LogCore.h"
#include "Replication2/ReplicatedLog/LogParticipantI.h"
#include "Replication2/ReplicatedLog/PersistedLog.h"

namespace arangodb::replication2::replicated_log {

enum class AppendEntriesErrorReason {
  NONE,
  INVALID_LEADER_ID,
  LOST_LOG_CORE,
  WRONG_TERM,
  NO_PREV_LOG_MATCH
};

struct AppendEntriesResult {
  LogTerm const logTerm = LogTerm{};
  ErrorCode const errorCode = TRI_ERROR_NO_ERROR;
  AppendEntriesErrorReason const reason = AppendEntriesErrorReason::NONE;

  [[nodiscard]] auto isSuccess() const noexcept -> bool {
    return errorCode == TRI_ERROR_NO_ERROR;
  }

  explicit AppendEntriesResult(LogTerm);
  AppendEntriesResult(LogTerm logTerm, ErrorCode errorCode, AppendEntriesErrorReason reason);
  void toVelocyPack(velocypack::Builder& builder) const;
  static auto fromVelocyPack(velocypack::Slice slice) -> AppendEntriesResult;
};

std::string to_string(AppendEntriesErrorReason reason);

struct AppendEntriesRequest {
  LogTerm leaderTerm;
  ParticipantId leaderId;
  // TODO assert index == 0 <=> term == 0
  LogTerm prevLogTerm;
  LogIndex prevLogIndex;
  LogIndex leaderCommit;
  immer::flex_vector<LogEntry> entries{};

  void toVelocyPack(velocypack::Builder& builder) const;
  static auto fromVelocyPack(velocypack::Slice slice) -> AppendEntriesRequest;
};

struct LogStatistics {
  LogIndex spearHead{};
  LogIndex commitIndex{};

  void toVelocyPack(velocypack::Builder& builder) const;
};

struct LeaderStatus {
  struct FollowerStatistics : LogStatistics {
    AppendEntriesErrorReason lastErrorReason;
    void toVelocyPack(velocypack::Builder& builder) const;
  };

  LogStatistics local;
  LogTerm term;
  std::unordered_map<ParticipantId, FollowerStatistics> follower;

  void toVelocyPack(velocypack::Builder& builder) const;
};

struct FollowerStatus {
  LogStatistics local;
  ParticipantId leader;
  LogTerm term;

  void toVelocyPack(velocypack::Builder& builder) const;
};

struct UnconfiguredStatus {
  void toVelocyPack(velocypack::Builder& builder) const;
};

using LogStatus = std::variant<UnconfiguredStatus, LeaderStatus, FollowerStatus>;

struct AbstractFollower {
  virtual ~AbstractFollower() = default;
  [[nodiscard]] virtual auto getParticipantId() const noexcept -> ParticipantId const& = 0;
  virtual auto appendEntries(AppendEntriesRequest)
      -> futures::Future<AppendEntriesResult> = 0;
};

struct QuorumData {
  QuorumData(const LogIndex& index, LogTerm term, std::vector<ParticipantId> quorum);

  LogIndex index;
  LogTerm term;
  std::vector<ParticipantId> quorum;
};

}  // namespace arangodb::replication2::replicated_log
