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
#include "Aql/ExecutionState.h"
#include "Basics/Result.h"

#include <cstdint>
#include <deque>
#include <utility>
#include <vector>

namespace arangodb {
namespace transaction {
class Methods;
}

namespace aql {
class AqlCallStack;
class InputAqlItemRow;
class ExecutionEngine;
class ExecutionNode;
class SharedAqlItemBlockPtr;

class ExecutionBlock {
 public:
  ExecutionBlock(ExecutionEngine*, ExecutionNode const*);

  virtual ~ExecutionBlock();

  ExecutionBlock(ExecutionBlock const&) = delete;
  ExecutionBlock& operator=(ExecutionBlock const&) = delete;

 public:
  /// @brief batch size value
  [[nodiscard]] static constexpr inline size_t DefaultBatchSize() {
    return 1000;
  }

  /// @brief Number to use when we skip all. Should really be inf, but don't
  /// use something near std::numeric_limits<size_t>::max() to avoid overflows
  /// in calculations.
  /// This is used as an argument for skipSome(), e.g. when counting everything.
  /// Setting this to any other value >0 does not (and must not) affect the
  /// results. It's only to reduce the number of necessary skipSome calls.
  [[nodiscard]] static constexpr inline size_t SkipAllSize() {
    return 1000000000;
  }

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
  [[nodiscard]] virtual std::pair<ExecutionState, Result> initializeCursor(InputAqlItemRow const& input);

  /// @brief shutdown, will be called exactly once for the whole query
  [[nodiscard]] virtual std::pair<ExecutionState, Result> shutdown(int errorCode);

  /// @brief getSome, gets some more items, semantic is as follows: not
  /// more than atMost items may be delivered. The method tries to
  /// return a block of at most atMost items, however, it may return
  /// less (for example if there are not enough items to come). However,
  /// if it returns an actual block, it must contain at least one item.
  /// getSome() also takes care of tracing and clearing registers; don't do it
  /// in getOrSkipSome() implementations.
  [[nodiscard]] virtual std::pair<ExecutionState, SharedAqlItemBlockPtr> getSome(size_t atMost) = 0;

  // Trace the start of a getSome call
  void traceGetSomeBegin(size_t atMost);

  // Trace the end of a getSome call, potentially with result
  [[nodiscard]] std::pair<ExecutionState, SharedAqlItemBlockPtr> traceGetSomeEnd(
      ExecutionState state, SharedAqlItemBlockPtr result);

  void traceSkipSomeBegin(size_t atMost);

  [[nodiscard]] std::pair<ExecutionState, size_t> traceSkipSomeEnd(std::pair<ExecutionState, size_t> res);

  [[nodiscard]] std::pair<ExecutionState, size_t> traceSkipSomeEnd(ExecutionState state,
                                                                   size_t skipped);

  /// @brief skipSome, skips some more items, semantic is as follows: not
  /// more than atMost items may be skipped. The method tries to
  /// skip a block of at most atMost items, however, it may skip
  /// less (for example if there are not enough items to come). The number of
  /// elements skipped is returned.
  [[nodiscard]] virtual std::pair<ExecutionState, size_t> skipSome(size_t atMost) = 0;

  [[nodiscard]] ExecutionState getHasMoreState();

  // TODO: Can we get rid of this? Problem: Subquery Executor is using it.
  [[nodiscard]] ExecutionNode const* getPlanNode() const;

  [[nodiscard]] velocypack::Options const* trxVpackOptions() const noexcept;

  /// @brief add a dependency
  void addDependency(ExecutionBlock* ep);

  /// @brief main function to produce data in this ExecutionBlock.
  ///        It gets the AqlCallStack defining the operations required in every
  ///        subquery level. It will then perform the requested amount of offset, data and fullcount.
  ///        The AqlCallStack is copied on purpose, so this block can modify it.
  ///        Will return
  ///        1. state:
  ///          * WAITING: We have async operation going on, nothing happend, please call again
  ///          * HASMORE: Here is some data in the request range, there is still more, if required call again
  ///          * DONE: Here is some data, and there will be no further data available.
  ///        2. size_t: Amount of documents skipped.
  ///        3. SharedAqlItemBlockPtr: The next data block.
  virtual std::tuple<ExecutionState, size_t, SharedAqlItemBlockPtr> execute(AqlCallStack stack) = 0;

  [[nodiscard]] bool isInSplicedSubquery() const noexcept;

 protected:
  /// @brief the execution engine
  ExecutionEngine* _engine;

  velocypack::Options const* _trxVpackOptions;

  /// @brief the Result returned during the shutdown phase. Is kept for multiple
  ///        waiting phases.
  Result _shutdownResult;

  /// @brief if this is set, we are done, this is reset to false by execute()
  bool _done;

  bool _isInSplicedSubquery;

  /// @brief our corresponding ExecutionNode node
  ExecutionNode const* _exeNode;  // TODO: Can we get rid of this? Problem: Subquery Executor is using it.

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

  /// @brief this is our buffer for the items, it is a deque of AqlItemBlocks.
  /// We keep the following invariant between this and the other two variables
  /// _pos and _done: If _buffer.size() != 0, then 0 <= _pos <
  /// _buffer[0]->size()
  /// and _buffer[0][_pos] is the next item to be handed on. If _done is true,
  /// then no more documents will ever be returned. _done will be set to
  /// true if and only if we have no more data ourselves (i.e.
  /// _buffer.size()==0)
  /// and we have unsuccessfully tried to get another block from our dependency.
  std::deque<SharedAqlItemBlockPtr> _buffer;

  /// @brief current working position in the first entry of _buffer
  size_t _pos;

  /// @brief Collects result blocks during ExecutionBlock::getOrSkipSome. Must
  /// be a member variable due to possible WAITING interruptions.
  aql::BlockCollector _collector;
};

}  // namespace aql
}  // namespace arangodb

#endif
