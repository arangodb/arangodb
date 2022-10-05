#include "WorkerApi.h"
#include <variant>

#include "Futures/Utilities.h"
#include "Pregel/Connection/Connection.h"
#include "Pregel/Messaging/WorkerMessages.h"
#include "Pregel/Messaging/ConductorMessages.h"

using namespace arangodb::pregel::conductor;

template<Addable Out, typename In>
auto WorkerApi::sendToAll(In const& in) const -> futures::Future<ResultT<Out>> {
  auto results = std::vector<futures::Future<ResultT<Out>>>{};
  for (auto&& server : _servers) {
    results.emplace_back(send<Out>(server, in));
  }
  return futures::collectAll(results).thenValue(
      [](auto responses) -> ResultT<Out> {
        auto out = Out{};
        for (auto const& response : responses) {
          if (response.get().fail()) {
            return Result{
                response.get().errorNumber(),
                fmt::format("Got unsuccessful response from worker: {}",
                            response.get().errorMessage())};
          }
          out.add(response.get().get());
        }
        return out;
      });
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
    -> futures::Future<ResultT<GraphLoaded>> {
  return sendToAll<GraphLoaded>(data);
}

auto WorkerApi::prepareGlobalSuperStep(PrepareGlobalSuperStep const& data)
    -> futures::Future<ResultT<GlobalSuperStepPrepared>> {
  return sendToAll<GlobalSuperStepPrepared>(data);
}

auto WorkerApi::runGlobalSuperStep(RunGlobalSuperStep const& data)
    -> futures::Future<ResultT<GlobalSuperStepFinished>> {
  return sendToAll<GlobalSuperStepFinished>(data);
}

auto WorkerApi::store(Store const& message)
    -> futures::Future<ResultT<Stored>> {
  return sendToAll<Stored>(message);
}

auto WorkerApi::cleanup(Cleanup const& message)
    -> futures::Future<ResultT<CleanupFinished>> {
  return sendToAll<CleanupFinished>(message);
}

auto WorkerApi::results(CollectPregelResults const& message) const
    -> futures::Future<ResultT<PregelResults>> {
  return sendToAll<PregelResults>(message);
}
