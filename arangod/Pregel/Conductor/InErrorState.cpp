#include "InErrorState.h"

#include "Pregel/Conductor.h"
#include "Pregel/WorkerConductorMessages.h"

using namespace arangodb::pregel::conductor;

InError::InError(Conductor& conductor) : conductor{conductor} {
  conductor.updateState(ExecutionState::IN_ERROR);
}

auto InError::receive(Message const& message) -> void {
  LOG_PREGEL_CONDUCTOR("14df4", WARN)
      << "When in error, we expect no messages, but received message type "
      << static_cast<int>(message.type());
}
