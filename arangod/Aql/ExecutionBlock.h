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

#include "Aql/AqlItemBlock.h"
#include "Aql/BlockCollector.h"
#include "Aql/ExecutionNode.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionState.h"
#include "Aql/ExecutionStats.h"
#include "Aql/Variable.h"
#include "QueryOptions.h"

#include <deque>

namespace arangodb {
struct ClusterCommResult;

namespace transaction {
class Methods;
}

namespace {

std::string const doneString = "DONE";
std::string const hasMoreString = "HASMORE";
std::string const waitingString = "WAITING";
std::string const unknownString = "UNKNOWN";

static std::string const& stateToString(aql::ExecutionState state) {
  switch (state) {
    case aql::ExecutionState::DONE:
      return doneString;
    case aql::ExecutionState::HASMORE:
      return hasMoreString;
    case aql::ExecutionState::WAITING:
      return waitingString;
    default:
      // just to suppress a warning ..
      return unknownString;
  }
}

}  // namespace

namespace aql {
class InputAqlItemRow;
class ExecutionEngine;
class SharedAqlItemBlockPtr;

class ExecutionBlock {
 public:
  ExecutionBlock(ExecutionEngine*, ExecutionNode const*);

  virtual ~ExecutionBlock();

  ExecutionBlock(ExecutionBlock const&) = delete;
  ExecutionBlock& operator=(ExecutionBlock const&) = delete;

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
  virtual std::pair<ExecutionState, Result> initializeCursor(InputAqlItemRow const& input);

  /// @brief shutdown, will be called exactly once for the whole query
  virtual std::pair<ExecutionState, Result> shutdown(int errorCode);

  /// @brief getSome, gets some more items, semantic is as follows: not
  /// more than atMost items may be delivered. The method tries to
  /// return a block of at most atMost items, however, it may return
  /// less (for example if there are not enough items to come). However,
  /// if it returns an actual block, it must contain at least one item.
  /// getSome() also takes care of tracing and clearing registers; don't do it
  /// in getOrSkipSome() implementations.
  virtual std::pair<ExecutionState, SharedAqlItemBlockPtr> getSome(size_t atMost) = 0;

  // Trace the start of a getSome call
  inline void traceGetSomeBegin(size_t atMost) {
    if (_profile >= PROFILE_LEVEL_BLOCKS) {
      if (_getSomeBegin <= 0.0) {
        _getSomeBegin = TRI_microtime();
      }
      if (_profile >= PROFILE_LEVEL_TRACE_1) {
        auto node = getPlanNode();
        LOG_TOPIC("ca7db", INFO, Logger::QUERIES)
            << "getSome type=" << node->getTypeString() << " atMost = " << atMost
            << " this=" << (uintptr_t)this << " id=" << node->id();
      }
    }
  }

  // Trace the end of a getSome call, potentially with result
  inline std::pair<ExecutionState, SharedAqlItemBlockPtr> traceGetSomeEnd(
      ExecutionState state, SharedAqlItemBlockPtr result) {
    TRI_ASSERT(result != nullptr || state != ExecutionState::HASMORE);
    if (_profile >= PROFILE_LEVEL_BLOCKS) {
      ExecutionNode const* en = getPlanNode();
      ExecutionStats::Node stats;
      stats.calls = 1;
      stats.items = result != nullptr ? result->size() : 0;
      if (state != ExecutionState::WAITING) {
        stats.runtime = TRI_microtime() - _getSomeBegin;
        _getSomeBegin = 0.0;
      }

      auto it = _engine->_stats.nodes.find(en->id());
      if (it != _engine->_stats.nodes.end()) {
        it->second += stats;
      } else {
        _engine->_stats.nodes.emplace(en->id(), stats);
      }

      if (_profile >= PROFILE_LEVEL_TRACE_1) {
        ExecutionNode const* node = getPlanNode();
        LOG_TOPIC("07a60", INFO, Logger::QUERIES)
            << "getSome done type=" << node->getTypeString()
            << " this=" << (uintptr_t)this << " id=" << node->id()
            << " state=" << stateToString(state);

        if (_profile >= PROFILE_LEVEL_TRACE_2) {
          if (result == nullptr) {
            LOG_TOPIC("daa64", INFO, Logger::QUERIES)
                << "getSome type=" << node->getTypeString() << " result: nullptr";
          } else {
            VPackBuilder builder;
            {
              VPackObjectBuilder guard(&builder);
              result->toVelocyPack(transaction(), builder);
            }
            LOG_TOPIC("fcd9c", INFO, Logger::QUERIES)
                << "getSome type=" << node->getTypeString()
                << " result: " << builder.toJson();
          }
        }
      }
    }
    return {state, std::move(result)};
  }

  inline void traceSkipSomeBegin(size_t atMost) {
    if (_profile >= PROFILE_LEVEL_BLOCKS) {
      if (_getSomeBegin <= 0.0) {
        _getSomeBegin = TRI_microtime();
      }
      if (_profile >= PROFILE_LEVEL_TRACE_1) {
        auto node = getPlanNode();
        LOG_TOPIC("dba8a", INFO, Logger::QUERIES)
            << "skipSome type=" << node->getTypeString() << " atMost = " << atMost
            << " this=" << (uintptr_t)this << " id=" << node->id();
      }
    }
  }

  inline std::pair<ExecutionState, size_t> traceSkipSomeEnd(std::pair<ExecutionState, size_t> const res) {
    ExecutionState const state = res.first;
    size_t const skipped = res.second;

    if (_profile >= PROFILE_LEVEL_BLOCKS) {
      ExecutionNode const* en = getPlanNode();
      ExecutionStats::Node stats;
      stats.calls = 1;
      stats.items = skipped;
      if (state != ExecutionState::WAITING) {
        stats.runtime = TRI_microtime() - _getSomeBegin;
        _getSomeBegin = 0.0;
      }

      auto it = _engine->_stats.nodes.find(en->id());
      if (it != _engine->_stats.nodes.end()) {
        it->second += stats;
      } else {
        _engine->_stats.nodes.emplace(en->id(), stats);
      }

      if (_profile >= PROFILE_LEVEL_TRACE_1) {
        ExecutionNode const* node = getPlanNode();
        LOG_TOPIC("d1950", INFO, Logger::QUERIES)
        << "skipSome done type=" << node->getTypeString()
        << " this=" << (uintptr_t)this << " id=" << node->id()
        << " state=" << stateToString(state);
      }
    }
    return res;
  }

  inline std::pair<ExecutionState, size_t> traceSkipSomeEnd(ExecutionState state, size_t skipped) {
    return traceSkipSomeEnd({state, skipped});
  }

  /// @brief skipSome, skips some more items, semantic is as follows: not
  /// more than atMost items may be skipped. The method tries to
  /// skip a block of at most atMost items, however, it may skip
  /// less (for example if there are not enough items to come). The number of
  /// elements skipped is returned.
  virtual std::pair<ExecutionState, size_t> skipSome(size_t atMost) = 0;

  ExecutionState getHasMoreState() {
    if (_done) {
      return ExecutionState::DONE;
    }
    if (_buffer.empty() && _upstreamState == ExecutionState::DONE) {
      _done = true;
      return ExecutionState::DONE;
    }
    return ExecutionState::HASMORE;
  }

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
