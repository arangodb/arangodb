////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_MUTEX_EXECUTOR_H
#define ARANGOD_AQL_MUTEX_EXECUTOR_H

#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionState.h"
#include "Aql/SharedAqlItemBlockPtr.h"
#include "Aql/SingleRowFetcher.h"

#include <mutex>
#include <tuple>
#include <utility>

namespace arangodb {
namespace aql {

class MutexNode;

// MutexExecutor is actually implemented by specializing ExecutionBlockImpl,
// so this class only exists to identify the specialization.
class MutexExecutor final {};

/**
 * @brief See ExecutionBlockImpl.h for documentation.
 */
template <>
class ExecutionBlockImpl<MutexExecutor> : public ExecutionBlock {
 public:
  ExecutionBlockImpl(ExecutionEngine* engine, MutexNode const* node);

  ~ExecutionBlockImpl() override = default;
  
  std::tuple<ExecutionState, SkipResult, SharedAqlItemBlockPtr> execute(AqlCallStack stack) override;

  std::pair<ExecutionState, Result> initializeCursor(InputAqlItemRow const& input) override;

  std::pair<ExecutionState, Result> shutdown(int errorCode) override;

 private:
  std::pair<ExecutionState, SharedAqlItemBlockPtr> getSomeWithoutTrace(size_t atMost);

  std::pair<ExecutionState, size_t> skipSomeWithoutTrace(size_t atMost);


 private:
  std::mutex _mutex;
  bool _isShutdown;
};


}  // namespace aql
}  // namespace arangodb

#endif
