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

#include "Logger/LogMacros.h"
#include "Random/RandomGenerator.h"

#include "Algorithms.h"

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
      //    - it is seen as healthy be the supervision

      // if enough servers are found, declare the server with
      // the "best" log as leader in a new term
      agency::LogCurrentSupervisionElection election;
      election.term = spec.currentTerm ? spec.currentTerm->term : LogTerm{0};

      auto newLeaderSet = std::vector<replication2::ParticipantId>{};
      auto bestTermIndex = replication2::replicated_log::TermIndexPair{};
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
        // TODO is this still correct for MoveShard if Participants > ReplicationFactor?
        return spec->participants.size() - spec->config.writeConcern + 1;
      });

      TRI_ASSERT(requiredNumberOfAvailableParticipants > 0);

      election.participantsRequired = requiredNumberOfAvailableParticipants;
      election.participantsAvailable = numberOfAvailableParticipants;

      if (numberOfAvailableParticipants >= requiredNumberOfAvailableParticipants) {
        TRI_ASSERT(!newLeaderSet.empty());
        // Randomly select one of the best participants
        auto const& newLeader = newLeaderSet.at(RandomGenerator::interval(newLeaderSet.size() - 1));
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
