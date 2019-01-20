////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_EXECUTION_BLOCK_SHUTDOWNABLE_IMPL_H
#define ARANGOD_AQL_EXECUTION_BLOCK_SHUTDOWNABLE_IMPL_H 1

#include "ExecutionBlockImpl.h"

namespace arangodb {
namespace aql {

class ExecutionNode;
class ExecutionEngine;

template <class Executor>
class ExecutionBlockShutdownableImpl : public ExecutionBlockImpl<Executor> {
 public:
  ExecutionBlockShutdownableImpl(ExecutionEngine* engine, ExecutionNode const* node,
                                 typename Executor::Infos&&);
  ~ExecutionBlockShutdownableImpl();

  /// @brief shutdown, will be called exactly once for the whole query
  /// Special implementation for all Executors that need to implement Shutdown
  /// Most do not, we might be able to move their shutdown logic to a more
  /// central place.
  std::pair<ExecutionState, Result> shutdown(int) override;
};

}  // namespace aql
}  // namespace arangodb
#endif