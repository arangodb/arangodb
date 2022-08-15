#include "FatalErrorState.h"

#include <chrono>

#include "Pregel/Conductor.h"
#include "Pregel/WorkerConductorMessages.h"

using namespace arangodb::pregel::conductor;

FatalError::FatalError(Conductor& conductor, std::chrono::seconds const& ttl)
    : conductor{conductor} {
  conductor.updateState(ExecutionState::FATAL_ERROR);
  expiration = std::chrono::system_clock::now() + ttl;
  if (not conductor._timing.total.hasFinished()) {
    conductor._timing.total.finish();
  }
}

auto FatalError::receive(Message const& message) -> void {
  LOG_PREGEL_CONDUCTOR("6363d", WARN) << "When in fatal error, we expect no "
                                         "messages, but received message type "
                                      << static_cast<int>(message.type());
}

auto FatalError::getResults(bool withId, VPackBuilder& out) -> void {
  auto collectPregelResultsCommand = CollectPregelResults{
      .executionNumber = conductor._executionNumber, .withId = withId};
  auto response = conductor._sendToAllDBServers<PregelResults>(
      Utils::aqlResultsPath, collectPregelResultsCommand);
  if (response.fail()) {
    THROW_ARANGO_EXCEPTION(response.errorNumber());
  }
  {
    VPackArrayBuilder ab(&out);
    for (auto const& message : response.get()) {
      if (message.results.slice().isArray()) {
        out.add(VPackArrayIterator(message.results.slice()));
      }
    }
  }
}
