#pragma once

#include "Pregel/Messaging/WorkerMessages.h"
#include "Pregel/Messaging/ConductorMessages.h"

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
  [[nodiscard]] virtual auto runGlobalSuperStep(RunGlobalSuperStep const& data)
      -> futures::Future<ResultT<GlobalSuperStepFinished>> = 0;
  [[nodiscard]] virtual auto store(Store const& message)
      -> futures::Future<ResultT<Stored>> = 0;
  [[nodiscard]] virtual auto cleanup(Cleanup const& message)
      -> futures::Future<ResultT<CleanupFinished>> = 0;
  [[nodiscard]] virtual auto results(CollectPregelResults const& message) const
      -> futures::Future<ResultT<PregelResults>> = 0;
};

}  // namespace arangodb::pregel
