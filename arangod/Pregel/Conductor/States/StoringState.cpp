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

  auto aggregate = _workerApi.aggregateAllResponses();
  if (aggregate.fail()) {
    LOG_PREGEL_CONDUCTOR_STATE("00251", ERR) << aggregate.errorMessage();
    return std::make_unique<FatalError>(conductor, std::move(_workerApi));
  }

  // workers deleted themselves after storing
  _workerApi._servers = {};
  return std::make_unique<Done>(conductor, std::move(_workerApi));
}

auto Storing::receive(MessagePayload message) -> void {
  auto queued = _workerApi.queue(message);
  if (queued.fail()) {
    LOG_PREGEL_CONDUCTOR_STATE("7698e", ERR) << queued.errorMessage();
  }
}

auto Storing::cancel() -> std::optional<std::unique_ptr<State>> {
  return std::make_unique<Canceled>(conductor, std::move(_workerApi));
}
