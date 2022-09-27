#include "LoadingState.h"

#include <fmt/format.h>
#include <memory>
#include "Pregel/Algorithm.h"
#include "Pregel/Conductor/Conductor.h"
#include "Metrics/Gauge.h"
#include "Pregel/MasterContext.h"
#include "Pregel/PregelFeature.h"
#include "Pregel/WorkerConductorMessages.h"

using namespace arangodb::pregel::conductor;

Loading::Loading(Conductor& conductor) : conductor{conductor} {
  conductor._timing.loading.start();
  conductor._feature.metrics()->pregelConductorsLoadingNumber->fetch_add(1);
}

Loading::~Loading() {
  conductor._timing.loading.finish();
  conductor._feature.metrics()->pregelConductorsLoadingNumber->fetch_sub(1);
}

auto Loading::run() -> std::optional<std::unique_ptr<State>> {
  auto create = _createWorkers().get();
  if (create.fail()) {
    LOG_PREGEL_CONDUCTOR("ae855", ERR) << create.errorMessage();
    return std::make_unique<Canceled>(conductor);
  }

  LOG_PREGEL_CONDUCTOR("3a255", DEBUG) << "Telling workers to load the data";
  auto graphLoaded = _loadGraph().get();
  if (graphLoaded.fail()) {
    LOG_PREGEL_CONDUCTOR("8e855", ERR) << graphLoaded.errorMessage();
    return std::make_unique<Canceled>(conductor);
  }

  LOG_PREGEL_CONDUCTOR("76631", INFO)
      << "Running Pregel " << conductor._algorithm->name() << " with "
      << conductor._totalVerticesCount << " vertices, "
      << conductor._totalEdgesCount << " edges";
  if (conductor._masterContext) {
    conductor._masterContext->initialize(conductor._totalVerticesCount,
                                         conductor._totalEdgesCount,
                                         conductor._aggregators.get());
  }

  return std::make_unique<Computing>(conductor);
}

auto Loading::_createWorkers() -> futures::Future<Result> {
  return conductor._initializeWorkers().thenValue([&](auto result) -> Result {
    if (result.fail()) {
      return Result{result.errorNumber(), result.errorMessage()};
    }
    return Result{};
  });
}

auto Loading::_loadGraph() -> futures::Future<Result> {
  return conductor._workers.loadGraph(LoadGraph{})
      .thenValue([&](auto graphLoaded) -> Result {
        if (graphLoaded.fail()) {
          return Result{graphLoaded.errorNumber(),
                        fmt::format("While loading graph: {}",
                                    graphLoaded.errorMessage())};
        }
        conductor._totalVerticesCount += graphLoaded.get().vertexCount;
        conductor._totalEdgesCount += graphLoaded.get().edgeCount;
        return Result{};
      });
}
