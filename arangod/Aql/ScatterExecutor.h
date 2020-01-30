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
#include "Aql/ExecutorInfos.h"

namespace arangodb {
namespace aql {

// The ScatterBlock is actually implemented by specializing ExecutionBlockImpl,
// so this class only exists to identify the specialization.
class ScatterExecutor {
 public:
  class ClientBlockData {
   public:
    auto clear() -> void;
    auto addBlock(SharedAqlItemBlockPtr block) -> void;
    auto hasDataFor(AqlCall const& call) -> bool;

    auto execute(AqlCall call, ExecutionState upstreamState)
        -> std::tuple<ExecutionState, size_t, SharedAqlItemBlockPtr>;

   private:
    auto firstBlockRows() -> size_t;
    auto dropBlock() -> void;

   private:
    std::deque<SharedAqlItemBlockPtr> _queue;
    size_t _firstBlockPos{0};
  };

  ScatterExecutor();
  ~ScatterExecutor() = default;

  auto distributeBlock(SharedAqlItemBlockPtr block,
                       std::unordered_map<std::string, ClientBlockData>& blockMap) const
      -> void;
};

/**
 * @brief See ExecutionBlockImpl.h for documentation.
 */
template <>
class ExecutionBlockImpl<ScatterExecutor> : public BlocksWithClientsImpl<ScatterExecutor> {
 public:
  // TODO Even if it's not strictly necessary here, for consistency's sake the
  // non-standard argument (shardIds) should probably be moved into some
  // ScatterExecutorInfos class.
  ExecutionBlockImpl(ExecutionEngine* engine, ScatterNode const* node,
                     ExecutorInfos&& infos, std::vector<std::string> const& shardIds);

  ~ExecutionBlockImpl() override = default;

  /// @brief getSomeForShard
  std::pair<ExecutionState, SharedAqlItemBlockPtr> getSomeForShard(size_t atMost,
                                                                   std::string const& shardId) override;

  /// @brief skipSomeForShard
  std::pair<ExecutionState, size_t> skipSomeForShard(size_t atMost,
                                                     std::string const& shardId) override;

 private:
  /// @brief getSomeForShard
  std::pair<ExecutionState, SharedAqlItemBlockPtr> getSomeForShardWithoutTrace(
      size_t atMost, std::string const& shardId);

  /// @brief skipSomeForShard
  std::pair<ExecutionState, size_t> skipSomeForShardWithoutTrace(size_t atMost,
                                                                 std::string const& shardId);

  std::pair<ExecutionState, arangodb::Result> getOrSkipSomeForShard(
      size_t atMost, bool skipping, SharedAqlItemBlockPtr& result,
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
