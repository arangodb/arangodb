#include "FatalErrorState.h"

#include <chrono>

#include "Pregel/Conductor/Conductor.h"
#include "Pregel/WorkerConductorMessages.h"
#include "velocypack/Builder.h"

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

auto FatalError::getResults(bool withId) -> PregelResults {
  auto collectPregelResultsCommand = CollectPregelResults{.withId = withId};
  auto response = conductor._sendToAllDBServers(collectPregelResultsCommand);
  if (response.fail()) {
    THROW_ARANGO_EXCEPTION(response.errorNumber());
  }
  VPackBuilder results;
  {
    VPackArrayBuilder ab(&results);
    for (auto const& message : response.get()) {
      if (!std::holds_alternative<PregelResults>(message.payload)) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                       "Message from worker does not include "
                                       "the expected PregelResults type");
      }
      auto payload = std::get<PregelResults>(message.payload);
      if (payload.results.slice().isArray()) {
        results.add(VPackArrayIterator(payload.results.slice()));
      }
    }
  }
  return PregelResults{.results = results};
}
