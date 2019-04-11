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

#ifndef ARANGOD_AQL_EXECUTION_BLOCK_H
#define ARANGOD_AQL_EXECUTION_BLOCK_H 1

#include "Aql/BlockCollector.h"
#include "Aql/ExecutionNode.h"
#include "Aql/ExecutionState.h"
#include "Aql/Variable.h"

#include <deque>

namespace arangodb {
struct ClusterCommResult;

namespace transaction {
class Methods;
}

namespace aql {
class InputAqlItemRow;
class ExecutionEngine;

class ExecutionBlock {
 public:
  ExecutionBlock(ExecutionEngine*, ExecutionNode const*);

  virtual ~ExecutionBlock();

  ExecutionBlock(ExecutionBlock const&) = delete;

 public:
  /// @brief batch size value
  static constexpr inline size_t DefaultBatchSize() { return 1000; }

  /// @brief Methods for execution
  /// Lifecycle is:
  ///    CONSTRUCTOR
  ///    then the ExecutionEngine automatically calls
  ///      initialize() once, including subqueries
  ///    possibly repeat many times:
  ///      initializeCursor(...)   (optionally with bind parameters)
  ///      // use cursor functionality
  ///    then the ExecutionEngine automatically calls
  ///      shutdown()
  ///    DESTRUCTOR

  /// @brief initializeCursor, could be called multiple times
  virtual std::pair<ExecutionState, Result> initializeCursor(InputAqlItemRow const& input) = 0;

  /// @brief shutdown, will be called exactly once for the whole query
  virtual std::pair<ExecutionState, Result> shutdown(int errorCode) = 0;

  /// @brief getSome, gets some more items, semantic is as follows: not
  /// more than atMost items may be delivered. The method tries to
  /// return a block of at most atMost items, however, it may return
  /// less (for example if there are not enough items to come). However,
  /// if it returns an actual block, it must contain at least one item.
  /// getSome() also takes care of tracing and clearing registers; don't do it
  /// in getOrSkipSome() implementations.
  virtual std::pair<ExecutionState, std::unique_ptr<AqlItemBlock>> getSome(size_t atMost) = 0;

  void traceGetSomeBegin(size_t atMost);
  void traceGetSomeEnd(AqlItemBlock const*, ExecutionState state);

  void traceSkipSomeBegin(size_t atMost);
  void traceSkipSomeEnd(size_t skipped, ExecutionState state);

  /// @brief skipSome, skips some more items, semantic is as follows: not
  /// more than atMost items may be skipped. The method tries to
  /// skip a block of at most atMost items, however, it may skip
  /// less (for example if there are not enough items to come). The number of
  /// elements skipped is returned.
  virtual std::pair<ExecutionState, size_t> skipSome(size_t atMost) = 0;

  // TODO: Can we get rid of this? Problem: Subquery Executor is using it.
  ExecutionNode const* getPlanNode() const { return _exeNode; }

  transaction::Methods* transaction() const { return _trx; }

  // @brief Will be called on the querywakeup callback with the
  // result collected over the network. Needs to be implemented
  // on all nodes that use this mechanism.
  virtual bool handleAsyncResult(ClusterCommResult* result) {
    // This indicates that a node uses async functionality
    // but does not react to the response.
    TRI_ASSERT(false);
    return true;
  }

  /// @brief add a dependency
  void addDependency(ExecutionBlock* ep) {
    TRI_ASSERT(ep != nullptr);
    _dependencies.emplace_back(ep);
    _dependencyPos = _dependencies.end();
  }

  /// @brief throw an exception if query was killed
  void throwIfKilled();

 protected:
  /// @brief the execution engine
  ExecutionEngine* _engine;

  /// @brief the transaction for this query
  transaction::Methods* _trx;

  /// @brief the Result returned during the shutdown phase. Is kept for multiple
  ///        waiting phases.
  Result _shutdownResult;

  /// @brief if this is set, we are done, this is reset to false by execute()
  bool _done;

  /// @brief our corresponding ExecutionNode node
  ExecutionNode const* _exeNode; // TODO: Can we get rid of this? Problem: Subquery Executor is using it.

  /// @brief our dependent nodes
  std::vector<ExecutionBlock*> _dependencies;

  /// @brief position in the dependencies while iterating through them
  ///        used in initializeCursor and shutdown.
  ///        Needs to be set to .end() everytime we modify _dependencies
  std::vector<ExecutionBlock*>::iterator _dependencyPos;

  /// @brief profiling level
  uint32_t _profile;

  /// @brief getSome begin point in time
  double _getSomeBegin;

  /// @brief the execution state of the dependency
  ///        used to determine HASMORE or DONE better
  ExecutionState _upstreamState;

  /// @brief Collects result blocks during ExecutionBlock::getOrSkipSome. Must
  /// be a member variable due to possible WAITING interruptions.
  aql::BlockCollector _collector;
};

}  // namespace aql
}  // namespace arangodb

#endif
