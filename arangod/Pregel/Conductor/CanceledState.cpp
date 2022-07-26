#include "CanceledState.h"

#include "Basics/FunctionUtils.h"
#include "Pregel/Conductor.h"
#include "Pregel/WorkerConductorMessages.h"
#include "Pregel/PregelFeature.h"

using namespace arangodb::pregel::conductor;

Canceled::Canceled(Conductor& conductor) : conductor{conductor} {
  conductor.updateState(ExecutionState::CANCELED);
  if (not conductor._timing.total.hasFinished()) {
    conductor._timing.total.finish();
  }
}

Canceled::~Canceled() {}

auto Canceled::run() -> void {
  conductor._callbackMutex.assertLockedByCurrentThread();
  LOG_PREGEL_CONDUCTOR("dd721", WARN)
      << "Execution was canceled, results will be discarded.";

  bool ok = basics::function_utils::retryUntilTimeout(
      [this]() -> bool {
        conductor.cleanup();
        LOG_PREGEL_CONDUCTOR("fc187", DEBUG) << "Finalizing workers";
        auto finalizeExecutionCommand =
            FinalizeExecution{.executionNumber = conductor._executionNumber,
                              .gss = conductor._globalSuperstep,
                              .withStoring = false};
        VPackBuilder command;
        serialize(command, finalizeExecutionCommand);

        return conductor._sendToAllDBServers(Utils::finalizeExecutionPath,
                                             command) != TRI_ERROR_QUEUE_FULL;
      },
      Logger::PREGEL, "cancel worker execution");
  if (!ok) {
    LOG_PREGEL_CONDUCTOR("f8b3c", ERR)
        << "Failed to cancel worker execution for five minutes, giving up.";
  }
  conductor._workHandle.reset();
}

auto Canceled::receive(Message const& message) -> void {
  if (message.type() != MessageType::CleanupFinished) {
    LOG_PREGEL_CONDUCTOR("14df4", WARN)
        << "When canceled, we expect a CleanupFinished "
           "message, but we received message type "
        << message.type();
    return;
  }
  if (conductor._inErrorAbort) {
    conductor.updateState(ExecutionState::FATAL_ERROR);
    // TODO change to FatalErrorState
    conductor.changeState(StateType::Placeholder);
    return;
  }

  auto* scheduler = SchedulerFeature::SCHEDULER;
  if (scheduler) {
    scheduler->queue(
        RequestLane::CLUSTER_AQL, [this, self = conductor.shared_from_this()] {
          LOG_PREGEL_CONDUCTOR("6928f", INFO) << "Conductor is erased";
          conductor._feature.cleanupConductor(conductor._executionNumber);
        });
  }
}
