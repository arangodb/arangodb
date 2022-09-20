#include "LoadingState.h"

#include <fmt/format.h>
#include <memory>
#include "Pregel/Algorithm.h"
#include "Pregel/Conductor/Conductor.h"
#include "Metrics/Gauge.h"
#include "Pregel/Conductor/States/CanceledState.h"
#include "Pregel/MasterContext.h"
#include "Pregel/PregelFeature.h"
#include "Pregel/WorkerConductorMessages.h"

using namespace arangodb::pregel::conductor;

Loading::Loading(Conductor& conductor) : conductor{conductor} {
  conductor.updateState(ExecutionState::LOADING);
  conductor._timing.loading.start();
  conductor._feature.metrics()->pregelConductorsLoadingNumber->fetch_add(1);
}

Loading::~Loading() {
  conductor._timing.loading.finish();
  conductor._feature.metrics()->pregelConductorsLoadingNumber->fetch_sub(1);
}

auto Loading::run() -> std::optional<std::unique_ptr<State>> {
  LOG_PREGEL_CONDUCTOR("3a255", DEBUG) << "Telling workers to load the data";
  return conductor._initializeWorkers(VPackSlice())
      .thenValue([&](auto results) -> std::unique_ptr<State> {
        for (auto const& result : results) {
          if (result.get().fail()) {
            LOG_PREGEL_CONDUCTOR("8e855", ERR) << fmt::format(
                "Got unsuccessful response from worker while loading graph: "
                "{}\n",
                result.get().errorMessage());
            return std::make_unique<Canceled>(conductor, conductor._ttl);
          }
          auto graphLoaded = result.get().get();
          conductor._totalVerticesCount += graphLoaded.vertexCount;
          conductor._totalEdgesCount += graphLoaded.edgeCount;
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
      })
      .get();
}
