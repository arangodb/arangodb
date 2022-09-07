#include "InitialState.h"
#include "Pregel/Conductor/Conductor.h"
#include "Pregel/WorkerConductorMessages.h"

using namespace arangodb::pregel::conductor;

Initial::Initial(Conductor& conductor) : conductor{conductor} {}

// todo: remove Initial
auto Initial::run() -> void { conductor.changeState(StateType::Loading); }

auto Initial::receive(Message const& message) -> void {
  LOG_PREGEL_CONDUCTOR("54f7b", WARN)
      << "When done, we expect no messages, but received message type "
      << static_cast<int>(message.type());
}
