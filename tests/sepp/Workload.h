////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include "StoppingCriterion.h"

namespace arangodb::sepp {

struct Server;
struct Execution;
struct ExecutionThread;

struct Workload {
  virtual ~Workload() = default;

  using WorkerThreadList = std::vector<std::unique_ptr<ExecutionThread>>;
  virtual WorkerThreadList createThreads(Execution& exec, Server& server) = 0;

  virtual StoppingCriterion::type stoppingCriterion() const noexcept = 0;
};
}  // namespace arangodb::sepp