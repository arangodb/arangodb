////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#include <type_traits>

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::agency;
using namespace arangodb::replication2::algorithms;

auto algorithms::checkReplicatedLog(DatabaseID const& database,
                                    LogPlanSpecification const& spec,
                                    LogCurrent const& current,
                                    std::unordered_map<ParticipantId, ParticipantRecord> const& info)
    -> std::variant<std::monostate, agency::LogPlanTermSpecification, agency::LogCurrentSupervisionElection> {
  auto const verifyServerRebootId = [&](ParticipantId const& id, RebootId rebootId) -> bool {
    if (auto it = info.find(id); it != std::end(info)) {
      return it->second.rebootId == rebootId;
    }

    return false;
  };

  auto const isServerHealthy = [&](ParticipantId const& id) -> bool {
    if (auto it = info.find(id); it != std::end(info)) {
      return it->second.isHealthy;
    }

    return false;
  };

  if (auto const& term = spec.currentTerm; term) {
    if (auto const& leader = term->leader; leader) {
      // check if leader is still valid
      if (false == verifyServerRebootId(leader->serverId, leader->rebootId)) {
        // create a new term with no leader
        LogPlanTermSpecification newTermSpec = *term;
        newTermSpec.leader = std::nullopt;
        newTermSpec.term.value += 1;
        LOG_TOPIC("bc357", WARN, Logger::REPLICATION2)
            << "replicated log " << database << "/" << spec.id
            << " - leader gone " << term->leader->serverId;
        return newTermSpec;
      }
    } else {
      // check if we can find a new leader
      // wait for enough servers to report the current term
      // a server is counted if:
      //    - its reported term is the current term
      //    - it is seen as healthy by the supervision

      // if enough servers are found, declare the server with
      // the "best" log as leader in a new term
      agency::LogCurrentSupervisionElection election;
      election.term = spec.currentTerm ? spec.currentTerm->term : LogTerm{0};

      auto newLeaderSet = std::vector<replication2::ParticipantId>{};
      auto bestTermIndex = replication2::TermIndexPair{};
      auto numberOfAvailableParticipants = std::size_t{0};

      for (auto const& [participant, status] : current.localState) {
        auto error = std::invoke([&, &status = status, &participant = participant]{
          bool const isHealthy = isServerHealthy(participant);
          if (!isHealthy) {
            return agency::LogCurrentSupervisionElection::ErrorCode::SERVER_NOT_GOOD;
          } else if (status.term != spec.currentTerm->term) {
            return agency::LogCurrentSupervisionElection::ErrorCode::TERM_NOT_CONFIRMED;
          } else {
            return agency::LogCurrentSupervisionElection::ErrorCode::OK;
          }
        });

        election.detail.emplace(participant, error);
        if (error != agency::LogCurrentSupervisionElection::ErrorCode::OK) {
          continue;
        }

        numberOfAvailableParticipants += 1;
        if (status.spearhead >= bestTermIndex) {
          if (status.spearhead != bestTermIndex) {
            newLeaderSet.clear();
          }
          newLeaderSet.push_back(participant);
          bestTermIndex = status.spearhead;
        }
      }

      auto const requiredNumberOfAvailableParticipants = std::invoke([&spec = spec.currentTerm] {
        return spec->participants.size() - spec->config.writeConcern + 1;
      });

      LOG_TOPIC("8a53d", TRACE, Logger::REPLICATION2)
          << "participant size = " << spec.currentTerm->participants.size()
          << " writeConcern = " << spec.currentTerm->config.writeConcern
          << " requiredNumberOfAvailableParticipants = " << requiredNumberOfAvailableParticipants;

      TRI_ASSERT(requiredNumberOfAvailableParticipants > 0);

      election.participantsRequired = requiredNumberOfAvailableParticipants;
      election.participantsAvailable = numberOfAvailableParticipants;

      if (numberOfAvailableParticipants >= requiredNumberOfAvailableParticipants) {
        auto const numParticipants = newLeaderSet.size();
        if (ADB_UNLIKELY(numParticipants == 0 ||
                         numParticipants > std::numeric_limits<uint16_t>::max())) {
          abortOrThrow(
              TRI_ERROR_NUMERIC_OVERFLOW,
              basics::StringUtils::concatT(
                  "Number of participants out of range, should be between ", 1,
                  " and ", std::numeric_limits<uint16_t>::max(), ", but is ", numParticipants),
              ADB_HERE);
        }
        auto const maxIdx = static_cast<uint16_t>(numParticipants - 1);
        // Randomly select one of the best participants
        auto const& newLeader = newLeaderSet.at(RandomGenerator::interval(maxIdx));
        auto const& record = info.at(newLeader);

        // we can elect a new leader
        LogPlanTermSpecification newTermSpec = *spec.currentTerm;
        newTermSpec.term.value += 1;
        newTermSpec.leader = LogPlanTermSpecification::Leader{newLeader, record.rebootId};
        LOG_TOPIC("458ad", INFO, Logger::REPLICATION2)
            << "declaring " << newLeader << " as new leader for log "
            << database << "/" << spec.id;
        return newTermSpec;

      } else {
        // Check if something has changed
        if (!current.supervision || !current.supervision->election ||
            election != current.supervision->election) {
          LOG_TOPIC("57de2", WARN, Logger::REPLICATION2)
              << "replicated log " << database << "/" << spec.id
              << " not enough participants available for leader election "
              << numberOfAvailableParticipants << "/"
              << requiredNumberOfAvailableParticipants;
          return election;
        }
      }
    }
  }

  return std::monostate{};
}

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

auto algorithms::detectConflict(replicated_log::InMemoryLog const& log, TermIndexPair prevLog) noexcept
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
        if (auto idx = log.getFirstIndexOfTerm(entry->entry().logTerm()); idx.has_value()) {
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
      return std::make_pair(ConflictReason::LOG_ENTRY_BEFORE_BEGIN, TermIndexPair{});
    }
  }
}

auto algorithms::updateReplicatedLog(LogActionContext& ctx, ServerID const& serverId,
                                     RebootId rebootId, LogId logId,
                                     agency::LogPlanSpecification const* spec) noexcept
    -> arangodb::Result {
  return basics::catchToResult([&]() -> Result {
    if (spec == nullptr) {
      return ctx.dropReplicatedLog(logId);
    }

    TRI_ASSERT(logId == spec->id);
    TRI_ASSERT(spec->currentTerm.has_value());
    auto& leader = spec->currentTerm->leader;
    auto log = ctx.ensureReplicatedLog(logId);

    if (leader.has_value() && leader->serverId == serverId && leader->rebootId == rebootId) {
      auto followers =
          std::vector<std::shared_ptr<replication2::replicated_log::AbstractFollower>>{};
      for (auto const& [participant, data] : spec->currentTerm->participants) {
        if (participant != serverId) {
          followers.emplace_back(ctx.buildAbstractFollowerImpl(logId, participant));
        }
      }

      auto newLeader = log->becomeLeader(spec->currentTerm->config, serverId,
                                         spec->currentTerm->term, followers);
      newLeader->triggerAsyncReplication(); // TODO move this call into becomeLeader?
    } else {
      auto leaderString = std::optional<ParticipantId>{};
      if (spec->currentTerm->leader) {
        leaderString = spec->currentTerm->leader->serverId;
      }

      std::ignore = log->becomeFollower(serverId, spec->currentTerm->term, leaderString);
    }

    return Result();
  });
}
