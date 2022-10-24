#include "StoringState.h"

#include "Pregel/Conductor/Conductor.h"
#include "Metrics/Gauge.h"
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

Storing::Storing(Conductor& conductor, WorkerApi<Stored>&& workerApi)
    : conductor{conductor}, _workerApi{std::move(workerApi)} {
  conductor._timing.storing.start();
  conductor._feature.metrics()->pregelConductorsStoringNumber->fetch_add(1);
}

Storing::~Storing() {
  conductor._timing.storing.finish();
  conductor._feature.metrics()->pregelConductorsStoringNumber->fetch_sub(1);
}

auto Storing::run() -> std::optional<std::unique_ptr<State>> {
  auto sent = _workerApi.sendToAll(Store{});
  if (sent.fail()) {
    LOG_PREGEL_CONDUCTOR_STATE("e2dad", ERR) << sent.errorMessage();
    return std::make_unique<FatalError>(conductor, std::move(_workerApi));
  }
  return std::nullopt;
}

auto Storing::receive(MessagePayload message)
    -> std::optional<std::unique_ptr<State>> {
  auto explicitMessage = getResultTMessage<Stored>(message);
  if (explicitMessage.fail()) {
    LOG_PREGEL_CONDUCTOR_STATE("dde4a", ERR) << explicitMessage.errorMessage();
    return std::make_unique<FatalError>(conductor, std::move(_workerApi));
  }
  auto aggregatedMessage = _workerApi.collect(explicitMessage.get());
  if (!aggregatedMessage.has_value()) {
    return std::nullopt;
  }

  // workers deleted themselves after storing
  _workerApi._servers = {};

  return std::make_unique<Done>(conductor, std::move(_workerApi));
}

auto Storing::cancel() -> std::optional<std::unique_ptr<State>> {
  // TODO no defined which api to use here
  return std::make_unique<Canceled>(conductor, std::move(_workerApi));
}
