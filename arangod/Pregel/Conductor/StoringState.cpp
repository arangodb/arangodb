#include "StoringState.h"

#include "Pregel/Conductor.h"
#include "Metrics/Gauge.h"
#include "Pregel/Conductor/State.h"
#include "Pregel/MasterContext.h"
#include "Pregel/PregelFeature.h"
#include "Pregel/Recovery.h"
#include "Pregel/WorkerConductorMessages.h"

using namespace arangodb::pregel::conductor;

Storing::Storing(Conductor& conductor) : conductor{conductor} {
  conductor.updateState(ExecutionState::STORING);
  conductor._timing.storing.start();
  conductor._feature.metrics()->pregelConductorsStoringNumber->fetch_add(1);
}

Storing::~Storing() {
  conductor._timing.storing.finish();
  conductor._feature.metrics()->pregelConductorsStoringNumber->fetch_sub(1);
}

auto Storing::run() -> void {
  conductor._callbackMutex.assertLockedByCurrentThread();

  conductor.cleanup();

  LOG_PREGEL_CONDUCTOR("fc187", DEBUG) << "Finalizing workers";

  auto finalizeExecutionCommand =
      FinalizeExecution{.executionNumber = conductor._executionNumber,
                        .gss = conductor._globalSuperstep,
                        .withStoring = true};
  VPackBuilder command;
  serialize(command, finalizeExecutionCommand);

  conductor._sendToAllDBServers(Utils::finalizeExecutionPath, command);
}

auto Storing::receive(Message const& message) -> void {
  if (message.type() != MessageType::CleanupFinished) {
    LOG_PREGEL_CONDUCTOR("14df4", WARN)
        << "When storing, we expect a CleanupFinished "
           "message, but we received message type "
        << static_cast<int>(message.type());
    return;
  }
  if (conductor._inErrorAbort) {
    conductor.updateState(ExecutionState::FATAL_ERROR);
    // TODO change to FatalErrorState
    conductor.changeState(StateType::Placeholder);
    return;
  }
  conductor.changeState(StateType::Done);
}
