#include "StoringState.h"

#include "Pregel/Conductor/Conductor.h"
#include "Metrics/Gauge.h"
#include "Pregel/Conductor/States/State.h"
#include "Pregel/MasterContext.h"
#include "Pregel/PregelFeature.h"
#include "Pregel/WorkerConductorMessages.h"

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
    LOG_PREGEL_CONDUCTOR("bc495", ERR) << store.errorMessage();
    return std::make_unique<FatalError>(conductor);
  }

  LOG_PREGEL_CONDUCTOR("fc187", DEBUG) << "Cleanup workers";
  auto cleanup = _cleanup().get();
  if (cleanup.fail()) {
    LOG_PREGEL_CONDUCTOR("4b34d", ERR) << cleanup.errorMessage();
    return std::make_unique<FatalError>(conductor);
  }

  return std::make_unique<Done>(conductor);
}

auto Storing::_store() -> futures::Future<Result> {
  return conductor._workers.store(Store{}).thenValue([&](auto results)
                                                         -> Result {
    for (auto const& result : results) {
      if (result.get().fail()) {
        return Result{
            result.get().errorNumber(),
            fmt::format(
                "Got unsuccessful response from worker while storing graph: {}",
                result.get().errorMessage())};
      }
    }
    return Result{};
  });
}

auto Storing::_cleanup() -> futures::Future<Result> {
  return conductor._workers.cleanup(Cleanup{}).thenValue(
      [&](auto results) -> Result {
        for (auto const& result : results) {
          if (result.get().fail()) {
            return Result{
                result.get().errorNumber(),
                fmt::format(
                    "Got unsuccessful response from worker while cleaning "
                    "up: {}",
                    result.get().errorMessage())};
          }
        }
        if (conductor._inErrorAbort) {
          return Result{TRI_ERROR_INTERNAL, "Conductor in error"};
        }
        return Result{};
      });
}
