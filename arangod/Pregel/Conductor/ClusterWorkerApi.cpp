#include "ClusterWorkerApi.h"

#include "Pregel/WorkerConductorMessages.h"
#include "Pregel/NetworkConnection.h"

using namespace arangodb::pregel::conductor;

[[nodiscard]] auto ClusterWorkerApi::loadGraph(LoadGraph const& graph)
    -> futures::Future<ResultT<GraphLoaded>> {
  return _connection.post<GraphLoaded>(graph);
}

[[nodiscard]] auto ClusterWorkerApi::prepareGlobalSuperStep(
    PrepareGlobalSuperStep const& data)
    -> futures::Future<ResultT<GlobalSuperStepPrepared>> {
  return _connection.post<GlobalSuperStepPrepared>(data);
}
