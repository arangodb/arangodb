#include "Cluster/ClusterTypes.h"
#include "Futures/Future.h"
#include "Pregel/ExecutionNumber.h"
#include "Pregel/WorkerConductorMessages.h"
#include "Pregel/WorkerInterface.h"
#include "Utils/DatabaseGuard.h"

struct TRI_vocbase_t;

namespace arangodb::pregel::conductor {

struct SingleServerWorkerApi : NewIWorker {
  SingleServerWorkerApi(ExecutionNumber executionNumber, PregelFeature& feature,
                        TRI_vocbase_t& vocbase)
      : _executionNumber{std::move(executionNumber)},
        _feature{feature},
        _vocbaseGuard{vocbase} {}
  [[nodiscard]] auto loadGraph(LoadGraph const& graph)
      -> futures::Future<ResultT<GraphLoaded>> override;
  [[nodiscard]] auto prepareGlobalSuperStep(PrepareGlobalSuperStep const& data)
      -> futures::Future<ResultT<GlobalSuperStepPrepared>> override;

 private:
  ExecutionNumber _executionNumber;
  PregelFeature& _feature;
  const DatabaseGuard _vocbaseGuard;
};

}  // namespace arangodb::pregel::conductor
