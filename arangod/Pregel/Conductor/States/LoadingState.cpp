#include "LoadingState.h"

#include "Metrics/Gauge.h"
#include "Pregel/Algorithm.h"
#include "Pregel/Conductor/Conductor.h"
#include "Pregel/MasterContext.h"
#include "Pregel/PregelFeature.h"

using namespace arangodb::pregel::conductor;

Loading::Loading(Conductor& conductor, WorkerApi<GraphLoaded>&& workerApi)
    : conductor{conductor}, _workerApi{std::move(workerApi)} {
  conductor._timing.loading.start();
  conductor._feature.metrics()->pregelConductorsLoadingNumber->fetch_add(1);
}

Loading::~Loading() {
  conductor._timing.loading.finish();
  conductor._feature.metrics()->pregelConductorsLoadingNumber->fetch_sub(1);
}

auto Loading::run() -> std::optional<std::unique_ptr<State>> {
  LOG_PREGEL_CONDUCTOR_STATE("3a255", DEBUG)
      << "Telling workers to load the data";
  auto sent = _workerApi.sendToAll(LoadGraph{});
  if (sent.fail()) {
    LOG_PREGEL_CONDUCTOR_STATE("dddad", ERR) << sent.errorMessage();
    return std::make_unique<FatalError>(conductor, std::move(_workerApi));
  }

  auto aggregate = _workerApi.aggregateAllResponses();
  if (aggregate.fail()) {
    LOG_PREGEL_CONDUCTOR_STATE("1ddad", ERR) << aggregate.errorMessage();
    return std::make_unique<FatalError>(conductor, std::move(_workerApi));
  }

  conductor._totalVerticesCount += aggregate.get().vertexCount;
  conductor._totalEdgesCount += aggregate.get().edgeCount;

  if (conductor._masterContext) {
    conductor._masterContext->initialize(conductor._totalVerticesCount,
                                         conductor._totalEdgesCount,
                                         conductor._aggregators.get());
  }

  LOG_PREGEL_CONDUCTOR_STATE("76631", INFO)
      << fmt::format("Start running Pregel {} with {} vertices, {} edges",
                     conductor._algorithm->name(),
                     conductor._totalVerticesCount, conductor._totalEdgesCount);

  return std::make_unique<Computing>(conductor, std::move(_workerApi));
}

auto Loading::receive(MessagePayload message) -> void {
  auto queued = _workerApi.queue(message);
  if (queued.fail()) {
    LOG_PREGEL_CONDUCTOR_STATE("7698e", ERR) << queued.errorMessage();
  }
}

auto Loading::cancel() -> std::optional<std::unique_ptr<State>> {
  return std::make_unique<Canceled>(conductor, std::move(_workerApi));
}
