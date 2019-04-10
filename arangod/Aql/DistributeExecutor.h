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

#ifndef ARANGOD_AQL_DISTRIBUTE_EXECUTOR_H
#define ARANGOD_AQL_DISTRIBUTE_EXECUTOR_H

#include "Aql/ClusterBlocks.h"
#include "Aql/ExecutionBlockImpl.h"

namespace arangodb {
namespace aql {

class DistributeNode;

// The DistributeBlock is actually implemented by specializing
// ExecutionBlockImpl, so this class only exists to identify the specialization.
class DistributeExecutor {};

/**
 * @brief See ExecutionBlockImpl.h for documentation.
 */
template <>
class ExecutionBlockImpl<DistributeExecutor> : public BlockWithClients {
 public:
  // TODO Even if it's not strictly necessary here, for consistency's sake the
  // non-standard arguments (shardIds, collection) should probably be moved into
  // some DistributeExecutorInfos class.
  ExecutionBlockImpl(ExecutionEngine* engine, DistributeNode const* node,
                     ExecutorInfos&& infos, std::vector<std::string> const& shardIds,
                     Collection const* collection, RegisterId regId,
                     RegisterId alternativeRegId, bool allowSpecifiedKeys,
                     bool allowKeyConversionToObject, bool createKeys);

  ~ExecutionBlockImpl() override = default;

  std::pair<ExecutionState, Result> initializeCursor(InputAqlItemRow const& input) override;

  /// @brief getSomeForShard
  std::pair<ExecutionState, std::unique_ptr<AqlItemBlock>> getSomeForShard(
      size_t atMost, std::string const& shardId) override;

  /// @brief skipSomeForShard
  std::pair<ExecutionState, size_t> skipSomeForShard(size_t atMost,
                                                     std::string const& shardId) override;

 private:
  std::pair<ExecutionState, std::unique_ptr<AqlItemBlock>> traceGetSomeEnd(
      ExecutionState state, std::unique_ptr<AqlItemBlock> result);

  std::pair<ExecutionState, size_t> traceSkipSomeEnd(ExecutionState state, size_t skipped);

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

  /// @brief hasMoreForShard: any more for shard <shardId>?
  bool hasMoreForShard(std::string const& shardId) const;

  /// @brief getBlockForClient: try to get at atMost pairs into
  /// _distBuffer.at(clientId).
  std::pair<ExecutionState, bool> getBlockForClient(size_t atMost, size_t clientId);

  std::pair<ExecutionState, bool> getBlock(size_t atMost); // TODO: Move to Impl?

  /// @brief sendToClient: for each row of the incoming AqlItemBlock use the
  /// attributes <shardKeys> of the register <id> to determine to which shard
  /// the row should be sent.
  size_t sendToClient(AqlItemBlock*);

  /// @brief create a new document key
  std::string createKey(arangodb::velocypack::Slice) const;

  ExecutorInfos const& infos() const { return _infos; }

 private:
  ExecutorInfos _infos;

  Query const& _query;

  /// @brief _distBuffer.at(i) is a deque containing pairs (j,k) such that
  //  _buffer.at(j) row k should be sent to the client with id = i.
  std::vector<std::deque<std::pair<size_t, size_t>>> _distBuffer;

  // a reusable Builder object for building _key values
  arangodb::velocypack::Builder _keyBuilder;

  // a reusable Builder object for building document objects
  arangodb::velocypack::Builder _objectBuilder;

  /// @brief _colectionName: the name of the sharded collection
  Collection const* _collection;

  /// @brief _index: the block in _buffer we are currently considering
  size_t _index;

  /// @brief _regId: the register to inspect
  RegisterId _regId;

  /// @brief a second register to inspect (used only for UPSERT nodes at the
  /// moment to distinguish between search and insert)
  RegisterId _alternativeRegId;

  /// @brief whether or not the collection uses the default sharding
  bool _usesDefaultSharding;

  /// @brief allow specified keys even in non-default sharding case
  bool _allowSpecifiedKeys;

  bool _allowKeyConversionToObject;

  bool _createKeys;

  /// @brief the execution state of the dependency
  ///        used to determine HASMORE or DONE better
  ExecutionState _upstreamState;

};

}  // namespace aql
}  // namespace arangodb

#endif  // ARANGOD_AQL_DISTRIBUTE_EXECUTOR_H
