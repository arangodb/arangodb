#include "Cluster/ClusterTypes.h"
#include "Futures/Future.h"
#include "Pregel/ExecutionNumber.h"
#include "Pregel/WorkerConductorMessages.h"
#include "Pregel/WorkerInterface.h"
#include "Utils/DatabaseGuard.h"
#include "Pregel/NetworkConnection.h"

namespace arangodb::pregel::conductor {

struct ClusterWorkerApi : NewIWorker {
  ClusterWorkerApi(ServerID server, ExecutionNumber executionNumber,
                   Connection network)
      : _server{std::move(server)},
        _executionNumber{std::move(executionNumber)},
        _connection{std::move(network)} {}
  [[nodiscard]] auto loadGraph(LoadGraph const& graph)
      -> futures::Future<ResultT<GraphLoaded>> override;
  [[nodiscard]] auto prepareGlobalSuperStep(PrepareGlobalSuperStep const& data)
      -> futures::Future<ResultT<GlobalSuperStepPrepared>> override;
  [[nodiscard]] auto runGlobalSuperStep(RunGlobalSuperStep const& data)
      -> futures::Future<ResultT<GlobalSuperStepFinished>> override;

 private:
  ServerID _server;
  ExecutionNumber _executionNumber;
  Connection _connection;

  template<typename Out, typename In>
  auto execute(In const& in) -> futures::Future<ResultT<Out>>;
};

}  // namespace arangodb::pregel::conductor
