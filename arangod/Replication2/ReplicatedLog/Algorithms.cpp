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

#include "Algorithms.h"

#include "Basics/Exceptions.h"
#include "Basics/StringUtils.h"
#include "Basics/application-exit.h"
#include "Logger/LogMacros.h"
#include "Random/RandomGenerator.h"
#include "Cluster/FailureOracle.h"

#include <algorithm>
#include <type_traits>
#include <random>
#include <tuple>

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

auto algorithms::detectConflict(replicated_log::InMemoryLog const& log,
                                TermIndexPair prevLog) noexcept
    -> std::optional<std::pair<ConflictReason, TermIndexPair>> {
  /*
   * There are three situations to handle here:
   *  - We don't have that log entry
   *    - It is behind our last entry
   *    - It is before our first entry
   *  - The term does not match.
   */
  auto entry = log.getEntryByIndex(prevLog.index);
  if (entry.has_value()) {
    // check if the term matches
    if (entry->entry().logTerm() != prevLog.term) {
      auto conflict = std::invoke([&] {
        if (auto idx = log.getFirstIndexOfTerm(entry->entry().logTerm());
            idx.has_value()) {
          return TermIndexPair{entry->entry().logTerm(), *idx};
        }
        return TermIndexPair{};
      });

      return std::make_pair(ConflictReason::LOG_ENTRY_NO_MATCH, conflict);
    } else {
      // No conflict
      return std::nullopt;
    }
  } else {
    auto lastEntry = log.getLastEntry();
    if (!lastEntry.has_value()) {
      // The log is empty, reset to (0, 0)
      return std::make_pair(ConflictReason::LOG_EMPTY, TermIndexPair{});
    } else if (prevLog.index > lastEntry->entry().logIndex()) {
      // the given entry is too far ahead
      return std::make_pair(ConflictReason::LOG_ENTRY_AFTER_END,
                            TermIndexPair{lastEntry->entry().logTerm(),
                                          lastEntry->entry().logIndex() + 1});
    } else {
      TRI_ASSERT(prevLog.index < lastEntry->entry().logIndex());
      TRI_ASSERT(prevLog.index < log.getFirstEntry()->entry().logIndex());
      // the given index too old, reset to (0, 0)
      return std::make_pair(ConflictReason::LOG_ENTRY_BEFORE_BEGIN,
                            TermIndexPair{});
    }
  }
}

auto algorithms::updateReplicatedLog(
    LogActionContext& ctx, ServerID const& myServerId, RebootId myRebootId,
    LogId logId, agency::LogPlanSpecification const* spec,
    std::shared_ptr<cluster::IFailureOracle const> failureOracle) noexcept
    -> futures::Future<arangodb::Result> {
  auto result = basics::catchToResultT([&]() -> futures::Future<
                                                 arangodb::Result> {
    if (spec == nullptr) {
      return ctx.dropReplicatedLog(logId);
    }

    TRI_ASSERT(logId == spec->id);
    TRI_ASSERT(spec->currentTerm.has_value());
    auto& plannedLeader = spec->currentTerm->leader;
    auto log = ctx.ensureReplicatedLog(logId);

    if (log->getParticipant()->getTerm() == spec->currentTerm->term) {
      // something has changed in the term volatile configuration
      auto leader = log->getLeader();
      TRI_ASSERT(leader != nullptr);
      // Provide the leader with a way to build a follower
      auto const buildFollower = [&ctx,
                                  &logId](ParticipantId const& participantId) {
        return ctx.buildAbstractFollowerImpl(logId, participantId);
      };
      auto index = leader->updateParticipantsConfig(
          std::make_shared<ParticipantsConfig const>(spec->participantsConfig),
          buildFollower);
      return leader->waitFor(index).thenValue(
          [](auto&& quorum) -> Result { return Result{TRI_ERROR_NO_ERROR}; });
    } else if (plannedLeader.has_value() &&
               plannedLeader->serverId == myServerId &&
               plannedLeader->rebootId == myRebootId) {
      auto followers = std::vector<
          std::shared_ptr<replication2::replicated_log::AbstractFollower>>{};
      for (auto const& [participant, data] :
           spec->participantsConfig.participants) {
        if (participant != myServerId) {
          followers.emplace_back(
              ctx.buildAbstractFollowerImpl(logId, participant));
        }
      }

      TRI_ASSERT(spec->participantsConfig.generation > 0);
      auto newLeader = log->becomeLeader(
          spec->currentTerm->config, myServerId, spec->currentTerm->term,
          followers,
          std::make_shared<ParticipantsConfig>(spec->participantsConfig),
          std::move(failureOracle));
      newLeader->triggerAsyncReplication();  // TODO move this call into
                                             // becomeLeader?
      return newLeader->waitForLeadership().thenValue(
          [](auto&& quorum) -> Result { return Result{TRI_ERROR_NO_ERROR}; });
    } else {
      auto leaderString = std::optional<ParticipantId>{};
      if (spec->currentTerm->leader) {
        leaderString = spec->currentTerm->leader->serverId;
      }

      std::ignore = log->becomeFollower(myServerId, spec->currentTerm->term,
                                        leaderString);
    }

    return futures::Future<arangodb::Result>{std::in_place};
  });

  if (result.ok()) {
    return *std::move(result);
  } else {
    return futures::Future<arangodb::Result>{std::in_place, result.result()};
  }
}

auto algorithms::operator<<(std::ostream& os,
                            ParticipantState const& p) noexcept
    -> std::ostream& {
  os << '{' << p.id << ':' << p.lastAckedEntry << ", ";
  os << "failed = " << std::boolalpha << p.failed;
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

auto ParticipantState::isFailed() const noexcept -> bool { return failed; };

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

algorithms::CalculateCommitIndexOptions::CalculateCommitIndexOptions(
    std::size_t writeConcern, std::size_t softWriteConcern)
    : _writeConcern(writeConcern), _softWriteConcern(softWriteConcern) {}

auto algorithms::calculateCommitIndex(
    std::vector<ParticipantState> const& participants,
    CalculateCommitIndexOptions const opt, LogIndex const currentCommitIndex,
    TermIndexPair const lastTermIndex)
    -> std::tuple<LogIndex, CommitFailReason, std::vector<ParticipantId>> {
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
                 return p.isAllowedInQuorum() &&
                        p.lastTerm() == lastTermIndex.term;
               });

  // If servers are unavailable because they are either failed or excluded,
  // the actualWriteConcern may be lowered from softWriteConcern down to at
  // least writeConcern.
  auto const numAvailableParticipants = std::size_t(std::count_if(
      std::begin(participants), std::end(participants),
      [](auto const& p) { return !p.isFailed() && p.isAllowedInQuorum(); }));
  // We write to at least writeConcern servers, ideally more if available.
  auto const effectiveWriteConcern =
      std::max(opt._writeConcern,
               std::min(numAvailableParticipants, opt._softWriteConcern));

  if (effectiveWriteConcern > participants.size() &&
      participants.size() == eligible.size()) {
    // With WC greater than the number of participants, we cannot commit
    // anything, even if all participants are eligible.
    TRI_ASSERT(!participants.empty());
    TRI_ASSERT(opt._writeConcern == effectiveWriteConcern);
    return {currentCommitIndex,
            CommitFailReason::withFewerParticipantsThanWriteConcern({
                .writeConcern = opt._writeConcern,
                .softWriteConcern = opt._softWriteConcern,
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
  auto minForcedParticipantId = std::optional<ParticipantId>{};
  for (auto const& pt : participants) {
    if (pt.isForced()) {
      if (pt.lastTerm() != lastTermIndex.term) {
        // A forced participant has entries from a previous term. We can't use
        // this participant, hence it becomes impossible to commit anything.
        return {currentCommitIndex,
                CommitFailReason::withForcedParticipantNotInQuorum(pt.id),
                {}};
      }
      if (pt.lastIndex() < minForcedCommitIndex) {
        minForcedCommitIndex = pt.lastIndex();
        minForcedParticipantId = pt.id;
      }
    }
  }

  // While effectiveWriteConcern == 0 is silly we still allow it.
  if (effectiveWriteConcern == 0) {
    return {minForcedCommitIndex, CommitFailReason::withNothingToCommit(), {}};
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

    if (spearhead == commitIndex) {
      // The quorum has been reached and any uncommitted entries can now be
      // committed.
      return {commitIndex, CommitFailReason::withNothingToCommit(),
              std::move(quorum)};
    } else if (minForcedCommitIndex < minNonExcludedCommitIndex) {
      // The forced participant didn't make the quorum because its index
      // is too low. Return its index, but report that it is dragging down the
      // commit index.
      TRI_ASSERT(minForcedParticipantId.has_value());
      return {commitIndex,
              CommitFailReason::withForcedParticipantNotInQuorum(
                  minForcedParticipantId.value()),
              {}};
    } else {
      // We commit as far away as we can get, but report all participants who
      // can't be part of a quorum for the spearhead.
      auto who = CommitFailReason::QuorumSizeNotReached::who_type();
      for (auto const& participant : participants) {
        if (participant.lastAckedEntry < lastTermIndex ||
            !participant.isAllowedInQuorum()) {
          who.try_emplace(
              participant.id,
              CommitFailReason::QuorumSizeNotReached::ParticipantInfo{
                  .isFailed = participant.isFailed(),
                  .isAllowedInQuorum = participant.isAllowedInQuorum(),
                  .lastAcknowledged = participant.lastAckedEntry,
              });
        }
      }
      return {commitIndex,
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
                             kNotAllowedInQuorum);
    } else if (p.lastTerm() != lastTermIndex.term) {
      candidates.emplace(
          p.id,
          CommitFailReason::NonEligibleServerRequiredForQuorum::kWrongTerm);
    }
  }

  TRI_ASSERT(!participants.empty());
  return {currentCommitIndex,
          CommitFailReason::withNonEligibleServerRequiredForQuorum(
              std::move(candidates)),
          {}};
}
