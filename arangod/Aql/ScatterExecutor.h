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

#ifndef ARANGOD_AQL_SCATTER_EXECUTOR_H
#define ARANGOD_AQL_SCATTER_EXECUTOR_H

#include "Aql/BlocksWithClients.h"
#include "Aql/ClusterNodes.h"
#include "Aql/ExecutionBlockImpl.h"

namespace arangodb {
namespace aql {

// The ScatterBlock is actually implemented by specializing ExecutionBlockImpl,
// so this class only exists to identify the specialization.
class ScatterExecutor {};

/**
 * @brief See ExecutionBlockImpl.h for documentation.
 */
template <>
class ExecutionBlockImpl<ScatterExecutor> : public BlocksWithClients {
 public:
  // TODO Even if it's not strictly necessary here, for consistency's sake the
  // non-standard argument (shardIds) should probably be moved into some
  // ScatterExecutorInfos class.
  ExecutionBlockImpl(ExecutionEngine* engine, ScatterNode const* node,
                     ExecutorInfos&& infos, std::vector<std::string> const& shardIds);

  ~ExecutionBlockImpl() override = default;

  std::pair<ExecutionState, Result> initializeCursor(InputAqlItemRow const& input) override;

  /// @brief getSomeForShard
  std::pair<ExecutionState, std::unique_ptr<AqlItemBlock>> getSomeForShard(
      size_t atMost, std::string const& shardId) override;

  /// @brief skipSomeForShard
  std::pair<ExecutionState, size_t> skipSomeForShard(size_t atMost,
                                                     std::string const& shardId) override;

 private:
  /// @brief getSomeForShard
  std::pair<ExecutionState, std::unique_ptr<AqlItemBlock>> getSomeForShardWithoutTrace(
      size_t atMost, std::string const& shardId);

  /// @brief skipSomeForShard
  std::pair<ExecutionState, size_t> skipSomeForShardWithoutTrace(size_t atMost,
                                                                 std::string const& shardId);

  std::pair<ExecutionState, arangodb::Result> getOrSkipSomeForShard(
      size_t atMost, bool skipping, std::unique_ptr<AqlItemBlock>& result,
      size_t& skipped, std::string const& shardId);

  bool hasMoreForClientId(size_t clientId) const;

  /// @brief getHasMoreStateForClientId: State for client <clientId>?
  ExecutionState getHasMoreStateForClientId(size_t clientId) const;

  ExecutorInfos const& infos() const { return _infos; }

 private:
  ExecutorInfos _infos;

  Query const& _query;

  /// @brief _posForClient:
  std::vector<std::pair<size_t, size_t>> _posForClient;
};

}  // namespace aql
}  // namespace arangodb

#endif  // ARANGOD_AQL_SCATTER_EXECUTOR_H
