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

template<typename T>
using FutureOfWorkerResults =
    futures::Future<std::vector<futures::Try<ResultT<T>>>>;

struct WorkerApi {
  WorkerApi() = default;
  WorkerApi(std::vector<ServerID> servers, ExecutionNumber executionNumber,
            std::unique_ptr<Connection> connection)
      : _servers{std::move(servers)},
        _executionNumber{std::move(executionNumber)},
        _connection{std::move(connection)} {}
  [[nodiscard]] auto createWorkers(
      std::unordered_map<ServerID, CreateWorker> const& data)
      -> futures::Future<ResultT<WorkerCreated>>;
  [[nodiscard]] auto loadGraph(LoadGraph const& graph)
      -> FutureOfWorkerResults<GraphLoaded>;
  [[nodiscard]] auto prepareGlobalSuperStep(PrepareGlobalSuperStep const& data)
      -> FutureOfWorkerResults<GlobalSuperStepPrepared>;
  [[nodiscard]] auto runGlobalSuperStep(RunGlobalSuperStep const& data)
      -> FutureOfWorkerResults<GlobalSuperStepFinished>;
  [[nodiscard]] auto store(Store const& message)
      -> FutureOfWorkerResults<Stored>;
  [[nodiscard]] auto cleanup(Cleanup const& message)
      -> FutureOfWorkerResults<CleanupFinished>;
  [[nodiscard]] auto results(CollectPregelResults const& message) const
      -> FutureOfWorkerResults<PregelResults>;

 private:
  std::vector<ServerID> _servers;
  ExecutionNumber _executionNumber;
  std::unique_ptr<Connection> _connection;

  // TODO accumulate results inside and return Future<ResultT<Out>> directly.
  // This can be done when AggregatorHandler, StatsManager and ReportManager can
  // be accumulated propertly
  template<typename Out, typename In>
  auto sendToAll(In const& in) const -> FutureOfWorkerResults<Out>;

  // This template enforces In and Out type of the function:
  // 'In' enforces which type of message is sent
  // 'Out' defines the expected response type:
  // The function returns an error if this expectation is not fulfilled
  template<typename Out, typename In>
  auto send(ServerID const& server, In const& in) const
      -> futures::Future<ResultT<Out>>;
};

}  // namespace arangodb::pregel::conductor
