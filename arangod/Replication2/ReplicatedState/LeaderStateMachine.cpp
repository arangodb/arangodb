#include "LeaderStateMachine.h"

namespace arangodb::replication2::replicated_state {


auto replicatedLogAction(Log const& log, ParticipantsHealth const& health) -> std::unique_ptr<Action> {

    if(log.plan.term.leader) {
        if (health.contains(log.plan.term.leader->serverId) &&
            log.plan.term.leader->rebootId == health.at(log.plan.term.leader->serverId).rebootId &&
            health.at(log.plan.term.leader->serverId).isHealthy) {

            // Current leader is all healthy so nothing to do.
            return nullptr;
        } else {
            auto newTerm = log.plan.term;
            newTerm.leader.reset();
            newTerm.id += 1;

            return std::make_unique<UpdateTermAction>(newTerm);
        }
    }
    return nullptr;
}

}
