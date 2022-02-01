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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <variant>
#include <unordered_map>
#include <set>

#include "Cluster/ClusterTypes.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/ReplicatedLog/ReplicatedLog.h"
#include "Replication2/ReplicatedLog/AgencyLogSpecification.h"

namespace arangodb::replication2::algorithms {

struct ParticipantRecord {
  RebootId rebootId;
  bool isHealthy;

  ParticipantRecord(RebootId rebootId, bool isHealthy)
      : rebootId(rebootId), isHealthy(isHealthy) {}
};

using ParticipantInfo = std::unordered_map<ParticipantId, ParticipantRecord>;

enum class ConflictReason {
  LOG_ENTRY_AFTER_END,
  LOG_ENTRY_BEFORE_BEGIN,
  LOG_EMPTY,
  LOG_ENTRY_NO_MATCH,
};

auto to_string(ConflictReason r) noexcept -> std::string_view;

auto detectConflict(replicated_log::InMemoryLog const& log,
                    TermIndexPair prevLog) noexcept
    -> std::optional<std::pair<ConflictReason, TermIndexPair>>;

struct LogActionContext {
  virtual ~LogActionContext() = default;
  virtual auto dropReplicatedLog(LogId) -> Result = 0;
  virtual auto ensureReplicatedLog(LogId)
      -> std::shared_ptr<replicated_log::ReplicatedLog> = 0;
  virtual auto buildAbstractFollowerImpl(LogId, ParticipantId)
      -> std::shared_ptr<replication2::replicated_log::AbstractFollower> = 0;
};

auto updateReplicatedLog(LogActionContext& ctx, ServerID const& myServerId,
                         RebootId myRebootId, LogId logId,
                         agency::LogPlanSpecification const* spec) noexcept
    -> futures::Future<arangodb::Result>;

struct ParticipantStateTuple {
  TermIndexPair lastAckedEntry;
  ParticipantId id;
  bool failed = false;
  ParticipantFlags flags{};

  [[nodiscard]] auto isExcluded() const noexcept -> bool;
  [[nodiscard]] auto isForced() const noexcept -> bool;
  [[nodiscard]] auto isFailed() const noexcept -> bool;

  [[nodiscard]] auto lastTerm() const noexcept -> LogTerm;
  [[nodiscard]] auto lastIndex() const noexcept -> LogIndex;

  friend auto operator<=>(ParticipantStateTuple const&,
                          ParticipantStateTuple const&) noexcept;
  friend auto operator<<(std::ostream& os,
                         ParticipantStateTuple const& p) noexcept
      -> std::ostream&;
};

auto operator<=>(ParticipantStateTuple const& left,
                 ParticipantStateTuple const& right) noexcept;
auto operator<<(std::ostream& os, ParticipantStateTuple const& p) noexcept
    -> std::ostream&;

struct CalculateCommitIndexOptions {
  std::size_t const _writeConcern{0};
  std::size_t const _softWriteConcern{0};

  CalculateCommitIndexOptions(std::size_t writeConcern,
                              std::size_t softWriteConcern);
};

auto calculateCommitIndex(std::vector<ParticipantStateTuple> const& indexes,
                          CalculateCommitIndexOptions opt,
                          LogIndex currentCommitIndex,
                          TermIndexPair lastTermIndex)
    -> std::tuple<LogIndex, replicated_log::CommitFailReason,
                  std::vector<ParticipantId>>;

}  // namespace arangodb::replication2::algorithms
