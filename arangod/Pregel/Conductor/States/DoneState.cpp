#include "DoneState.h"

#include "Pregel/AggregatorHandler.h"
#include "Pregel/Conductor/Conductor.h"
#include "velocypack/Builder.h"
#include "velocypack/Iterator.h"

using namespace arangodb::pregel::conductor;

Done::Done(Conductor& conductor) : conductor{conductor} {
  expiration = std::chrono::system_clock::now() + conductor._ttl;
  if (!conductor._timing.total.hasFinished()) {
    conductor._timing.total.finish();
  }
}

auto Done::run() -> std::optional<std::unique_ptr<State>> {
  VPackBuilder debugOut;
  {
    VPackObjectBuilder ob(&debugOut);
    conductor._aggregators->serializeValues(debugOut);
  }
  auto stats = velocypack::serialize(conductor._statistics);

  LOG_PREGEL_CONDUCTOR_STATE("063b5", INFO)
      << fmt::format("Done. We did {} rounds.", conductor._globalSuperstep)
      << (conductor._timing.loading.hasStarted()
              ? fmt::format(" Startup time: {}s",
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
      << fmt::format(", overall: {}s, stats: {}, aggregators: {}",
                     conductor._timing.total.elapsedSeconds().count(),
                     stats.toJson(), debugOut.toJson());
  return std::nullopt;
}

auto Done::cancel() -> std::optional<std::unique_ptr<State>> {
  return std::make_unique<Canceled>(conductor);
}

auto Done::getResults(bool withId) -> ResultT<PregelResults> {
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
