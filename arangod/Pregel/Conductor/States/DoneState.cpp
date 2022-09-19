#include "DoneState.h"

#include <chrono>
#include "Pregel/AggregatorHandler.h"
#include "Pregel/Conductor/Conductor.h"
#include "Pregel/WorkerConductorMessages.h"
#include "velocypack/Builder.h"
#include "velocypack/Iterator.h"

using namespace arangodb::pregel::conductor;

Done::Done(Conductor& conductor, std::chrono::seconds const& ttl)
    : conductor{conductor} {
  conductor.updateState(ExecutionState::DONE);
  expiration = std::chrono::system_clock::now() + ttl;
  if (!conductor._timing.total.hasFinished()) {
    conductor._timing.total.finish();
  }
}

auto Done::run() -> void {
  VPackBuilder debugOut;
  debugOut.openObject();
  debugOut.add("stats", VPackValue(VPackValueType::Object));
  conductor._statistics.serializeValues(debugOut);
  debugOut.close();
  conductor._aggregators->serializeValues(debugOut);
  debugOut.close();

  LOG_PREGEL_CONDUCTOR("063b5", INFO)
      << "Done. We did " << conductor._globalSuperstep << " rounds."
      << (conductor._timing.loading.hasStarted()
              ? fmt::format("Startup time: {}s",
                            conductor._timing.loading.elapsedSeconds().count())
              : "")
      << (conductor._timing.computation.hasStarted()
              ? fmt::format(
                    ", computation time: {}s",
                    conductor._timing.computation.elapsedSeconds().count())
              : "")
      << (conductor._storeResults
              ? fmt::format(", storage time: {}s",
                            conductor._timing.storing.elapsedSeconds().count())
              : "")
      << ", overall: " << conductor._timing.total.elapsedSeconds().count()
      << "s"
      << ", stats: " << debugOut.slice().toJson();
}

auto Done::receive(Message const& message) -> void {
  LOG_PREGEL_CONDUCTOR("88f66", WARN)
      << "When done, we expect no messages, but received message type "
      << static_cast<int>(message.type());
}

auto Done::getResults(bool withId) -> PregelResults {
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
