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
  return _aggregate.doUnderLock(
      [&](auto& agg) -> std::optional<std::unique_ptr<State>> {
        auto aggregate = conductor._workers.store(Store{});
        if (aggregate.fail()) {
          LOG_PREGEL_CONDUCTOR_STATE("dddad", ERR) << aggregate.errorMessage();
          return std::make_unique<FatalError>(conductor);
        }
        agg = aggregate.get();
        return std::nullopt;
      });
}

auto Storing::receive(MessagePayload message)
    -> std::optional<std::unique_ptr<State>> {
  auto explicitMessage = getResultTMessage<Stored>(message);
  if (explicitMessage.fail()) {
    LOG_PREGEL_CONDUCTOR_STATE("7698e", ERR) << explicitMessage.errorMessage();
    return std::make_unique<FatalError>(conductor);
  }
  auto finishedAggregate =
      _aggregate.doUnderLock([message = std::move(explicitMessage).get()](
                                 auto& agg) { return agg.aggregate(message); });
  if (!finishedAggregate.has_value()) {
    return std::nullopt;
  }

  LOG_PREGEL_CONDUCTOR_STATE("fc187", DEBUG) << "Cleanup workers";
  auto cleanup = _cleanup().get();
  if (cleanup.fail()) {
    LOG_PREGEL_CONDUCTOR_STATE("4b34d", ERR) << cleanup.errorMessage();
    return std::make_unique<FatalError>(conductor);
  }

  return std::make_unique<Done>(conductor);
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
