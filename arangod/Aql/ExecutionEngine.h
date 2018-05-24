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

#include "Basics/Common.h"
#include "Aql/AqlItemBlockManager.h"
#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/ExecutionStats.h"

namespace arangodb {
namespace aql {
class AqlItemBlock;
class QueryRegistry;

class ExecutionEngine {
 public:
  /// @brief create the engine
  explicit ExecutionEngine(Query* query);

  /// @brief destroy the engine, frees all assigned blocks
  TEST_VIRTUAL ~ExecutionEngine();

 public:
  // @brief create an execution engine from a plan
  static ExecutionEngine* instantiateFromPlan(QueryRegistry*, Query*,
                                              ExecutionPlan*, bool);

  TEST_VIRTUAL Result createBlocks(
      std::vector<ExecutionNode*> const& nodes,
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
  int initializeCursor(AqlItemBlock* items, size_t pos) {
    return _root->initializeCursor(items, pos);
  }
  
  /// @brief initialize
  int initialize() {
    return _root->initialize();
  }

  /// @brief shutdown, will be called exactly once for the whole query
  int shutdown(int errorCode);

  /// @brief getSome
  AqlItemBlock* getSome(size_t atMost) {
    return _root->getSome(atMost);
  }

  /// @brief skipSome
  size_t skipSome(size_t atMost) {
    return _root->skipSome(atMost);
  }

  /// @brief getOne
  AqlItemBlock* getOne() { return _root->getSome(1); }

  /// @brief skip
  bool skip(size_t number, size_t& actuallySkipped) { 
    return _root->skip(number, actuallySkipped); 
  }

  /// @brief hasMore
  inline bool hasMore() const { return _root->hasMore(); }

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

  /// @brief _lockedShards
  TEST_VIRTUAL void setLockedShards(std::unordered_set<std::string>* lockedShards) {
    _lockedShards = lockedShards;
  }

  /// @brief _lockedShards
  std::unordered_set<std::string>* lockedShards() const {
    return _lockedShards;
  }

 public:
  /// @brief execution statistics for the query
  /// note that the statistics are modification by execution blocks
  ExecutionStats _stats;

  /// @brief memory recycler for AqlItemBlocks
  AqlItemBlockManager _itemBlockManager;

 private:
  /// @brief all blocks registered, used for memory management
  std::vector<ExecutionBlock*> _blocks;

  /// @brief root block of the engine
  ExecutionBlock* _root;

  /// @brief a pointer to the query
  Query* _query;

  /// @brief the register the final result of the query is stored in
  RegisterId _resultRegister;

  /// @brief whether or not shutdown() was executed
  bool _wasShutdown;

  /// @brief _previouslyLockedShards, this is read off at instanciating
  /// time from a thread local variable
  std::unordered_set<std::string>* _previouslyLockedShards;

  /// @brief _lockedShards, these are the shards we have locked for our query
  std::unordered_set<std::string>* _lockedShards;
};
}
}

#endif
