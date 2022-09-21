#include "CanceledState.h"
#include "fmt/chrono.h"
#include "Basics/FunctionUtils.h"
#include "Pregel/Conductor/Conductor.h"
#include "Pregel/WorkerConductorMessages.h"
#include "Pregel/PregelFeature.h"

using namespace arangodb::pregel::conductor;

Canceled::Canceled(Conductor& conductor) : conductor{conductor} {
  expiration = std::chrono::system_clock::now() + conductor._ttl;
  if (not conductor._timing.total.hasFinished()) {
    conductor._timing.total.finish();
  }
}

auto Canceled::run() -> std::optional<std::unique_ptr<State>> {
  LOG_PREGEL_CONDUCTOR("dd721", WARN)
      << "Execution was canceled, conductor and workers are discarded.";

  return _cleanupUntilTimeout(std::chrono::steady_clock::now())
      .thenValue([&](auto result) -> std::optional<std::unique_ptr<State>> {
        if (result.fail()) {
          LOG_PREGEL_CONDUCTOR("f8b3c", ERR) << result.errorMessage();
          return std::nullopt;
        }

        LOG_PREGEL_CONDUCTOR("6928f", DEBUG) << "Conductor is erased";
        conductor._feature.cleanupConductor(conductor._executionNumber);
        return std::nullopt;
      })
      .get();
}

auto Canceled::_cleanupUntilTimeout(std::chrono::steady_clock::time_point start)
    -> futures::Future<Result> {
  conductor._cleanup();

  if (conductor._feature.isStopping()) {
    LOG_PREGEL_CONDUCTOR("bd540", DEBUG)
        << "Feature is stopping, workers are already shutting down, no need to "
           "clean them up.";
    return Result{};
  }

  LOG_PREGEL_CONDUCTOR("fc187", DEBUG) << "Cleanup workers";
  return conductor._workers.cleanup(Cleanup{}).thenValue([&](auto results) {
    for (auto const& result : results) {
      if (result.get().fail()) {
        LOG_PREGEL_CONDUCTOR("1c495", ERR) << fmt::format(
            "Got unsuccessful response from worker while cleaning up: {}",
            result.get().errorMessage());
        if (std::chrono::steady_clock::now() - start >= _timeout) {
          return Result{
              TRI_ERROR_INTERNAL,
              fmt::format("Failed to cancel worker execution for {}, giving up",
                          _timeout)};
        }
        std::this_thread::sleep_for(_retryInterval);
        return _cleanupUntilTimeout(start).get();
      }
    }

    if (conductor._inErrorAbort) {
      return Result{TRI_ERROR_INTERNAL, "Conductor in error"};
    }
    return Result{};
  });
}
