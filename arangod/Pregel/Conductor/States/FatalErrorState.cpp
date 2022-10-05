#include "FatalErrorState.h"

#include "Pregel/Conductor/Conductor.h"
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
      .thenValue([&](auto pregelResults) -> ResultT<PregelResults> {
        if (pregelResults.fail()) {
          return Result{pregelResults.errorNumber(),
                        fmt::format("While requesting pregel results: {}",
                                    pregelResults.errorMessage())};
        }
        return pregelResults;
      })
      .get();
}
