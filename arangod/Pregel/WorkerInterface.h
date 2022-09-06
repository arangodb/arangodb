#pragma once

#include "Pregel/WorkerConductorMessages.h"

#include <Futures/Future.h>

namespace arangodb::pregel {

class NewIWorker {
 public:
  virtual ~NewIWorker() = default;
  [[nodiscard]] virtual auto loadGraph(LoadGraph const& graph)
      -> futures::Future<ResultT<GraphLoaded>> = 0;
  [[nodiscard]] virtual auto prepareGlobalSuperStep(
      PrepareGlobalSuperStep const& data)
      -> futures::Future<ResultT<GlobalSuperStepPrepared>> = 0;
};

}  // namespace arangodb::pregel
