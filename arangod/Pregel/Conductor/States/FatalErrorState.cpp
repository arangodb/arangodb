#include "FatalErrorState.h"

#include "Pregel/Conductor/Conductor.h"

using namespace arangodb::pregel::conductor;

FatalError::FatalError(Conductor& conductor, WorkerApi<VoidMessage>&& workerApi)
    : conductor{conductor}, _workerApi{std::move(workerApi)} {
  expiration = std::chrono::system_clock::now() + conductor._ttl;
  if (not conductor._timing.total.hasFinished()) {
    conductor._timing.total.finish();
  }
}

auto FatalError::cancel() -> std::optional<std::unique_ptr<State>> {
  return std::make_unique<Canceled>(conductor, std::move(_workerApi));
}

auto FatalError::getResults(bool withId) -> ResultT<PregelResults> {
  auto pregelResults =
      _workerApi
          .requestFromAll<PregelResults>(CollectPregelResults{.withId = withId})
          .get();
  if (pregelResults.fail()) {
    return Result{pregelResults.errorNumber(),
                  fmt::format("While requesting pregel results: {}",
                              pregelResults.errorMessage())};
  }
  return pregelResults;
}
