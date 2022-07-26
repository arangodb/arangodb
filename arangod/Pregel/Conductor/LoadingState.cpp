#include "LoadingState.h"

#include "Pregel/Algorithm.h"
#include "Pregel/Conductor.h"
#include "Metrics/Gauge.h"
#include "Pregel/Conductor/State.h"
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
  auto res =
      conductor._initializeWorkers(Utils::startExecutionPath, VPackSlice());
  if (res != TRI_ERROR_NO_ERROR) {
    LOG_PREGEL_CONDUCTOR("30171", ERR)
        << "Not all DBServers started the execution";
    conductor.changeState(StateType::Canceled);
  }
}

auto Loading::receive(Message const& message) -> void {
  if (message.type() != MessageType::GraphLoaded) {
    LOG_PREGEL_CONDUCTOR("14df4", WARN)
        << "When loading, we expect a GraphLoaded "
           "message, but we received message type "
        << message.type();
    return;
  }
  auto loadedMessage = static_cast<GraphLoaded const&>(message);
  conductor._ensureUniqueResponse(loadedMessage.senderId);
  conductor._totalVerticesCount += loadedMessage.vertexCount;
  conductor._totalEdgesCount += loadedMessage.edgeCount;
  if (conductor._respondedServers.size() != conductor._dbServers.size()) {
    return;
  }
  LOG_PREGEL_CONDUCTOR("76631", INFO)
      << "Running Pregel " << conductor._algorithm->name() << " with "
      << conductor._totalVerticesCount << " vertices, "
      << conductor._totalEdgesCount << " edges";
  conductor.updateState(ExecutionState::RUNNING);
  // TODO change to ComputingState
  conductor.changeState(StateType::Placeholder);
}
