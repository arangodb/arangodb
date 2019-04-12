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
/// @author Michael Hackstein
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "ExecutionBlock.h"
#include "Aql/AqlItemBlock.h"
#include "Aql/Ast.h"
#include "Aql/BlockCollector.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/Query.h"
#include "Basics/Exceptions.h"

using namespace arangodb;
using namespace arangodb::aql;

namespace {

std::string const doneString = "DONE";
std::string const hasMoreString = "HASMORE";
std::string const waitingString = "WAITING";
std::string const unknownString = "UNKNOWN";

static std::string const& stateToString(ExecutionState state) {
  switch (state) {
    case ExecutionState::DONE:
      return doneString;
    case ExecutionState::HASMORE:
      return hasMoreString;
    case ExecutionState::WAITING:
      return waitingString;
  }
  TRI_ASSERT(false);
  return unknownString;
}

}  // namespace

ExecutionBlock::ExecutionBlock(ExecutionEngine* engine, ExecutionNode const* ep)
    : _engine(engine),
      _trx(engine->getQuery()->trx()),
      _exeNode(ep),
      _dependencyPos(_dependencies.end()),
      _shutdownResult(TRI_ERROR_NO_ERROR),
      _pos(0),
      _done(false),
      _profile(engine->getQuery()->queryOptions().getProfileLevel()),
      _getSomeBegin(0.0),
      _upstreamState(ExecutionState::HASMORE),
      _skipped(0),
      _collector(&engine->itemBlockManager()) {
  TRI_ASSERT(_trx != nullptr);

  // already insert ourselves into the statistics results
  if (_profile >= PROFILE_LEVEL_BLOCKS) {
    _engine->_stats.nodes.emplace(ep->id(), ExecutionStats::Node());
  }
}

ExecutionBlock::~ExecutionBlock() = default;

/// @brief returns the register id for a variable id
/// will return ExecutionNode::MaxRegisterId for an unknown variable
RegisterId ExecutionBlock::getRegister(VariableId id) const {
  auto it = _exeNode->getRegisterPlan()->varInfo.find(id);

  if (it != _exeNode->getRegisterPlan()->varInfo.end()) {
    return (*it).second.registerId;
  }
  return ExecutionNode::MaxRegisterId;
}

RegisterId ExecutionBlock::getRegister(Variable const* variable) const {
  TRI_ASSERT(variable != nullptr);
  return getRegister(variable->id);
}

bool ExecutionBlock::removeDependency(ExecutionBlock* ep) {
  auto it = _dependencies.begin();
  while (it != _dependencies.end()) {
    if (*it == ep) {
      _dependencies.erase(it);
      _dependencyPos = _dependencies.end();
      return true;
    }
    ++it;
  }
  return false;
}

/// @brief whether or not the query was killed
void ExecutionBlock::throwIfKilled() {
  if (_engine->getQuery()->killed()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_KILLED);
  }
}

std::pair<ExecutionState, arangodb::Result> ExecutionBlock::initializeCursor(InputAqlItemRow const& input) {
  if (_dependencyPos == _dependencies.end()) {
    // We need to start again.
    _dependencyPos = _dependencies.begin();
  }
  for (; _dependencyPos != _dependencies.end(); ++_dependencyPos) {
    auto res = (*_dependencyPos)->initializeCursor(input);
    if (res.first == ExecutionState::WAITING || !res.second.ok()) {
      // If we need to wait or get an error we return as is.
      return res;
    }
  }

  _buffer.clear();

  _done = false;
  _upstreamState = ExecutionState::HASMORE;
  _pos = 0;
  _skipped = 0;
  _collector.clear();

  TRI_ASSERT(getHasMoreState() == ExecutionState::HASMORE);
  TRI_ASSERT(_dependencyPos == _dependencies.end());
  return {ExecutionState::DONE, TRI_ERROR_NO_ERROR};
}

/// @brief shutdown, will be called exactly once for the whole query
std::pair<ExecutionState, Result> ExecutionBlock::shutdown(int errorCode) {
  if (_dependencyPos == _dependencies.end()) {
    _shutdownResult.reset(TRI_ERROR_NO_ERROR);
    _dependencyPos = _dependencies.begin();
  }

  for (; _dependencyPos != _dependencies.end(); ++_dependencyPos) {
    Result res;
    ExecutionState state;
    try {
      std::tie(state, res) = (*_dependencyPos)->shutdown(errorCode);
      if (state == ExecutionState::WAITING) {
        return {state, TRI_ERROR_NO_ERROR};
      }
    } catch (...) {
      _shutdownResult.reset(TRI_ERROR_INTERNAL);
    }

    if (res.fail()) {
      _shutdownResult = res;
    }
  }

  _buffer.clear();

  return {ExecutionState::DONE, _shutdownResult};
}

// Trace the start of a getSome call
void ExecutionBlock::traceGetSomeBegin(size_t atMost) {
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
void ExecutionBlock::traceGetSomeEnd(AqlItemBlock const* result, ExecutionState state) {
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
          << "getSome done type=" << node->getTypeString() << " this=" << (uintptr_t)this
          << " id=" << node->id() << " state=" << ::stateToString(state);

      if (_profile >= PROFILE_LEVEL_TRACE_2) {
        if (result == nullptr) {
          LOG_TOPIC("daa64", INFO, Logger::QUERIES)
              << "getSome type=" << node->getTypeString() << " result: nullptr";
        } else {
          VPackBuilder builder;
          {
            VPackObjectBuilder guard(&builder);
            result->toVelocyPack(_trx, builder);
          }
          LOG_TOPIC("fcd9c", INFO, Logger::QUERIES) << "getSome type=" << node->getTypeString()
                                           << " result: " << builder.toJson();
        }
      }
    }
  }
}

void ExecutionBlock::traceSkipSomeBegin(size_t atMost) {
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

void ExecutionBlock::traceSkipSomeEnd(size_t skipped, ExecutionState state) {
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
          << "skipSome done type=" << node->getTypeString() << " this=" << (uintptr_t)this
          << " id=" << node->id() << " state=" << ::stateToString(state);
    }
  }
}

/// @brief request an AqlItemBlock from the memory manager
SharedAqlItemBlockPtr ExecutionBlock::requestBlock(size_t nrItems, RegisterId nrRegs) {
  return _engine->itemBlockManager().requestBlock(nrItems, nrRegs);
}

/// @brief return an AqlItemBlock to the memory manager
void ExecutionBlock::returnBlock(AqlItemBlock*& block) noexcept {
  _engine->itemBlockManager().returnBlock(block);
}

/// @brief return an AqlItemBlock to the memory manager, but ignore nullptr
void ExecutionBlock::returnBlockUnlessNull(AqlItemBlock*& block) noexcept {
  if (block != nullptr) {
    _engine->itemBlockManager().returnBlock(block);
  }
}

/// @brief copy register data from one block (src) into another (dst)
/// register values are cloned
void ExecutionBlock::inheritRegisters(AqlItemBlock const* src, AqlItemBlock* dst,
                                      size_t srcRow, size_t dstRow) {
  TRI_ASSERT(src != nullptr);
  RegisterId const n = src->getNrRegs();
  auto planNode = getPlanNode();

  for (RegisterId i = 0; i < n; i++) {
    if (planNode->_regsToClear.find(i) != planNode->_regsToClear.end()) {
      continue;
    }

    auto const& value = src->getValueReference(srcRow, i);

    if (!value.isEmpty()) {
      AqlValue a = value.clone();
      AqlValueGuard guard(a, true);

      TRI_IF_FAILURE("ExecutionBlock::inheritRegisters") {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }

      dst->setValue(dstRow, i, a);
      guard.steal();
    }
  }
}

/// @brief the following is internal to pull one more block and append it to
/// our _buffer deque. Returns true if a new block was appended and false if
/// the dependent node is exhausted.
std::pair<ExecutionState, bool> ExecutionBlock::getBlock(size_t atMost) {
  throwIfKilled();  // check if we were aborted

  std::pair<ExecutionState, SharedAqlItemBlockPtr> res = _dependencies[0]->getSome(atMost);
  if (res.first == ExecutionState::WAITING) {
    return {res.first, false};
  }

  TRI_IF_FAILURE("ExecutionBlock::getBlock") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  _upstreamState = res.first;

  if (res.second != nullptr) {
    _buffer.emplace_back(std::move(res.second));
    return {res.first, true};
  }

  return {res.first, false};
}

void ExecutionBlock::clearRegisters(AqlItemBlock* result) {
  // Clear out registers not needed later on:
  if (result != nullptr) {
    result->clearRegisters(getPlanNode()->_regsToClear);
  }
}

ExecutionBlock::BufferState ExecutionBlock::getBlockIfNeeded(size_t atMost) {
  if (!_buffer.empty()) {
    return BufferState::HAS_BLOCKS;
  }

  // _pos must be reset to 0 during both initialize and after removing an item
  // from _buffer, so if the buffer is empty, it must always be 0
  TRI_ASSERT(_pos == 0);

  if (_upstreamState == ExecutionState::DONE) {
    return BufferState::NO_MORE_BLOCKS;
  }

  ExecutionState state;
  bool blockAppended;
  std::tie(state, blockAppended) = getBlock(atMost);

  if (state == ExecutionState::WAITING) {
    TRI_ASSERT(!blockAppended);
    return BufferState::WAITING;
  }

  // !blockAppended => DONE
  TRI_ASSERT(blockAppended || state == ExecutionState::DONE);

  if (blockAppended) {
    return BufferState::HAS_NEW_BLOCK;
  }

  return BufferState::NO_MORE_BLOCKS;
}

ExecutionState ExecutionBlock::getHasMoreState() {
  if (_done) {
    return ExecutionState::DONE;
  }
  if (_buffer.empty() && _upstreamState == ExecutionState::DONE) {
    _done = true;
    return ExecutionState::DONE;
  }
  return ExecutionState::HASMORE;
}

RegisterId ExecutionBlock::getNrInputRegisters() const {
  ExecutionNode const* previousNode = getPlanNode()->getFirstDependency();
  if (previousNode == nullptr) {
    return 0;
  }
  TRI_ASSERT(previousNode != nullptr);
  RegisterId const inputNrRegs =
      previousNode->getRegisterPlan()->nrRegs[previousNode->getDepth()];

  return inputNrRegs;
}

RegisterId ExecutionBlock::getNrOutputRegisters() const {
  ExecutionNode const* planNode = getPlanNode();
  TRI_ASSERT(planNode != nullptr);
  RegisterId const outputNrRegs =
      planNode->getRegisterPlan()->nrRegs[planNode->getDepth()];

  return outputNrRegs;
}
