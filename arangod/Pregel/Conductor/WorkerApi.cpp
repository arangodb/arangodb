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
auto WorkerApi::execute(In const& in) const -> futures::Future<ResultT<Out>> {
  return _connection
      ->send(
          Destination{Destination::Type::server, _server},
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

auto WorkerApi::loadGraph(LoadGraph const& graph)
    -> futures::Future<ResultT<GraphLoaded>> {
  return execute<GraphLoaded>(graph);
}

auto WorkerApi::prepareGlobalSuperStep(PrepareGlobalSuperStep const& data)
    -> futures::Future<ResultT<GlobalSuperStepPrepared>> {
  return execute<GlobalSuperStepPrepared>(data);
}

auto WorkerApi::runGlobalSuperStep(RunGlobalSuperStep const& data)
    -> futures::Future<ResultT<GlobalSuperStepFinished>> {
  return execute<GlobalSuperStepFinished>(data);
}

auto WorkerApi::store(Store const& message)
    -> futures::Future<ResultT<Stored>> {
  return execute<Stored>(message);
}

auto WorkerApi::cleanup(Cleanup const& message)
    -> futures::Future<ResultT<CleanupFinished>> {
  return execute<CleanupFinished>(message);
}

auto WorkerApi::results(CollectPregelResults const& message) const
    -> futures::Future<ResultT<PregelResults>> {
  return execute<PregelResults>(message);
}
