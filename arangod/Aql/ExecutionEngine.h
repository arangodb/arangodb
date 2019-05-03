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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_EXECUTION_ENGINE_H
#define ARANGOD_AQL_EXECUTION_ENGINE_H 1

#include "Aql/AqlItemBlockManager.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/ExecutionStats.h"
#include "Aql/Query.h"
#include "Basics/Common.h"

namespace arangodb {
namespace aql {
class AqlItemBlock;
class ExecutionBlock;
class QueryRegistry;

class ExecutionEngine {
 public:
  /// @brief create the engine
  explicit ExecutionEngine(Query* query);

  /// @brief destroy the engine, frees all assigned blocks
  TEST_VIRTUAL ~ExecutionEngine();

 public:
  // @brief create an execution engine from a plan
  static ExecutionEngine* instantiateFromPlan(QueryRegistry*, Query*, ExecutionPlan*, bool);

  TEST_VIRTUAL Result createBlocks(std::vector<ExecutionNode*> const& nodes,
                                   std::unordered_set<std::string> const& restrictToShards,
                                   MapRemoteToSnippet const& queryIds);

  /// @brief get the root block
  TEST_VIRTUAL ExecutionBlock* root() const {
    TRI_ASSERT(_root != nullptr);
    return _root;
  }

  /// @brief set the root block
  TEST_VIRTUAL void root(ExecutionBlock* root) {
    TRI_ASSERT(root != nullptr);
    _root = root;
  }

  /// @brief get the query
  TEST_VIRTUAL Query* getQuery() const { return _query; }

  /// @brief initializeCursor, could be called multiple times
  std::pair<ExecutionState, Result> initializeCursor(SharedAqlItemBlockPtr&& items, size_t pos);

  /// @brief shutdown, will be called exactly once for the whole query, blocking
  /// variant
  Result shutdownSync(int errorCode) noexcept;

  /// @brief shutdown, will be called exactly once for the whole query, may
  /// return waiting
  std::pair<ExecutionState, Result> shutdown(int errorCode);

  /// @brief getSome
  std::pair<ExecutionState, SharedAqlItemBlockPtr> getSome(size_t atMost);

  /// @brief skipSome
  std::pair<ExecutionState, size_t> skipSome(size_t atMost);

  /// @brief whether or not initializeCursor was called
  bool initializeCursorCalled() const { return _initializeCursorCalled; }

  /// @brief add a block to the engine
  TEST_VIRTUAL void addBlock(ExecutionBlock*);

  /// @brief add a block to the engine
  /// @returns added block
  ExecutionBlock* addBlock(std::unique_ptr<ExecutionBlock>&&);

  /// @brief set the register the final result of the query is stored in
  void resultRegister(RegisterId resultRegister) {
    _resultRegister = resultRegister;
  }

  /// @brief get the register the final result of the query is stored in
  RegisterId resultRegister() const { return _resultRegister; }

  /// @brief accessor to the memory recyler for AqlItemBlocks
  TEST_VIRTUAL AqlItemBlockManager& itemBlockManager() {
    return _itemBlockManager;
  }

 public:
  /// @brief execution statistics for the query
  /// note that the statistics are modification by execution blocks
  ExecutionStats _stats;

 private:
  /// @brief memory recycler for AqlItemBlocks
  AqlItemBlockManager _itemBlockManager;

  /// @brief all blocks registered, used for memory management
  std::vector<ExecutionBlock*> _blocks;

  /// @brief root block of the engine
  ExecutionBlock* _root;

  /// @brief a pointer to the query
  Query* _query;

  /// @brief the register the final result of the query is stored in
  RegisterId _resultRegister;

  /// @brief whether or not initializeCursor was called
  bool _initializeCursorCalled;

  /// @brief whether or not shutdown() was executed
  bool _wasShutdown;
};
}  // namespace aql
}  // namespace arangodb

#endif
