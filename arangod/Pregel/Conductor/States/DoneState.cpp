#include "DoneState.h"

#include "Pregel/AggregatorHandler.h"
#include "Pregel/Conductor/Conductor.h"
#include "Pregel/WorkerConductorMessages.h"
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
  return std::nullopt;
}

auto Done::getResults(bool withId) -> ResultT<PregelResults> {
  return conductor._workers.results(CollectPregelResults{.withId = withId})
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
