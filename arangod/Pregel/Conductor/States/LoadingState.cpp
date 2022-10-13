#include "LoadingState.h"

#include <fmt/format.h>
#include <memory>
#include <optional>
#include "Pregel/Algorithm.h"
#include "Pregel/Conductor/Conductor.h"
#include "Metrics/Gauge.h"
#include "Pregel/MasterContext.h"
#include "Pregel/Messaging/Message.h"
#include "Pregel/Messaging/WorkerMessages.h"
#include "Pregel/PregelFeature.h"

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
    LOG_PREGEL_CONDUCTOR("ae855", ERR)
        << fmt::format("Loading state: {}", create.errorMessage());
    return std::make_unique<Canceled>(conductor);
  }

  LOG_PREGEL_CONDUCTOR("3a255", DEBUG) << "Telling workers to load the data";
  return _aggregate.doUnderLock(
      [&](auto& agg) -> std::optional<std::unique_ptr<State>> {
        auto aggregate = conductor._workers.loadGraph(LoadGraph{});
        if (aggregate.fail()) {
          LOG_PREGEL_CONDUCTOR("dddad", ERR)
              << fmt::format("{} state: {}", name(), aggregate.errorMessage());
          return std::make_unique<Canceled>(conductor);
        }
        agg = aggregate.get();
        return std::nullopt;
      });
}

auto Loading::_createWorkers() -> futures::Future<Result> {
  return conductor._initializeWorkers().thenValue([&](auto result) -> Result {
    if (result.fail()) {
      return Result{result.errorNumber(), result.errorMessage()};
    }
    return Result{};
  });
}

auto Loading::receive(MessagePayload message)
    -> std::optional<std::unique_ptr<State>> {
  auto explicitMessage = getResultTMessage<GraphLoaded>(message);
  if (explicitMessage.fail()) {
    LOG_PREGEL_CONDUCTOR("7698e", ERR)
        << fmt::format("{} state: {}", name(), explicitMessage.errorMessage());
    return std::make_unique<Canceled>(conductor);
  }
  auto finishedAggregate =
      _aggregate.doUnderLock([message = std::move(explicitMessage).get()](
                                 auto& agg) { return agg.aggregate(message); });
  if (!finishedAggregate.has_value()) {
    return std::nullopt;
  }

  auto graphLoadedData = finishedAggregate.value();
  conductor._totalVerticesCount += graphLoadedData.vertexCount;
  conductor._totalEdgesCount += graphLoadedData.edgeCount;

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
