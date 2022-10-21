#include "StoringState.h"

#include "Pregel/Conductor/Conductor.h"
#include "Metrics/Gauge.h"
#include "Pregel/Conductor/States/State.h"
#include "Pregel/MasterContext.h"
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
  return std::visit(
      overloaded{
          [&](ResultT<Stored> const& x)
              -> std::optional<std::unique_ptr<State>> {
            auto explicitMessage = getResultTMessage<Stored>(message);
            if (explicitMessage.fail()) {
              LOG_PREGEL_CONDUCTOR_STATE("7698e", ERR)
                  << explicitMessage.errorMessage();
              return std::make_unique<FatalError>(conductor);
            }
            auto finishedAggregate = _aggregate.doUnderLock(
                [message = std::move(explicitMessage).get()](auto& agg) {
                  return agg.aggregate(message);
                });
            if (!finishedAggregate.has_value()) {
              return std::nullopt;
            }

            LOG_PREGEL_CONDUCTOR_STATE("fc187", DEBUG) << "Cleanup workers";
            return _cleanupAggregate.doUnderLock(
                [&](auto& agg) -> std::optional<std::unique_ptr<State>> {
                  auto aggregate = conductor._workers.cleanup(Cleanup{});
                  if (aggregate.fail()) {
                    LOG_PREGEL_CONDUCTOR_STATE("dddad", ERR)
                        << aggregate.errorMessage();
                    return std::make_unique<FatalError>(conductor);
                  }
                  agg = aggregate.get();
                  return std::nullopt;
                });
          },
          [&](ResultT<CleanupFinished> const& x)
              -> std::optional<std::unique_ptr<State>> {
            auto explicitMessage = getResultTMessage<CleanupFinished>(message);
            if (explicitMessage.fail()) {
              LOG_PREGEL_CONDUCTOR_STATE("dde4a", ERR)
                  << explicitMessage.errorMessage();
              return std::make_unique<FatalError>(conductor);
            }
            auto finishedAggregate = _cleanupAggregate.doUnderLock(
                [message = std::move(explicitMessage).get()](auto& agg) {
                  return agg.aggregate(message);
                });
            if (!finishedAggregate.has_value()) {
              return std::nullopt;
            }

            return std::make_unique<Done>(conductor);
          },
          [&](auto const& x) -> std::optional<std::unique_ptr<State>> {
            LOG_PREGEL_CONDUCTOR_STATE("a698e", ERR)
                << "Received unexpected message type";
            return std::make_unique<FatalError>(conductor);
          },
      },
      message);
}

auto Storing::cancel() -> std::optional<std::unique_ptr<State>> {
  return std::make_unique<Canceled>(conductor);
}
