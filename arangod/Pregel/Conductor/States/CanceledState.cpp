#include "CanceledState.h"

#include "Pregel/Messaging/WorkerMessages.h"
#include "fmt/chrono.h"
#include "Basics/FunctionUtils.h"
#include "Pregel/Conductor/Conductor.h"
#include "Pregel/PregelFeature.h"

using namespace arangodb::pregel::conductor;

namespace {
template<class... Ts>
struct overloaded : Ts... {
  using Ts::operator()...;
};
template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;
}  // namespace

Canceled::Canceled(Conductor& conductor, WorkerApi<CleanupFinished>&& workerApi)
    : conductor{conductor}, _workerApi{std::move(workerApi)} {
  expiration = std::chrono::system_clock::now() + conductor._ttl;
  if (not conductor._timing.total.hasFinished()) {
    conductor._timing.total.finish();
  }
}

auto Canceled::run() -> std::optional<std::unique_ptr<State>> {
  LOG_PREGEL_CONDUCTOR_STATE("dd721", WARN)
      << "Execution was canceled, conductor and workers are discarded.";

  if (conductor._feature.isStopping()) {
    LOG_PREGEL_CONDUCTOR_STATE("bd540", DEBUG)
        << "Feature is stopping, workers are already shutting down, no need to "
           "clean them up.";
    return std::nullopt;
  }

  auto cleanup = _cleanupUntilTimeout(std::chrono::steady_clock::now());
  if (cleanup.fail()) {
    LOG_PREGEL_CONDUCTOR_STATE("f8b3c", ERR) << cleanup.errorMessage();
    return std::nullopt;
  }

  auto aggregate = _workerApi.aggregateAllResponses();
  if (aggregate.fail()) {
    LOG_PREGEL_CONDUCTOR_STATE("cc890", ERR) << aggregate.errorMessage();
    return std::make_unique<FatalError>(conductor, std::move(_workerApi));
  }

  LOG_PREGEL_CONDUCTOR_STATE("6928f", DEBUG) << "Conductor is erased";
  conductor._feature.cleanupConductor(conductor._executionNumber);
  return std::nullopt;
}

auto Canceled::receive(MessagePayload message) -> void {
  auto queued = _workerApi.queue(message);
  // This is a final error state for the Loading state: It is possible
  // that this state receives WorkerCreated messages and this state
  // needs to ignore them in the receive fct.
  if (queued.fail() &&
      !std::holds_alternative<ResultT<WorkerCreated>>(message)) {
    LOG_PREGEL_CONDUCTOR_STATE("88911", ERR) << queued.errorMessage();
  }
};

auto Canceled::_cleanupUntilTimeout(std::chrono::steady_clock::time_point start)
    -> Result {
  LOG_PREGEL_CONDUCTOR_STATE("fc187", DEBUG) << "Cleanup workers";
  auto sent = _workerApi.sendToAll(Cleanup{});
  if (sent.fail()) {
    LOG_PREGEL_CONDUCTOR_STATE("1c495", ERR)
        << fmt::format("While cleaning up: {}", sent.errorMessage());
    if (std::chrono::steady_clock::now() - start >= _timeout) {
      return Result{
          TRI_ERROR_INTERNAL,
          fmt::format("Failed to cancel worker execution for {}, giving up",
                      _timeout)};
    }
    std::this_thread::sleep_for(_retryInterval);
    return _cleanupUntilTimeout(start);
  }
  return Result{};
}
