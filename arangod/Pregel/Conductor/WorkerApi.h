////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Julia Volmer
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Cluster/ClusterTypes.h"
#include "Futures/Future.h"
#include "Pregel/ExecutionNumber.h"
#include "Pregel/WorkerConductorMessages.h"
#include "Pregel/WorkerInterface.h"
#include "Utils/DatabaseGuard.h"
#include "Pregel/Connection/Connection.h"

namespace arangodb::pregel::conductor {

struct WorkerApi : NewIWorker {
  WorkerApi() = default;
  WorkerApi(ServerID server, ExecutionNumber executionNumber,
            std::unique_ptr<Connection> connection)
      : _server{std::move(server)},
        _executionNumber{std::move(executionNumber)},
        _connection{std::move(connection)} {}
  [[nodiscard]] auto loadGraph(LoadGraph const& graph)
      -> futures::Future<ResultT<GraphLoaded>> override;
  [[nodiscard]] auto prepareGlobalSuperStep(PrepareGlobalSuperStep const& data)
      -> futures::Future<ResultT<GlobalSuperStepPrepared>> override;
  [[nodiscard]] auto runGlobalSuperStep(RunGlobalSuperStep const& data)
      -> futures::Future<ResultT<GlobalSuperStepFinished>> override;
  [[nodiscard]] auto store(Store const& message)
      -> futures::Future<ResultT<Stored>> override;
  [[nodiscard]] auto cleanup(Cleanup const& message)
      -> futures::Future<ResultT<CleanupFinished>> override;
  [[nodiscard]] auto results(CollectPregelResults const& message) const
      -> futures::Future<ResultT<PregelResults>> override;

 private:
  ServerID _server;
  ExecutionNumber _executionNumber;
  std::unique_ptr<Connection> _connection;

  template<typename Out, typename In>
  auto execute(In const& in) const -> futures::Future<ResultT<Out>>;
};

}  // namespace arangodb::pregel::conductor
