////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_REMOTE_EXECUTOR_H
#define ARANGOD_AQL_REMOTE_EXECUTOR_H

#include "Aql/ExecutionBlockImpl.h"

namespace arangodb {
namespace aql {

// TODO Can the class definitions be moved into the .cpp file, with only forward
// declarations left here, if we provide a wrapper around
// std::make_unique<ExecutionBlockImpl<RemoteExecutor>>?

class RemoteExecutor final {};

/**
 * @brief See ExecutionBlockImpl.h for documentation.
 */
template <>
class ::arangodb::aql::ExecutionBlockImpl<RemoteExecutor> final : public ExecutionBlock {
 public:
  ExecutionBlockImpl(ExecutionEngine* engine, ExecutionNode const* node,
                     ExecutorInfos&& infos);

  ~ExecutionBlockImpl() = default;

  std::pair<ExecutionState, std::unique_ptr<AqlItemBlock>> getSome(size_t atMost) override;

  std::pair<ExecutionState, size_t> skipSome(size_t atMost) override;

  std::pair<ExecutionState, Result> initializeCursor(AqlItemBlock* items, size_t pos) override;

  std::pair<ExecutionState, Result> shutdown(int) override;

 private:
  std::pair<ExecutionState, std::unique_ptr<AqlItemBlock>> traceGetSomeEnd(
      ExecutionState state, std::unique_ptr<AqlItemBlock> result);

  std::pair<ExecutionState, size_t> traceSkipSomeEnd(ExecutionState state, size_t skipped);

  std::pair<ExecutionState, std::unique_ptr<AqlItemBlock>> getSomeWithoutTrace(size_t atMost);

  ExecutorInfos const& infos() const { return _infos; }

  Query const& getQuery() const { return _query; }

 private:
  ExecutorInfos _infos;

  Query const& _query;
};
}  // namespace aql
}  // namespace arangodb

#endif  // ARANGOD_AQL_REMOTE_EXECUTOR_H
