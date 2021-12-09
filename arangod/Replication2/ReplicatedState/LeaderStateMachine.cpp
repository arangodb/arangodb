#include "LeaderStateMachine.h"

namespace arangodb::replication2::replicated_state {

auto replicatedStateAction(log::Log, state::State) -> action::Action {
    return action::Action::Delete;
}

}
