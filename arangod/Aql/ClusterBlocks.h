////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_CLUSTER_BLOCKS_H
#define ARANGOD_AQL_CLUSTER_BLOCKS_H 1

#include "Aql/ClusterNodes.h"
#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionNode.h"
#include "Aql/SortRegister.h"
#include "Basics/Common.h"
#include "Cluster/ClusterComm.h"
#include "Rest/GeneralRequest.h"

#include <velocypack/Builder.h>

namespace arangodb {

namespace httpclient {
class SimpleHttpResult;
}

namespace transaction {
class Methods;
}

struct ClusterCommResult;

namespace aql {
class AqlItemBlock;
struct Collection;
class ExecutionEngine;

class BlockWithClients : public ExecutionBlock {
 public:
  BlockWithClients(ExecutionEngine* engine, ExecutionNode const* ep,
                   std::vector<std::string> const& shardIds);

  ~BlockWithClients() override = default;

 public:
  /// @brief initializeCursor
  std::pair<ExecutionState, Result> initializeCursor(AqlItemBlock* items, size_t pos) override;

  /// @brief shutdown
  std::pair<ExecutionState, Result> shutdown(int) override;

  /// @brief getSome: shouldn't be used, use skipSomeForShard
  std::pair<ExecutionState, std::unique_ptr<AqlItemBlock>> getSome(size_t atMost) override final {
    TRI_ASSERT(false);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  /// @brief skipSome: shouldn't be used, use skipSomeForShard
  std::pair<ExecutionState, size_t> skipSome(size_t atMost) override final {
    TRI_ASSERT(false);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  /// @brief getSomeForShard
  virtual std::pair<ExecutionState, std::unique_ptr<AqlItemBlock>> getSomeForShard(
      size_t atMost, std::string const& shardId) = 0;

  /// @brief skipSomeForShard
  virtual std::pair<ExecutionState, size_t> skipSomeForShard(size_t atMost,
                                                             std::string const& shardId) = 0;

 protected:
  /// @brief getClientId: get the number <clientId> (used internally)
  /// corresponding to <shardId>
  size_t getClientId(std::string const& shardId) const;

 protected:
  /// @brief _shardIdMap: map from shardIds to clientNrs
  std::unordered_map<std::string, size_t> _shardIdMap;

  /// @brief _nrClients: total number of clients
  size_t _nrClients;

 private:
  bool _wasShutdown;
};

////////////////////////////////////////////////////////////////////////////////
/// @class UnsortingGatherBlock
/// @brief Execution block for gathers without order
////////////////////////////////////////////////////////////////////////////////
class UnsortingGatherBlock final : public ExecutionBlock {
 public:
  UnsortingGatherBlock(ExecutionEngine& engine, GatherNode const& en)
      : ExecutionBlock(&engine, &en) {
    TRI_ASSERT(en.elements().empty());
  }

  /// @brief initializeCursor
  std::pair<ExecutionState, arangodb::Result> initializeCursor(AqlItemBlock* items,
                                                               size_t pos) override final;

  /// @brief getSome
  std::pair<ExecutionState, std::unique_ptr<AqlItemBlock>> getSome(size_t atMost) override final;

  /// @brief skipSome
  std::pair<ExecutionState, size_t> skipSome(size_t atMost) override final;

 private:
  /// @brief _atDep: currently pulling blocks from _dependencies.at(_atDep),
  size_t _atDep{};
};  // UnsortingGatherBlock

////////////////////////////////////////////////////////////////////////////////
/// @struct SortingStrategy
////////////////////////////////////////////////////////////////////////////////
struct SortingStrategy {
  typedef std::pair<size_t,  // dependency index
                    size_t   // position within a dependecy
                    >
      ValueType;

  virtual ~SortingStrategy() = default;

  /// @brief returns next value
  virtual ValueType nextValue() = 0;

  /// @brief prepare strategy fetching values
  virtual void prepare(std::vector<ValueType>& /*blockPos*/) {}

  /// @brief resets strategy state
  virtual void reset() = 0;
};  // SortingStrategy

////////////////////////////////////////////////////////////////////////////////
/// @class SortingGatherBlock
/// @brief Execution block for gathers with order
////////////////////////////////////////////////////////////////////////////////
class SortingGatherBlock final : public ExecutionBlock {
 public:
  SortingGatherBlock(ExecutionEngine& engine, GatherNode const& en);

  ~SortingGatherBlock();

  /// @brief initializeCursor
  std::pair<ExecutionState, arangodb::Result> initializeCursor(AqlItemBlock* items,
                                                               size_t pos) override final;

  /// @brief getSome
  std::pair<ExecutionState, std::unique_ptr<AqlItemBlock>> getSome(size_t atMost) override final;

  /// @brief skipSome
  std::pair<ExecutionState, size_t> skipSome(size_t atMost) override final;

 private:
  void clearBuffers() noexcept;

  /**
   * @brief Fills all _gatherBlockBuffer entries. Is repeatable during WAITING.
   *
   *
   * @param atMost The amount of data requested per block.
   *
   * @return Will return {WAITING, 0} if it had to request new data from
   * upstream. If everything is in place: all buffers are either filled, or the
   * upstream block is DONE. Will return {DONE, SUM(_gatherBlockBuffer)} on
   * success.
   */
  std::pair<ExecutionState, size_t> fillBuffers(size_t atMost);

  /// @brief getBlock: from dependency i into _gatherBlockBuffer.at(i),
  /// non-simple case only
  std::pair<ExecutionState, bool> getBlocks(size_t i, size_t atMost);

  /// @brief Updates _gatherBlockBuffer and _gatherBlockPos so they point to the
  /// next row.
  void nextRow(size_t i);

  /// @brief Calculates and returns the number of available rows in buffer i.
  size_t availableRows(size_t i) const;

 private:
  /// @brief _gatherBlockBuffer: buffer the incoming block from each dependency
  /// separately
  std::vector<std::deque<AqlItemBlock*>> _gatherBlockBuffer;

  /// @brief _gatherBlockPos: pairs (i, _pos in _buffer.at(i)), i.e. the same as
  /// the usual _pos but one pair per dependency
  std::vector<std::pair<size_t, size_t>> _gatherBlockPos;

  /// @brief whether or not the dependency at index is done
  std::vector<bool> _gatherBlockPosDone;

  /// @brief sort elements for this block
  std::vector<SortRegister> _sortRegisters;

  /// @brief sorting strategy
  std::unique_ptr<SortingStrategy> _strategy;
};  // SortingGatherBlock

class SingleRemoteOperationBlock final : public ExecutionBlock {
  /// @brief constructors/destructors
 private:
  bool getOne(arangodb::aql::AqlItemBlock* aqlres, size_t outputCounter);

 public:
  SingleRemoteOperationBlock(ExecutionEngine* engine, SingleRemoteOperationNode const* en);

  /// @brief timeout
  static double const defaultTimeOut;

  /// @brief getSome
  std::pair<ExecutionState, std::unique_ptr<AqlItemBlock>> getSome(size_t atMost) override final;

  /// @brief skipSome
  std::pair<ExecutionState, size_t> skipSome(size_t atMost) override final;

 private:
  /// @brief _colectionName: the name of the sharded collection
  Collection const* _collection;

  /// @brief the key of the document to fetch
  std::string const _key;
};

}  // namespace aql
}  // namespace arangodb

#endif
