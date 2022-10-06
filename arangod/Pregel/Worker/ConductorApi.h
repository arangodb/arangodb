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

#include "Pregel/Connection/Connection.h"
#include "Pregel/ExecutionNumber.h"
#include "Pregel/Messaging/WorkerMessages.h"

namespace arangodb::pregel::worker {

struct ConductorApi {
  ConductorApi() = default;
  ConductorApi(ServerID conductorServer, ExecutionNumber executionNumber,
               std::unique_ptr<Connection> connection)
      : _server{std::move(conductorServer)},
        _executionNumber{std::move(executionNumber)},
        _connection{std::move(connection)} {}
  auto graphLoaded(ResultT<GraphLoaded> const& data) const -> Result;

 private:
  ServerID _server;
  ExecutionNumber _executionNumber;
  std::unique_ptr<Connection> _connection;
};

}  // namespace arangodb::pregel::worker
