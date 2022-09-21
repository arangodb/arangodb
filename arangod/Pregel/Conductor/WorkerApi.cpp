#include "WorkerApi.h"
#include <variant>

#include "Pregel/WorkerConductorMessages.h"
#include "Pregel/Connection/Connection.h"

using namespace arangodb::pregel::conductor;

// This template enforces In and Out type of the function:
// 'In' enforces which type of message is sent
// 'Out' defines the expected response type:
// The function returns an error if this expectation is not fulfilled
template<typename Out, typename In>
auto WorkerApi::sendToAll(In const& in) const -> FutureOfWorkerResults<Out> {
  auto results = std::vector<futures::Future<ResultT<Out>>>{};
  for (auto&& server : _servers) {
    results.emplace_back(send<Out>(server, in));
  }
  return futures::collectAll(results);
}

template<typename Out, typename In>
auto WorkerApi::send(ServerID const& server, In const& in) const
    -> futures::Future<ResultT<Out>> {
  return _connection
      ->send(
          Destination{Destination::Type::server, server},
          ModernMessage{.executionNumber = _executionNumber, .payload = {in}})
      .thenValue([](auto&& response) -> futures::Future<ResultT<Out>> {
        if (response.fail()) {
          return Result{response.errorNumber(), response.errorMessage()};
        }
        if (!std::holds_alternative<ResultT<Out>>(response.get().payload)) {
          return Result{
              TRI_ERROR_INTERNAL,
              fmt::format(
                  "Message from worker does not include the expected {} "
                  "type",
                  typeid(Out).name())};
        }
        return std::get<ResultT<Out>>(response.get().payload);
      });
}

auto WorkerApi::createWorkers(
    std::unordered_map<ServerID, CreateWorker> const& data)
    -> futures::Future<Result> {
  auto results = std::vector<futures::Future<ResultT<WorkerCreated>>>{};
  for (auto const& [server, message] : data) {
    results.emplace_back(send<WorkerCreated, CreateWorker>(server, message));
  }
  return futures::collectAll(results).thenValue([&](auto results) -> Result {
    for (auto const& result : results) {
      if (result.get().fail()) {
        return Result{
            result.get().errorNumber(),
            fmt::format("Got unsuccessful response while creating worker: {}",
                        result.get().errorMessage())};
      }
      _servers.emplace_back(result.get().get().senderId);
    }
    return Result{};
  });
}

auto WorkerApi::loadGraph(LoadGraph const& data)
    -> FutureOfWorkerResults<GraphLoaded> {
  return sendToAll<GraphLoaded>(data);
}

auto WorkerApi::prepareGlobalSuperStep(PrepareGlobalSuperStep const& data)
    -> FutureOfWorkerResults<GlobalSuperStepPrepared> {
  return sendToAll<GlobalSuperStepPrepared>(data);
}

auto WorkerApi::runGlobalSuperStep(RunGlobalSuperStep const& data)
    -> FutureOfWorkerResults<GlobalSuperStepFinished> {
  return sendToAll<GlobalSuperStepFinished>(data);
}

auto WorkerApi::store(Store const& message) -> FutureOfWorkerResults<Stored> {
  return sendToAll<Stored>(message);
}

auto WorkerApi::cleanup(Cleanup const& message)
    -> FutureOfWorkerResults<CleanupFinished> {
  return sendToAll<CleanupFinished>(message);
}

auto WorkerApi::results(CollectPregelResults const& message) const
    -> FutureOfWorkerResults<PregelResults> {
  return sendToAll<PregelResults>(message);
}
