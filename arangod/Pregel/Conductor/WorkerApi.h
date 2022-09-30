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
#include "Pregel/Messaging/WorkerMessages.h"
#include "Pregel/Messaging/ConductorMessages.h"
#include "Pregel/WorkerInterface.h"
#include "Utils/DatabaseGuard.h"
#include "Pregel/Connection/Connection.h"

namespace arangodb::pregel::conductor {

template<typename T>
concept Addable = requires(T a, T b) {
  {a.add(b)};
};

struct WorkerApi : NewIWorker {
  WorkerApi() = default;
  WorkerApi(ExecutionNumber executionNumber,
            std::unique_ptr<Connection> connection)
      : _executionNumber{std::move(executionNumber)},
        _connection{std::move(connection)} {}
  [[nodiscard]] auto createWorkers(
      std::unordered_map<ServerID, CreateWorker> const& data)
      -> futures::Future<Result>;
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
  std::vector<ServerID> _servers = {};
  ExecutionNumber _executionNumber;
  std::unique_ptr<Connection> _connection;

  template<Addable Out, typename In>
  auto sendToAll(In const& in) const -> futures::Future<ResultT<Out>>;

  // This template enforces In and Out type of the function:
  // 'In' enforces which type of message is sent
  // 'Out' defines the expected response type:
  // The function returns an error if this expectation is not fulfilled
  template<typename Out, typename In>
  auto send(ServerID const& server, In const& in) const
      -> futures::Future<ResultT<Out>>;
};

}  // namespace arangodb::pregel::conductor
