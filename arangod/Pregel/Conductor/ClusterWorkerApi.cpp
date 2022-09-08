#include "ClusterWorkerApi.h"
#include <variant>

#include "Pregel/WorkerConductorMessages.h"
#include "Pregel/NetworkConnection.h"

using namespace arangodb::pregel::conductor;

// This template enforces In and Out type of the function:
// In enforces which type of message is sent
// Out defines the expected response type:
// The function returns an error if this expectation is not fulfilled
template<typename Out, typename In>
auto ClusterWorkerApi::execute(In const& in) -> futures::Future<ResultT<Out>> {
  return _connection
      .post(ModernMessage{.executionNumber = _executionNumber, .payload = {in}})
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

auto ClusterWorkerApi::loadGraph(LoadGraph const& graph)
    -> futures::Future<ResultT<GraphLoaded>> {
  return execute<GraphLoaded>(graph);
}

auto ClusterWorkerApi::prepareGlobalSuperStep(
    PrepareGlobalSuperStep const& data)
    -> futures::Future<ResultT<GlobalSuperStepPrepared>> {
  return execute<GlobalSuperStepPrepared>(data);
}

auto ClusterWorkerApi::runGlobalSuperStep(RunGlobalSuperStep const& data)
    -> futures::Future<ResultT<GlobalSuperStepFinished>> {
  return execute<GlobalSuperStepFinished>(data);
}
