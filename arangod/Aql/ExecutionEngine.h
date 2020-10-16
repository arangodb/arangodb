////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#include "Aql/ExecutionState.h"
#include "Aql/SharedAqlItemBlockPtr.h"
#include "Aql/types.h"
#include "Aql/WalkerWorker.h"
#include "Basics/Common.h"
#include "Containers/SmallVector.h"

#include <memory>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace arangodb {
namespace aql {

class AqlCallStack;
class AqlItemBlock;
class AqlItemBlockManager;
class ExecutionBlock;
class ExecutionNode;
class ExecutionPlan;
struct ExecutionStats;
class Query;
class QueryContext;
class QueryRegistry;
class SkipResult;
class SharedQueryState;

class ExecutionEngine {
 public:
  /// @brief create the engine
  ExecutionEngine(EngineId eId,
                  QueryContext& query,
                  AqlItemBlockManager& itemBlockManager,
                  SerializationFormat format,
                  std::shared_ptr<SharedQueryState> sharedState = nullptr);

  /// @brief destroy the engine, frees all assigned blocks
  TEST_VIRTUAL ~ExecutionEngine();

 public:
  
  // @brief create an execution engine from a plan
  static void instantiateFromPlan(Query& query,
                                  ExecutionPlan& plan,
                                  bool planRegisters,
                                  SerializationFormat format);

  /// @brief Prepares execution blocks for executing provided plan
  /// @param plan plan to execute, should be without cluster nodes. Only local execution
  /// without db-objects access!
  void initFromPlanForCalculation(ExecutionPlan& plan);
  
  TEST_VIRTUAL Result createBlocks(std::vector<ExecutionNode*> const& nodes,
                                   MapRemoteToSnippet const& queryIds);
  
  EngineId engineId() const {
    return _engineId;
  }


  /// @brief get the root block
  TEST_VIRTUAL ExecutionBlock* root() const;

  /// @brief set the root block
  TEST_VIRTUAL void root(ExecutionBlock* root);

  /// @brief get the query
  QueryContext& getQuery() const;
  
  std::shared_ptr<SharedQueryState> sharedState() const {
    return _sharedState;
  }

  /// @brief initializeCursor, could be called multiple times
  std::pair<ExecutionState, Result> initializeCursor(SharedAqlItemBlockPtr&& items, size_t pos);

  auto execute(AqlCallStack const& stack)
      -> std::tuple<ExecutionState, SkipResult, SharedAqlItemBlockPtr>;

  auto executeForClient(AqlCallStack const& stack, std::string const& clientId)
      -> std::tuple<ExecutionState, SkipResult, SharedAqlItemBlockPtr>;

  /// @brief getSome
  std::pair<ExecutionState, SharedAqlItemBlockPtr> getSome(size_t atMost);

  /// @brief skipSome
  std::pair<ExecutionState, size_t> skipSome(size_t atMost);

  /// @brief whether or not initializeCursor was called
  bool initializeCursorCalled() const;

  /// @brief add a block to the engine
  /// @returns added block
  ExecutionBlock* addBlock(std::unique_ptr<ExecutionBlock>);

  /// @brief clears all blocks and root in Engine
  void reset();

  /// @brief set the register the final result of the query is stored in
  void resultRegister(RegisterId resultRegister);

  /// @brief get the register the final result of the query is stored in
  RegisterId resultRegister() const;

  /// @brief accessor to the memory recyler for AqlItemBlocks
  AqlItemBlockManager& itemBlockManager();

  ///  @brief collected execution stats
  void collectExecutionStats(ExecutionStats& other);
  
  bool waitForSatellites(aql::QueryContext& query, Collection const* collection) const;
  
#ifdef USE_ENTERPRISE
  static void parallelizeTraversals(aql::Query& query, ExecutionPlan& plan,
                                    std::map<aql::ExecutionNodeId, aql::ExecutionNodeId>& aliases);
#endif
  
#ifdef ARANGODB_USE_GOOGLE_TESTS
  std::vector<std::unique_ptr<ExecutionBlock>> const& blocksForTesting() const {
    return _blocks;
  }
#endif
  
 private:

  /// @brief  optimizes root node: in case of single RETURN statement 
  /// makes it return value directly and not copy it to output register.
  /// Also sets root execution block as engine root
  void setupEngineRoot(ExecutionBlock& planRoot);
  
  const EngineId _engineId;
  
  /// @brief a pointer to the query
  QueryContext& _query;
  
  /// @brief memory recycler for AqlItemBlocks
  AqlItemBlockManager& _itemBlockManager;
  
  std::shared_ptr<SharedQueryState> _sharedState;

  /// @brief all blocks registered, used for memory management
  std::vector<std::unique_ptr<ExecutionBlock>> _blocks;

  /// @brief root block of the engine
  ExecutionBlock* _root;

  /// @brief the register the final result of the query is stored in
  RegisterId _resultRegister;
  
  /// @brief whether or not initializeCursor was called
  bool _initializeCursorCalled;
};
}  // namespace aql
}  // namespace arangodb

#endif
