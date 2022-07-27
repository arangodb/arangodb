#include "FatalErrorState.h"

#include "Pregel/Conductor.h"
#include "Pregel/WorkerConductorMessages.h"

using namespace arangodb::pregel::conductor;

FatalError::FatalError(Conductor& conductor) : conductor{conductor} {
  conductor.updateState(ExecutionState::FATAL_ERROR);
  if (not conductor._timing.total.hasFinished()) {
    conductor._timing.total.finish();
  }
}

auto FatalError::receive(Message const& message) -> void {
  LOG_PREGEL_CONDUCTOR("6363d", WARN) << "When in fatal error, we expect no "
                                         "messages, but received message type "
                                      << static_cast<int>(message.type());
}
