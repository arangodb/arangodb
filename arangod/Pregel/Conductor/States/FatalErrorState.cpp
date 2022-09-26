#include "FatalErrorState.h"

#include "Pregel/Conductor/Conductor.h"
#include "Pregel/WorkerConductorMessages.h"
#include "velocypack/Builder.h"

using namespace arangodb::pregel::conductor;

FatalError::FatalError(Conductor& conductor) : conductor{conductor} {
  expiration = std::chrono::system_clock::now() + conductor._ttl;
  if (not conductor._timing.total.hasFinished()) {
    conductor._timing.total.finish();
  }
}

auto FatalError::getResults(bool withId) -> ResultT<PregelResults> {
  return conductor._workers.results(CollectPregelResults{.withId = withId})
      .thenValue([&](auto responses) -> ResultT<PregelResults> {
        PregelResults pregelResults;
        for (auto const& response : responses) {
          if (response.get().fail()) {
            return Result{TRI_ERROR_INTERNAL,
                          fmt::format("Got unsuccessful response from worker "
                                      "while requesting results: "
                                      "{}",
                                      response.get().errorMessage())};
          }
          pregelResults.add(response.get().get());
        }
        return pregelResults;
      })
      .get();
}
