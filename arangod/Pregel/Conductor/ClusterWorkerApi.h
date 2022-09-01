#include "Cluster/ClusterTypes.h"
#include "Futures/Future.h"
#include "Pregel/ExecutionNumber.h"
#include "Pregel/WorkerConductorMessages.h"
#include "Pregel/WorkerInterface.h"
#include "Utils/DatabaseGuard.h"
#include "Pregel/NetworkConnection.h"

namespace arangodb::pregel::conductor {

struct ClusterWorkerApi : NewIWorker {
  ClusterWorkerApi(Connection network) : _connection{std::move(network)} {}
  [[nodiscard]] auto loadGraph(LoadGraph const& graph)
      -> futures::Future<ResultT<GraphLoaded>> override;
  [[nodiscard]] auto prepareGlobalSuperStep(PrepareGlobalSuperStep const& data)
      -> futures::Future<ResultT<GlobalSuperStepPrepared>> override;
  [[nodiscard]] auto runGlobalSuperStep(RunGlobalSuperStep const& data)
      -> futures::Future<ResultT<GlobalSuperStepFinished>> override;

 private:
  Connection _connection;
};

}  // namespace arangodb::pregel::conductor
