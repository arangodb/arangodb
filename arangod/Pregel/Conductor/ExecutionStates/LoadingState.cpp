#include "LoadingState.h"
#include "Pregel/AggregatorHandler.h"
#include "Pregel/Conductor/ExecutionStates/ComputingState.h"

using namespace arangodb::pregel::conductor;

Loading::Loading(ConductorState& conductor) : conductor{conductor} {
  conductor.timing.loading.start();
  // TODO GORDO-1510
  // _feature.metrics()->pregelConductorsLoadingNumber->fetch_add(1);
}
Loading::~Loading() {
  conductor.timing.loading.finish();
  // TODO GORDO-1510
  // conductor._feature.metrics()->pregelConductorsLoadingNumber->fetch_sub(1);
}
auto Loading::message() -> worker::message::WorkerMessages {
  return worker::message::LoadGraph{};
};
auto Loading::receive(actor::ActorPID sender,
                      message::ConductorMessages message)
    -> std::optional<std::unique_ptr<ExecutionState>> {
  if (not conductor.workers.contains(sender) or
      not std::holds_alternative<ResultT<message::GraphLoaded>>(message)) {
    // TODO return error state (GORDO-1553)
    return std::nullopt;
  }
  auto workerCreated = std::get<ResultT<message::GraphLoaded>>(message);
  if (not workerCreated.ok()) {
    // TODO return error state (GORDO-1553)
    return std::nullopt;
  }
  respondedWorkers.emplace(sender);
  totalVerticesCount += workerCreated.get().vertexCount;
  totalEdgesCount += workerCreated.get().edgeCount;

  if (respondedWorkers == conductor.workers) {
    return std::make_unique<Computing>(
        conductor,
        conductor.algorithm->masterContextUnique(
            totalVerticesCount, totalEdgesCount,
            std::make_unique<AggregatorHandler>(conductor.algorithm.get()),
            conductor.specifications.userParameters.slice()));
  }

  return std::nullopt;
};
