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
#include "Aql/ExecutionState.h"
#include "Aql/ExecutionStats.h"
#include "Aql/SharedAqlItemBlockPtr.h"
#include "Aql/types.h"
#include "Basics/Common.h"

#include <memory>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace arangodb {
class Result;
namespace aql {
class AqlItemBlock;
class ExecutionBlock;
class ExecutionNode;
class ExecutionPlan;
class QueryRegistry;
class Query;
enum class SerializationFormat;

class ExecutionEngine {
 public:
  /// @brief create the engine
  ExecutionEngine(Query& query, SerializationFormat format);

  /// @brief destroy the engine, frees all assigned blocks
  TEST_VIRTUAL ~ExecutionEngine();

 public:
  // @brief create an execution engine from a plan
  static ExecutionEngine* instantiateFromPlan(QueryRegistry& queryRegistry, Query& query,
                                              ExecutionPlan& plan, bool planRegisters,
                                              SerializationFormat format);

  TEST_VIRTUAL Result createBlocks(std::vector<ExecutionNode*> const& nodes,
                                   std::unordered_set<std::string> const& restrictToShards,
                                   MapRemoteToSnippet const& queryIds);

  /// @brief get the root block
  TEST_VIRTUAL ExecutionBlock* root() const;

  /// @brief set the root block
  TEST_VIRTUAL void root(ExecutionBlock* root);

  /// @brief get the query
  TEST_VIRTUAL Query* getQuery() const;

  /// @brief server to snippet mapping
  void snippetMapping(MapRemoteToSnippet&& dbServerMapping,
                      std::vector<uint64_t>&& coordinatorQueryIds) {
    _dbServerMapping = std::move(dbServerMapping);
    _coordinatorQueryIds = std::move(coordinatorQueryIds);
  }

  /// @brief kill the query
  void kill();

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
  bool initializeCursorCalled() const;

  /// @brief add a block to the engine
  /// @returns added block
  ExecutionBlock* addBlock(std::unique_ptr<ExecutionBlock>);

  /// @brief set the register the final result of the query is stored in
  void resultRegister(RegisterId resultRegister);

  /// @brief get the register the final result of the query is stored in
  RegisterId resultRegister() const;

  /// @brief accessor to the memory recyler for AqlItemBlocks
  TEST_VIRTUAL AqlItemBlockManager& itemBlockManager();

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
  Query& _query;

  /// @brief the register the final result of the query is stored in
  RegisterId _resultRegister;

  /// @brief whether or not initializeCursor was called
  bool _initializeCursorCalled;

  /// @brief whether or not shutdown() was executed
  bool _wasShutdown;

  /// @brief server to snippet mapping
  MapRemoteToSnippet _dbServerMapping;

  /// @brief ids of all coordinator query snippets
  std::vector<uint64_t> _coordinatorQueryIds;
};
}  // namespace aql
}  // namespace arangodb

#endif
