#include "SingleServerWorkerApi.h"

#include "Pregel/AlgoRegistry.h"
#include "Pregel/PregelFeature.h"
#include "Pregel/Worker/Worker.h"  // TODO include WorkerInterface instead when future refactoring is finished
#include "velocypack/Builder.h"

using namespace arangodb::pregel::conductor;

[[nodiscard]] auto SingleServerWorkerApi::loadGraph(LoadGraph const& graph)
    -> futures::Future<ResultT<GraphLoaded>> {
  if (_feature.isStopping()) {
    return Result{TRI_ERROR_SHUTTING_DOWN};
  }

  std::shared_ptr<IWorker> worker = _feature.worker(_executionNumber);
  if (worker) {
    return Result{TRI_ERROR_INTERNAL,
                  "a worker with this execution number already exists."};
  }

  try {
    auto created = AlgoRegistry::createWorker(_vocbaseGuard.database(),
                                              graph.details.slice(), _feature);
    TRI_ASSERT(created.get() != nullptr);
    _feature.addWorker(std::move(created), _executionNumber);
    worker = _feature.worker(_executionNumber);
    TRI_ASSERT(worker);
    return worker->loadGraph(graph);
  } catch (basics::Exception& e) {
    return Result{e.code(), e.message()};
  }
}

[[nodiscard]] auto SingleServerWorkerApi::prepareGlobalSuperStep(
    PrepareGlobalSuperStep const& message)
    -> futures::Future<ResultT<GlobalSuperStepPrepared>> {
  VPackBuilder out;
  auto worker = _feature.worker(_executionNumber);
  return worker->prepareGlobalSuperStep(message);
}
