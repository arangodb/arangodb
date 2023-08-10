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

#include "Algorithms.h"

#include "Basics/StringUtils.h"
#include "Basics/application-exit.h"
#include "Logger/LogMacros.h"
#include "Replication2/ReplicatedLog/TermIndexMapping.h"

#include <algorithm>
#include <random>
#include <tuple>
#include <type_traits>

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::agency;
using namespace arangodb::replication2::algorithms;
using namespace arangodb::replication2::replicated_log;

auto algorithms::to_string(ConflictReason r) noexcept -> std::string_view {
  switch (r) {
    case ConflictReason::LOG_ENTRY_AFTER_END:
      return "prev log is located after the last log entry";
    case ConflictReason::LOG_ENTRY_BEFORE_BEGIN:
      return "prev log is located before the first entry";
    case ConflictReason::LOG_EMPTY:
      return "the replicated log is empty";
    case ConflictReason::LOG_ENTRY_NO_MATCH:
      return "term mismatch";
  }
  LOG_TOPIC("03e11", FATAL, arangodb::Logger::REPLICATION2)
      << "Invalid ConflictReason "
      << static_cast<std::underlying_type_t<decltype(r)>>(r);
  FATAL_ERROR_ABORT();
}

auto algorithms::detectConflict(
    replicated_log::TermIndexMapping const& termIndexMap,
    TermIndexPair prevLog) noexcept
    -> std::optional<std::pair<ConflictReason, TermIndexPair>> {
  /*
   * There are three situations to handle here:
   *  - We don't have that log entry
   *    - It is behind our last entry
   *    - It is before our first entry
   *  - The term does not match.
   */
  auto entry = termIndexMap.getTermOfIndex(prevLog.index);
  if (entry.has_value()) {
    // check if the term matches
    if (*entry != prevLog.term) {
      auto conflict = std::invoke([&] {
        if (auto idx = termIndexMap.getFirstIndexOfTerm(*entry);
            idx.has_value()) {
          return TermIndexPair{*entry, *idx};
        }
        return TermIndexPair{};
      });

      return std::make_pair(ConflictReason::LOG_ENTRY_NO_MATCH, conflict);
    } else {
      // No conflict
      return std::nullopt;
    }
  } else {
    auto lastEntry = termIndexMap.getLastIndex();
    if (not lastEntry.has_value()) {
      // The log is empty, reset to (0, 0)
      return std::make_pair(ConflictReason::LOG_EMPTY, TermIndexPair{});
    } else if (prevLog.index > lastEntry->index) {
      // the given entry is too far ahead
      return std::make_pair(
          ConflictReason::LOG_ENTRY_AFTER_END,
          TermIndexPair{lastEntry->term, lastEntry->index + 1});
    } else {
      TRI_ASSERT(prevLog.index < lastEntry->index);
      TRI_ASSERT(prevLog.index < termIndexMap.getFirstIndex()->index);
      // the given index too old, reset to (0, 0)
      return std::make_pair(ConflictReason::LOG_ENTRY_BEFORE_BEGIN,
                            TermIndexPair{});
    }
  }
}

auto algorithms::operator<<(std::ostream& os,
                            ParticipantState const& p) noexcept
    -> std::ostream& {
  os << '{' << p.id << ':' << p.lastAckedEntry;
  os << ", snapshot = " << std::boolalpha << p.snapshotAvailable;
  os << ", flags = " << p.flags;
  os << '}';
  return os;
}

auto ParticipantState::isAllowedInQuorum() const noexcept -> bool {
  return flags.allowedInQuorum;
};

auto ParticipantState::isForced() const noexcept -> bool {
  return flags.forced;
};

auto ParticipantState::isSnapshotAvailable() const noexcept -> bool {
  return snapshotAvailable;
}

auto ParticipantState::lastTerm() const noexcept -> LogTerm {
  return lastAckedEntry.term;
}

auto ParticipantState::lastIndex() const noexcept -> LogIndex {
  return lastAckedEntry.index;
}

auto operator<=>(ParticipantState const& left,
                 ParticipantState const& right) noexcept {
  // return std::tie(left.index, left.id) <=> std::tie(right.index, right.id);
  // -- not supported by apple clang
  if (auto c = left.lastIndex() <=> right.lastIndex(); c != 0) {
    return c;
  }
  return left.id.compare(right.id) <=> 0;
}

auto algorithms::calculateCommitIndex(
    std::vector<ParticipantState> const& participants,
    size_t const effectiveWriteConcern, LogIndex const currentCommitIndex,
    TermIndexPair const lastTermIndex, LogIndex const currentSyncCommitIndex)
    -> CommitIndexReport {
  // We keep a vector of eligible participants.
  // To be eligible, a participant
  //  - must not be excluded, and
  //  - must be in the same term as the leader.
  // This is because we must not include an excluded server in any quorum, and
  // must never commit log entries from older terms.
  auto eligible = std::vector<ParticipantState>{};
  eligible.reserve(participants.size());
  std::copy_if(std::begin(participants), std::end(participants),
               std::back_inserter(eligible), [&](auto const& p) {
                 return p.isAllowedInQuorum() and p.isSnapshotAvailable() and
                        p.lastTerm() == lastTermIndex.term;
               });

  if (effectiveWriteConcern > participants.size() &&
      participants.size() == eligible.size()) {
    // With WC greater than the number of participants, we cannot commit
    // anything, even if all participants are eligible.
    TRI_ASSERT(!participants.empty());
    return {currentCommitIndex,
            currentSyncCommitIndex,
            CommitFailReason::withFewerParticipantsThanWriteConcern({
                .effectiveWriteConcern = effectiveWriteConcern,
                .numParticipants = participants.size(),
            }),
            {}};
  }

  auto const spearhead = lastTermIndex.index;

  // The minimal commit index caused by forced participants.
  // If there are no forced participants, this component is just
  // the spearhead (the furthest we could commit to).
  auto minForcedCommitIndex = spearhead;
  auto minForcedSyncCommitIndex = spearhead;
  auto minForcedParticipantId = std::optional<ParticipantId>{};
  for (auto const& pt : participants) {
    if (pt.isForced()) {
      if (pt.lastTerm() != lastTermIndex.term) {
        // A forced participant has entries from a previous term. We can't use
        // this participant, hence it becomes impossible to commit anything.
        return {currentCommitIndex,
                currentSyncCommitIndex,
                CommitFailReason::withForcedParticipantNotInQuorum(pt.id),
                {}};
      }
      if (pt.lastIndex() < minForcedCommitIndex) {
        minForcedCommitIndex = pt.lastIndex();
        minForcedParticipantId = pt.id;
      }
      minForcedSyncCommitIndex =
          std::min(minForcedSyncCommitIndex, pt.syncIndex);
    }
  }

  // While effectiveWriteConcern == 0 is silly we still allow it.
  if (effectiveWriteConcern == 0) {
    return {minForcedCommitIndex,
            minForcedSyncCommitIndex,
            CommitFailReason::withNothingToCommit(),
            {}};
  }

  if (effectiveWriteConcern <= eligible.size()) {
    auto nth = std::begin(eligible);

    TRI_ASSERT(effectiveWriteConcern > 0);
    std::advance(nth, effectiveWriteConcern - 1);

    std::nth_element(std::begin(eligible), nth, std::end(eligible),
                     [](auto& left, auto& right) {
                       return left.lastIndex() > right.lastIndex();
                     });
    auto const minNonExcludedCommitIndex = nth->lastIndex();

    auto commitIndex =
        std::min(minForcedCommitIndex, minNonExcludedCommitIndex);

    auto quorum = std::vector<ParticipantId>{};
    quorum.reserve(effectiveWriteConcern);

    std::transform(std::begin(eligible), std::next(nth),
                   std::back_inserter(quorum), [](auto& p) { return p.id; });

    // syncCommitIndex is only reported
    std::nth_element(std::begin(eligible), nth, std::end(eligible),
                     [](auto& left, auto& right) {
                       return left.syncIndex > right.syncIndex;
                     });
    auto const minNonExcludedSyncCommitIndex = nth->syncIndex;
    auto syncCommitIndex =
        std::min(minForcedSyncCommitIndex, minNonExcludedSyncCommitIndex);
    TRI_ASSERT(syncCommitIndex <= commitIndex);

    if (spearhead == commitIndex) {
      // The quorum has been reached and any uncommitted entries can now be
      // committed.
      return {commitIndex, syncCommitIndex,
              CommitFailReason::withNothingToCommit(), std::move(quorum)};
    } else if (minForcedCommitIndex < minNonExcludedCommitIndex) {
      // The forced participant didn't make the quorum because its index
      // is too low. Return its index, but report that it is dragging down the
      // commit index.
      TRI_ASSERT(minForcedParticipantId.has_value());
      return {commitIndex,
              syncCommitIndex,
              CommitFailReason::withForcedParticipantNotInQuorum(
                  minForcedParticipantId.value()),
              {}};
    } else {
      // We commit as far away as we can get, but report all participants who
      // can't be part of a quorum for the spearhead.
      auto who = CommitFailReason::QuorumSizeNotReached::who_type();
      for (auto const& participant : participants) {
        if (participant.lastAckedEntry < lastTermIndex ||
            !participant.isAllowedInQuorum() ||
            !participant.isSnapshotAvailable()) {
          who.try_emplace(
              participant.id,
              CommitFailReason::QuorumSizeNotReached::ParticipantInfo{
                  .isAllowedInQuorum = participant.isAllowedInQuorum(),
                  .snapshotAvailable = participant.isSnapshotAvailable(),
                  .lastAcknowledged = participant.lastAckedEntry,
              });
        }
      }
      return {commitIndex, syncCommitIndex,
              CommitFailReason::withQuorumSizeNotReached(std::move(who),
                                                         lastTermIndex),
              std::move(quorum)};
    }
  }

  // This happens when too many servers are either excluded or failed;
  // this certainly means we could not reach a quorum;
  // indexes cannot be empty because this particular case would've been handled
  // above by comparing actualWriteConcern to 0;
  CommitFailReason::NonEligibleServerRequiredForQuorum::CandidateMap candidates;
  for (auto const& p : participants) {
    if (!p.isAllowedInQuorum()) {
      candidates.emplace(p.id,
                         CommitFailReason::NonEligibleServerRequiredForQuorum::
                             Why::kNotAllowedInQuorum);
    } else if (p.lastTerm() != lastTermIndex.term) {
      candidates.emplace(p.id,
                         CommitFailReason::NonEligibleServerRequiredForQuorum::
                             Why::kWrongTerm);
    } else if (not p.isSnapshotAvailable()) {
      candidates.emplace(p.id,
                         CommitFailReason::NonEligibleServerRequiredForQuorum::
                             Why::kSnapshotMissing);
    }
  }

  TRI_ASSERT(!participants.empty());
  return {currentCommitIndex,
          currentSyncCommitIndex,
          CommitFailReason::withNonEligibleServerRequiredForQuorum(
              std::move(candidates)),
          {}};
}
