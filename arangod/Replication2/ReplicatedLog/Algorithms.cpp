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

namespace {
// For (unordered) maps left and right, return keys(left) \ keys(right)
auto keySetDifference = [](auto const& left, auto const& right) {
  using left_t = std::decay_t<decltype(left)>;
  using right_t = std::decay_t<decltype(right)>;
  static_assert(
      std::is_same_v<typename left_t::key_type, typename right_t::key_type>);
  using key_t = typename left_t::key_type;

  auto result = std::vector<key_t>{};
  for (auto const& [key, val] : left) {
    if (!right.contains(key)) {
      result.emplace_back(key);
    }
  }

  return result;
};
}  // namespace

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
      auto const status = log->getParticipant()->getStatus();
      auto* const leaderStatus = status.asLeaderStatus();
      // Note that newParticipants contains the leader, while oldFollowers does
      // not.
      auto const& oldFollowers = leaderStatus->follower;
      auto const& newParticipants = spec->participantsConfig.participants;
      // TODO move this calculation into updateParticipantsConfig()
      auto const additionalParticipantIds =
          keySetDifference(newParticipants, oldFollowers);
      auto const obsoleteParticipantIds =
          keySetDifference(oldFollowers, newParticipants);

      auto additionalParticipants =
          std::unordered_map<ParticipantId,
                             std::shared_ptr<AbstractFollower>>{};
      for (auto const& participantId : additionalParticipantIds) {
        if (participantId != myServerId) {
          additionalParticipants.try_emplace(
              participantId,
              ctx.buildAbstractFollowerImpl(logId, participantId));
        }
      }

      auto const& previousConfig = leaderStatus->activeParticipantsConfig;
      auto index = leader->updateParticipantsConfig(
          std::make_shared<ParticipantsConfig const>(spec->participantsConfig),
          previousConfig.generation, std::move(additionalParticipants),
          obsoleteParticipantIds);
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
                            ParticipantStateTuple const& p) noexcept
    -> std::ostream& {
  os << '{' << p.id << ':' << p.lastAckedEntry << ", ";
  os << "failed = " << std::boolalpha << p.failed;
  os << ", flags = " << p.flags;
  os << '}';
  return os;
}

auto ParticipantStateTuple::isExcluded() const noexcept -> bool {
  return flags.excluded;
};

auto ParticipantStateTuple::isForced() const noexcept -> bool {
  return flags.forced;
};

auto ParticipantStateTuple::isFailed() const noexcept -> bool {
  return failed;
};

auto ParticipantStateTuple::lastTerm() const noexcept -> LogTerm {
  return lastAckedEntry.term;
}

auto ParticipantStateTuple::lastIndex() const noexcept -> LogIndex {
  return lastAckedEntry.index;
}

auto operator<=>(ParticipantStateTuple const& left,
                 ParticipantStateTuple const& right) noexcept {
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
    std::vector<ParticipantStateTuple> const& indexes,
    CalculateCommitIndexOptions const opt, LogIndex currentCommitIndex,
    TermIndexPair lastTermIndex)
    -> std::tuple<LogIndex, CommitFailReason, std::vector<ParticipantId>> {
  // We keep a vector of participants that are neither excluded nor have failed.
  // All eligible participants have to be in the same term as the leader.
  // Never commit log entries from older terms.
  auto eligible = std::vector<ParticipantStateTuple>{};
  eligible.reserve(indexes.size());
  std::copy_if(std::begin(indexes), std::end(indexes),
               std::back_inserter(eligible), [&](auto& p) {
                 return !p.isFailed() && !p.isExcluded() &&
                        p.lastTerm() == lastTermIndex.term;
               });

  // We write to at least writeConcern servers, ideally more if available.
  auto actualWriteConcern = std::max(
      opt._writeConcern, std::min(eligible.size(), opt._softWriteConcern));

  if (actualWriteConcern > indexes.size() &&
      indexes.size() == eligible.size()) {
    // With WC greater than the number of participants, we cannot commit
    // anything, even if all participants are eligible.
    TRI_ASSERT(!indexes.empty());
    auto quorum = std::vector<ParticipantId>{};
    quorum.reserve(eligible.size());
    std::transform(std::begin(eligible), std::end(eligible),
                   std::back_inserter(quorum), [](auto& p) { return p.id; });
    auto const& who = std::min_element(
        std::begin(eligible), std::end(eligible), [](auto& left, auto& right) {
          return left.lastIndex() < right.lastIndex();
        });
    return {currentCommitIndex,
            CommitFailReason::withQuorumSizeNotReached(who->id),
            std::move(quorum)};
  }

  auto const spearhead = lastTermIndex.index;

  // The minimal commit index caused by forced participants.
  // If there are no forced participants, this component is just
  // the spearhead (the furthest we could commit to).
  auto minForcedCommitIndex = spearhead;
  auto minForcedParticipantId = std::optional<ParticipantId>{};
  for (auto const& pt : indexes) {
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

  // While actualWriteConcern == 0 is silly we still allow it.
  if (actualWriteConcern == 0) {
    return {minForcedCommitIndex, CommitFailReason::withNothingToCommit(), {}};
  }

  if (actualWriteConcern <= eligible.size()) {
    auto nth = std::begin(eligible);

    TRI_ASSERT(actualWriteConcern > 0);
    std::advance(nth, actualWriteConcern - 1);

    std::nth_element(std::begin(eligible), nth, std::end(eligible),
                     [](auto& left, auto& right) {
                       return left.lastIndex() > right.lastIndex();
                     });
    auto const minNonExcludedCommitIndex = nth->lastIndex();

    auto commitIndex =
        std::min(minForcedCommitIndex, minNonExcludedCommitIndex);

    auto quorum = std::vector<ParticipantId>{};
    quorum.reserve(actualWriteConcern);
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
      // We commit as far away as we can get, but report the participant whose
      // id is the furthest away from the spearhead.
      auto const& who = nth->id;
      return {commitIndex, CommitFailReason::withQuorumSizeNotReached(who),
              std::move(quorum)};
    }
  }

  // This happens when too many servers are either excluded or failed;
  // this certainly means we could not reach a quorum;
  // indexes cannot be empty because this particular case would've been handled
  // above by comparing actualWriteConcern to 0;
  CommitFailReason::NonEligibleServerRequiredForQuorum::CandidateMap candidates;
  for (auto const& p : indexes) {
    if (p.isFailed()) {
      candidates.emplace(
          p.id, CommitFailReason::NonEligibleServerRequiredForQuorum::kFailed);
    } else if (p.isExcluded()) {
      candidates.emplace(
          p.id,
          CommitFailReason::NonEligibleServerRequiredForQuorum::kExcluded);
    } else if (p.lastTerm() != lastTermIndex.term) {
      candidates.emplace(
          p.id,
          CommitFailReason::NonEligibleServerRequiredForQuorum::kWrongTerm);
    }
  }

  TRI_ASSERT(!indexes.empty());
  return {currentCommitIndex,
          CommitFailReason::withNonEligibleServerRequiredForQuorum(
              std::move(candidates)),
          {}};
}
