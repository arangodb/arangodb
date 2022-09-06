#include "ClusterWorkerApi.h"

#include "Pregel/WorkerConductorMessages.h"
#include "Pregel/NetworkConnection.h"

using namespace arangodb::pregel::conductor;

auto ClusterWorkerApi::loadGraph(LoadGraph const& graph)
    -> futures::Future<ResultT<GraphLoaded>> {
  return _connection.post<GraphLoaded>(graph);
}

auto ClusterWorkerApi::prepareGlobalSuperStep(
    PrepareGlobalSuperStep const& data)
    -> futures::Future<ResultT<GlobalSuperStepPrepared>> {
  return _connection.post<GlobalSuperStepPrepared>(data);
}

auto ClusterWorkerApi::runGlobalSuperStep(RunGlobalSuperStep const& data)
    -> futures::Future<ResultT<GlobalSuperStepFinished>> {
  return _connection.post<GlobalSuperStepFinished>(data);
}
