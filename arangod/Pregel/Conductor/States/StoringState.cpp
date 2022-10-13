#include "StoringState.h"

#include "Pregel/Conductor/Conductor.h"
#include "Metrics/Gauge.h"
#include "Pregel/Conductor/States/State.h"
#include "Pregel/MasterContext.h"
#include "Pregel/PregelFeature.h"

using namespace arangodb::pregel::conductor;

Storing::Storing(Conductor& conductor) : conductor{conductor} {
  conductor._timing.storing.start();
  conductor._feature.metrics()->pregelConductorsStoringNumber->fetch_add(1);
}

Storing::~Storing() {
  conductor._timing.storing.finish();
  conductor._feature.metrics()->pregelConductorsStoringNumber->fetch_sub(1);
}

auto Storing::run() -> std::optional<std::unique_ptr<State>> {
  conductor._cleanup();

  auto store = _store().get();
  if (store.fail()) {
    LOG_PREGEL_CONDUCTOR_STATE("bc495", ERR) << store.errorMessage();
    return std::make_unique<FatalError>(conductor);
  }

  LOG_PREGEL_CONDUCTOR_STATE("fc187", DEBUG) << "Cleanup workers";
  auto cleanup = _cleanup().get();
  if (cleanup.fail()) {
    LOG_PREGEL_CONDUCTOR_STATE("4b34d", ERR) << cleanup.errorMessage();
    return std::make_unique<FatalError>(conductor);
  }

  return std::make_unique<Done>(conductor);
}

auto Storing::_store() -> futures::Future<Result> {
  return conductor._workers.store(Store{}).thenValue(
      [&](auto stored) -> Result {
        if (stored.fail()) {
          return Result{
              stored.errorNumber(),
              fmt::format("While storing graph: {}", stored.errorMessage())};
        }
        return Result{};
      });
}

auto Storing::_cleanup() -> futures::Future<Result> {
  return conductor._workers.cleanup(Cleanup{}).thenValue(
      [&](auto cleanupFinished) -> Result {
        if (cleanupFinished.fail()) {
          return Result{cleanupFinished.errorNumber(),
                        fmt::format("While cleaning up: {}",
                                    cleanupFinished.errorMessage())};
        }
        return Result{};
      });
}
