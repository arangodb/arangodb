#include "StoringState.h"

#include "Pregel/Conductor/Conductor.h"
#include "Metrics/Gauge.h"
#include "Pregel/Conductor/States/State.h"
#include "Pregel/MasterContext.h"
#include "Pregel/PregelFeature.h"
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

auto Storing::_store() -> StoredFuture {
  auto results = std::vector<futures::Future<ResultT<Stored>>>{};
  for (auto&& [_, worker] : conductor.workers) {
    results.emplace_back(worker.store(Store{}));
  }
  return futures::collectAll(results);
}

auto Storing::_cleanup() -> CleanupFuture {
  auto results = std::vector<futures::Future<ResultT<CleanupFinished>>>{};
  for (auto&& [_, worker] : conductor.workers) {
    results.emplace_back(worker.cleanup(Cleanup{}));
  }
  return futures::collectAll(results);
}

auto Storing::run() -> void {
  conductor.cleanup();

  LOG_PREGEL_CONDUCTOR("fc187", DEBUG) << "Finalizing workers";

  auto startCleanupCommand =
      StartCleanup{.gss = conductor._globalSuperstep, .withStoring = true};
  auto response = conductor._sendToAllDBServers(startCleanupCommand);
  if (response.fail()) {
    LOG_PREGEL_CONDUCTOR("f382d", ERR) << "Cleanup could not be started";
  }
}

auto Storing::receive(Message const& message) -> void {
  if (message.type() != MessageType::CleanupFinished) {
    LOG_PREGEL_CONDUCTOR("1b831", WARN)
        << "When storing, we expect a CleanupFinished "
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

    LOG_PREGEL_CONDUCTOR("fc187", DEBUG) << "Cleanup workers";
    _cleanup().thenValue([&](auto results) {
      for (auto const& result : results) {
        if (result.get().fail()) {
          LOG_PREGEL_CONDUCTOR("bc495", ERR) << fmt::format(
              "Got unsuccessful response from worker while cleaning up: {}",
              result.get().errorMessage());
          conductor.changeState(StateType::InError);
          return;
        }
      }
      if (conductor._inErrorAbort) {
        conductor.changeState(StateType::FatalError);
        return;
      }
      conductor.changeState(StateType::Done);
      return;
    });
  });
}
