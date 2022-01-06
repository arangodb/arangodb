
#include "LeaderStateMachine.h"

#include "Basics/Exceptions.h"
#include "Basics/StringUtils.h"
#include "Basics/application-exit.h"
#include "Logger/LogMacros.h"
#include "Random/RandomGenerator.h"
#include "velocypack/Builder.h"
#include "velocypack/Value.h"

#include <cstdint>
#include <limits>
#include <memory>

namespace arangodb::replication2::replicated_state {

auto to_string(LeaderElectionCampaign::Reason reason) -> std::string_view {
  switch (reason) {
    case LeaderElectionCampaign::Reason::OK: {
      return "OK";
    } break;
    case LeaderElectionCampaign::Reason::ServerIll: {
      return "ServerIll";
    } break;
    case LeaderElectionCampaign::Reason::TermNotConfirmed: {
      return "TermNotConfirmed";
    } break;
  }
  return "this-value-is-here-to-shut-up-the-compiler-if-this-is-reached-that-"
         "is-a-bug";
}

auto operator<<(std::ostream& os, LeaderElectionCampaign::Reason reason)
    -> std::ostream& {
  return os << to_string(reason);
}

void LeaderElectionCampaign::toVelocyPack(VPackBuilder& builder) const {
  auto ob = VPackObjectBuilder(&builder);
  builder.add("numberOKParticipants", VPackValue(numberOKParticipants));

  builder.add(VPackValue("bestTermIndex"));
  bestTermIndex.toVelocyPack(builder);

  {
    auto rb = VPackObjectBuilder(&builder, "reasons");
    for (auto const& [participant, reason] : reasons) {
      builder.add(VPackValue(participant));
      builder.add(VPackValue(to_string(reason)));
    }
  }

  {
    auto eb = VPackArrayBuilder(&builder, "electibleLeaderSet");
    for (auto const& participant : electibleLeaderSet) {
      builder.add(VPackValue(participant));
    }
  }
}

auto to_string(LeaderElectionCampaign const& campaign) -> std::string {
  auto bb = VPackBuilder{};
  campaign.toVelocyPack(bb);
  return bb.toString();
}

auto operator<<(std::ostream& os, Action::ActionType const& action)
    -> std::ostream&;

auto to_string(Action::ActionType action) -> std::string_view {
  switch (action) {
    case Action::ActionType::FailedLeaderElectionAction: {
      return "FailedLeaderElection";
    } break;
    case Action::ActionType::SuccessfulLeaderElectionAction: {
      return "SuccessfulLeaderElection";
    } break;
    case Action::ActionType::UpdateTermAction: {
      return "UpdateTermAction";
    } break;
    case Action::ActionType::ImpossibleCampaignAction: {
      return "ImpossibleCampaignAction";
    } break;
  }
  return "this-value-is-here-to-shut-up-the-compiler-if-this-is-reached-that-"
         "is-a-bug";
}

auto operator<<(std::ostream& os, Action::ActionType const& action)
    -> std::ostream& {
  return os << to_string(action);
}

auto computeReason(LogCurrentLocalState const& status, bool healthy,
                   LogTerm term) -> LeaderElectionCampaign::Reason {
  if (!healthy) {
    return LeaderElectionCampaign::Reason::ServerIll;
  } else if (term != status.term) {
    return LeaderElectionCampaign::Reason::TermNotConfirmed;
  } else {
    return LeaderElectionCampaign::Reason::OK;
  }
}

auto runElectionCampaign(LogCurrentLocalStates const& states,
                         ParticipantsHealth const& health, LogTerm term)
    -> LeaderElectionCampaign {
  auto campaign = LeaderElectionCampaign{};

  for (auto const& [participant, status] : states) {
    auto reason = computeReason(status, health.isHealthy(participant), term);
    campaign.reasons.emplace(participant, reason);

    if (reason == LeaderElectionCampaign::Reason::OK) {
      campaign.numberOKParticipants += 1;

      if (status.spearhead >= campaign.bestTermIndex) {
        if (status.spearhead != campaign.bestTermIndex) {
          campaign.electibleLeaderSet.clear();
        }
        campaign.electibleLeaderSet.push_back(participant);
        campaign.bestTermIndex = status.spearhead;
      }
    }
  }
  return campaign;
}

auto checkLeaderHealth(Log const& log, ParticipantsHealth const& health)
    -> std::unique_ptr<Action> {
  if (health.isHealthy(log.plan.currentTerm->leader->serverId) &&
      health.validRebootId(log.plan.currentTerm->leader->serverId,
                           log.plan.currentTerm->leader->rebootId)) {
    // Current leader is all healthy so nothing to do.
    return nullptr;
  } else {
    // Leader is not healthy; start a new term
    auto newTerm = *log.plan.currentTerm;

    newTerm.leader.reset();
    newTerm.term = LogTerm{log.plan.currentTerm->term.value + 1};

    return std::make_unique<UpdateTermAction>(newTerm);
  }
}

auto tryLeadershipElection(Log const& log, ParticipantsHealth const& health)
    -> std::unique_ptr<Action> {
  // Check whether there are enough participants to reach a quorum
  if (log.plan.participantsConfig.participants.size() + 1 <=
      log.plan.currentTerm->config.writeConcern) {
    return std::make_unique<ImpossibleCampaignAction>(
        /* TODO: should we have an error message? */);
  }

  TRI_ASSERT(log.plan.participantsConfig.participants.size() + 1 >
             log.plan.currentTerm->config.writeConcern);

  auto const requiredNumberOfOKParticipants =
      log.plan.participantsConfig.participants.size() + 1 -
      log.plan.currentTerm->config.writeConcern;

  // Find the participants that are healthy and that have the best LogTerm
  auto const campaign = runElectionCampaign(log.current.localState, health,
                                            log.plan.currentTerm->term);

  auto const numElectible = campaign.electibleLeaderSet.size();

  // Something went really wrong: we have enough ok participants, but none
  // of them is electible, or too many of them are (we only support
  // uint16_t::max participants at the moment)
  //
  // TODO: should this really be throwing or just erroring?
  if (ADB_UNLIKELY(numElectible == 0 ||
                   numElectible > std::numeric_limits<uint16_t>::max())) {
    abortOrThrow(TRI_ERROR_NUMERIC_OVERFLOW,
                 basics::StringUtils::concatT(
                     "Number of participants electible for leadership out "
                     "of range, should be between ",
                     1, " and ", std::numeric_limits<uint16_t>::max(),
                     ", but is ", numElectible),
                 ADB_HERE);
  }

  if (campaign.numberOKParticipants >= requiredNumberOfOKParticipants) {
    // We randomly elect on of the electible leaders
    auto const maxIdx = static_cast<uint16_t>(numElectible - 1);
    auto const& newLeader =
        campaign.electibleLeaderSet.at(RandomGenerator::interval(maxIdx));
    auto const& newLeaderRebootId = health._health.at(newLeader).rebootId;

    auto action = std::make_unique<SuccessfulLeaderElectionAction>();

    action->_campaign = campaign;
    action->_newTerm = LogPlanTermSpecification(
        LogTerm{log.plan.currentTerm->term.value + 1},
        log.plan.currentTerm->config,
        LogPlanTermSpecification::Leader{.serverId = newLeader,
                                         .rebootId = newLeaderRebootId},
        log.plan.currentTerm->participants);
    action->_newLeader = newLeader;

    return action;
  } else {
    // Not enough participants were available to form a quorum, so
    // we can't elect a leader
    auto action = std::make_unique<FailedLeaderElectionAction>();
    action->_campaign = campaign;
    return action;
  }
}

auto replicatedLogAction(Log const& log, ParticipantsHealth const& health)
    -> std::unique_ptr<Action> {
  if (log.plan.currentTerm->leader) {
    // We have a leader; we check that the leader is
    // healthy; if it is, there is nothing to do,
    // if it isn't we
    return checkLeaderHealth(log, health);
  } else {
    // New leader required; we try running an election
    // TODO: Why are we duplicating this in replicated state
    return tryLeadershipElection(log, health);
  }

  // This is only here to make the compiler happy; we should never end up here;
  TRI_ASSERT(false);
  return nullptr;
}

}  // namespace arangodb::replication2::replicated_state
