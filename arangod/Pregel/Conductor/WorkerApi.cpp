#include "WorkerApi.h"
#include <variant>

#include "Futures/Utilities.h"
#include "Pregel/Connection/Connection.h"
#include "Pregel/Messaging/Aggregate.h"
#include "Pregel/Messaging/WorkerMessages.h"
#include "Pregel/Messaging/ConductorMessages.h"

using namespace arangodb::pregel::conductor;

template<Addable Out, typename In>
auto WorkerApi::sendToAll_old(In const& in) const
    -> futures::Future<ResultT<Out>> {
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

template<typename In>
auto WorkerApi::sendToAll(In const& in) const
    -> std::vector<futures::Future<Result>> {
  auto results = std::vector<futures::Future<Result>>{};
  for (auto&& server : _servers) {
    results.emplace_back(_connection->post(
        Destination{Destination::Type::server, server},
        ModernMessage{.executionNumber = _executionNumber, .payload = {in}}));
  }
  return results;
}

template<Addable Out>
auto WorkerApi::collectAllOks(std::vector<futures::Future<Result>> responses)
    -> ResultT<Aggregate<Out>> {
  return futures::collectAll(responses)
      .thenValue([](auto responses) -> ResultT<Aggregate<Out>> {
        for (auto const& response : responses) {
          if (response.get().fail()) {
            return Result{
                response.get().errorNumber(),
                fmt::format("Got unsuccessful response from worker: {}",
                            response.get().errorMessage())};
          }
        }
        return Aggregate<Out>::withComponentsCount(responses.size());
      })
      .get();
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
    -> ResultT<Aggregate<GraphLoaded>> {
  return collectAllOks<GraphLoaded>(sendToAll(data));
}

auto WorkerApi::runGlobalSuperStep(
    RunGlobalSuperStep const& data,
    std::unordered_map<ServerID, uint64_t> const& sendCountPerServer)
    -> ResultT<Aggregate<GlobalSuperStepFinished>> {
  auto results = std::vector<futures::Future<Result>>{};
  for (auto&& server : _servers) {
    results.emplace_back(_connection->post(
        Destination{Destination::Type::server, server},
        ModernMessage{.executionNumber = _executionNumber,
                      .payload = RunGlobalSuperStep{
                          .gss = data.gss,
                          .vertexCount = data.vertexCount,
                          .edgeCount = data.edgeCount,
                          .sendCount = sendCountPerServer.contains(server)
                                           ? sendCountPerServer.at(server)
                                           : data.sendCount,
                          .aggregators = data.aggregators}}));
  }
  return collectAllOks<GlobalSuperStepFinished>(std::move(results));
}

auto WorkerApi::store(Store const& message)
    -> futures::Future<ResultT<Stored>> {
  return sendToAll_old<Stored>(message);
}

auto WorkerApi::cleanup(Cleanup const& message)
    -> futures::Future<ResultT<CleanupFinished>> {
  return sendToAll_old<CleanupFinished>(message);
}

auto WorkerApi::results(CollectPregelResults const& message) const
    -> futures::Future<ResultT<PregelResults>> {
  return sendToAll_old<PregelResults>(message);
}
