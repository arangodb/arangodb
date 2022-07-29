#include "InErrorState.h"

#include <chrono>

#include "Pregel/Conductor.h"
#include "Pregel/WorkerConductorMessages.h"

using namespace arangodb::pregel::conductor;

InError::InError(Conductor& conductor, std::chrono::seconds const& ttl)
    : conductor{conductor} {
  conductor.updateState(ExecutionState::IN_ERROR);
  expiration = std::chrono::system_clock::now() + ttl;
}

auto InError::receive(Message const& message) -> void {
  LOG_PREGEL_CONDUCTOR("563ac", WARN)
      << "When in error, we expect no messages, but received message type "
      << static_cast<int>(message.type());
}
auto InError::recover() -> void {
  conductor.changeState(StateType::Recovering);
}
