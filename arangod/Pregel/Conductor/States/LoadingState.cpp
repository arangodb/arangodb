#include "LoadingState.h"

#include <fmt/format.h>
#include "Pregel/Algorithm.h"
#include "Pregel/Conductor/Conductor.h"
#include "Metrics/Gauge.h"
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

auto Loading::run() -> void {
  LOG_PREGEL_CONDUCTOR("3a255", DEBUG) << "Telling workers to load the data";
  conductor._initializeWorkers(VPackSlice())
      .thenValue([&](auto results) {
        for (auto const& result : results) {
          if (result.get().fail()) {
            LOG_PREGEL_CONDUCTOR("8e855", ERR) << fmt::format(
                "Got unsuccessful response from worker while loading graph: "
                "{}\n",
                result.get().errorMessage());
            conductor.changeState(StateType::Canceled);
          }
          auto graphLoaded = result.get().get();
          auto sender = graphLoaded.senderId;
          conductor._totalVerticesCount += graphLoaded.vertexCount;
          conductor._totalEdgesCount += graphLoaded.edgeCount;
        }
      })
      .thenValue([&](auto _) {
        LOG_PREGEL_CONDUCTOR("76631", INFO)
            << "Running Pregel " << conductor._algorithm->name() << " with "
            << conductor._totalVerticesCount << " vertices, "
            << conductor._totalEdgesCount << " edges";
        if (conductor._masterContext) {
          conductor._masterContext->initialize(conductor._totalVerticesCount,
                                               conductor._totalEdgesCount,
                                               conductor._aggregators.get());
        }
        conductor.changeState(StateType::Computing);
      })
      .wait();
}
