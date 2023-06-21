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

namespace arangodb::replication2::replicated_log {
struct TermIndexMapping;
}
namespace arangodb::replication2::algorithms {

enum class ConflictReason {
  LOG_ENTRY_AFTER_END,
  LOG_ENTRY_BEFORE_BEGIN,
  LOG_EMPTY,
  LOG_ENTRY_NO_MATCH,
};

auto to_string(ConflictReason r) noexcept -> std::string_view;

auto detectConflict(replicated_log::TermIndexMapping const& log,
                    TermIndexPair prevLog) noexcept
    -> std::optional<std::pair<ConflictReason, TermIndexPair>>;

struct ParticipantState {
  TermIndexPair lastAckedEntry;
  ParticipantId id;
  bool snapshotAvailable = false;
  ParticipantFlags flags{};

  [[nodiscard]] auto isAllowedInQuorum() const noexcept -> bool;
  [[nodiscard]] auto isForced() const noexcept -> bool;
  [[nodiscard]] auto isSnapshotAvailable() const noexcept -> bool;

  [[nodiscard]] auto lastTerm() const noexcept -> LogTerm;
  [[nodiscard]] auto lastIndex() const noexcept -> LogIndex;

  friend auto operator<=>(ParticipantState const&,
                          ParticipantState const&) noexcept;
  friend auto operator<<(std::ostream& os, ParticipantState const& p) noexcept
      -> std::ostream&;
};

auto operator<=>(ParticipantState const& left,
                 ParticipantState const& right) noexcept;
auto operator<<(std::ostream& os, ParticipantState const& p) noexcept
    -> std::ostream&;

auto calculateCommitIndex(std::vector<ParticipantState> const& participants,
                          size_t effectiveWriteConcern,
                          LogIndex currentCommitIndex,
                          TermIndexPair lastTermIndex)
    -> std::tuple<LogIndex, replicated_log::CommitFailReason,
                  std::vector<ParticipantId>>;

}  // namespace arangodb::replication2::algorithms
