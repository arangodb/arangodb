#include "CanceledState.h"
#include <chrono>

#include "Basics/FunctionUtils.h"
#include "Pregel/Conductor.h"
#include "Pregel/WorkerConductorMessages.h"
#include "Pregel/PregelFeature.h"

using namespace arangodb::pregel::conductor;

Canceled::Canceled(Conductor& conductor, std::chrono::seconds const& ttl)
    : conductor{conductor} {
  conductor.updateState(ExecutionState::CANCELED);
  expiration = std::chrono::system_clock::now() + ttl;
  if (not conductor._timing.total.hasFinished()) {
    conductor._timing.total.finish();
  }
}

auto Canceled::run() -> void {
  conductor._callbackMutex.assertLockedByCurrentThread();
  LOG_PREGEL_CONDUCTOR("dd721", WARN)
      << "Execution was canceled, results will be discarded.";

  bool ok = basics::function_utils::retryUntilTimeout(
      [this]() -> bool {
        conductor.cleanup();
        LOG_PREGEL_CONDUCTOR("fc187", DEBUG) << "Finalizing workers";
        auto startCleanupCommand =
            StartCleanup{.executionNumber = conductor._executionNumber,
                         .gss = conductor._globalSuperstep,
                         .withStoring = false};
        auto response = conductor._sendToAllDBServers<CleanupStarted>(
            Utils::finalizeExecutionPath, startCleanupCommand);
        return response.ok();
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
        << static_cast<int>(message.type());
    return;
  }
  auto event = static_cast<CleanupFinished const&>(message);
  conductor._ensureUniqueResponse(event.senderId);
  {
    auto reports = event.reports.slice();
    if (reports.isArray()) {
      conductor._reports.appendFromSlice(reports);
    }
  }
  if (conductor._respondedServers.size() != conductor._dbServers.size()) {
    return;
  }

  if (conductor._inErrorAbort) {
    conductor.changeState(StateType::FatalError);
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
