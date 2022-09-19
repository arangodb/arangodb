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

auto FatalError::_results(bool withId) -> ResultsFuture {
  auto results = std::vector<futures::Future<ResultT<PregelResults>>>{};
  for (auto&& [_, worker] : conductor.workers) {
    results.emplace_back(
        worker.results(CollectPregelResults{.withId = withId}));
  }
  return futures::collectAll(results);
}

auto FatalError::getResults(bool withId) -> ResultT<PregelResults> {
  return _results(withId)
      .thenValue([&](auto responses) -> ResultT<PregelResults> {
        VPackBuilder pregelResults;
        {
          VPackArrayBuilder ab(&pregelResults);
          for (auto const& response : responses) {
            if (response.get().fail()) {
              return Result{TRI_ERROR_INTERNAL,
                            fmt::format("Got unsuccessful response from worker "
                                        "while requesting results: "
                                        "{}",
                                        response.get().errorMessage())};
            }
            if (auto slice = response.get().get().results.slice();
                slice.isArray()) {
              pregelResults.add(VPackArrayIterator(slice));
            }
          }
        }
        return PregelResults{pregelResults};
      })
      .get();
}
