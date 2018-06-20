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
#include "Aql/Query.h"

using namespace arangodb::aql;
  
namespace {
  static std::string StateToString(ExecutionState state) {
    switch (state) {
      case ExecutionState::DONE:
        return "DONE";
      case ExecutionState::HASMORE:
        return "HASMORE";
      case ExecutionState::WAITING:
        return "WAITING";
    }
    TRI_ASSERT(false);
    return "unkown";
  }
}

ExecutionBlock::ExecutionBlock(ExecutionEngine* engine, ExecutionNode const* ep)
    : _engine(engine),
      _trx(engine->getQuery()->trx()),
      _exeNode(ep),
      _pos(0),
      _done(false),
      _profile(engine->getQuery()->queryOptions().profile),
      _getSomeBegin(0),
      _upstreamState(ExecutionState::HASMORE),
      _skipped(0),
      _collector(&engine->_itemBlockManager) {
  TRI_ASSERT(_trx != nullptr);
}

ExecutionBlock::~ExecutionBlock() {
  for (auto& it : _buffer) {
    delete it;
  }
}

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
      return true;
    }
    ++it;
  }
  return false;
}

/// @brief whether or not the query was killed
bool ExecutionBlock::isKilled() const { return _engine->getQuery()->killed(); }

/// @brief whether or not the query was killed
void ExecutionBlock::throwIfKilled() {
  if (isKilled()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_KILLED);
  }
}

std::pair<ExecutionState, arangodb::Result> ExecutionBlock::initializeCursor(
    AqlItemBlock* items, size_t pos) {
  for (auto& d : _dependencies) {
    auto res = d->initializeCursor(items, pos);
    if (res.first == ExecutionState::WAITING ||
        !res.second.ok()) {
      // If we need to wait or get an error we return as is.
      return res;
    }
  }

  for (auto& it : _buffer) {
    delete it;
  }
  _buffer.clear();

  _done = false;
  _upstreamState = ExecutionState::HASMORE;

  if (_profile >= PROFILE_LEVEL_BLOCKS) {
    // Set block type in per-block statistics.
    // Intentionally using operator[], which inserts a new element if it can't
    // find one.
    _engine->_stats.nodes[getPlanNode()->id()].type = this->getType();
  }

  TRI_ASSERT(getHasMoreState() == ExecutionState::HASMORE);
  return {ExecutionState::DONE, TRI_ERROR_NO_ERROR};
}

/// @brief shutdown, will be called exactly once for the whole query
int ExecutionBlock::shutdown(int errorCode) {
  int ret = TRI_ERROR_NO_ERROR;

  for (auto& it : _buffer) {
    delete it;
  }
  _buffer.clear();

  for (auto it = _dependencies.begin(); it != _dependencies.end(); ++it) {
    int res;
    try {
      res = (*it)->shutdown(errorCode);
    } catch (...) {
      res = TRI_ERROR_INTERNAL;
    }

    if (res != TRI_ERROR_NO_ERROR) {
      ret = res;
    }
  }

  return ret;
}

// Trace the start of a getSome call
void ExecutionBlock::traceGetSomeBegin(size_t atMost) {
  if (_profile >= PROFILE_LEVEL_BLOCKS) {
    _getSomeBegin = TRI_microtime();
    if (_profile >= PROFILE_LEVEL_TRACE_1) {
      auto node = getPlanNode();
      LOG_TOPIC(INFO, Logger::QUERIES)
      << "getSome type=" << node->getTypeString()
      << " atMost = " << atMost
      << " this=" << (uintptr_t) this << " id=" << node->id();
    }
  }
}

// Trace the end of a getSome call, potentially with result
void ExecutionBlock::traceGetSomeEnd(AqlItemBlock const* result, ExecutionState state) const {
  TRI_ASSERT(result != nullptr || state != ExecutionState::HASMORE);
  if (_profile >= PROFILE_LEVEL_BLOCKS) {
    ExecutionNode const* en = getPlanNode();
    ExecutionStats::Node stats;
    stats.calls = 1;
    stats.items = result != nullptr ? result->size() : 0;
    stats.runtime = TRI_microtime() - _getSomeBegin;
    auto it = _engine->_stats.nodes.find(en->id());
    if (it != _engine->_stats.nodes.end()) {
      it->second += stats;
    } else {
      _engine->_stats.nodes.emplace(en->id(), stats);
    }
    
    if (_profile >= PROFILE_LEVEL_TRACE_1) {
      ExecutionNode const* node = getPlanNode();
      LOG_TOPIC(INFO, Logger::QUERIES) << "getSome done type="
      << node->getTypeString() << " this=" << (uintptr_t) this
      << " id=" << node->id() << " state=" << StateToString(state);
      
      if (_profile >= PROFILE_LEVEL_TRACE_2) {
        if (result == nullptr) {
          LOG_TOPIC(INFO, Logger::QUERIES)
          << "getSome type=" << node->getTypeString() << " result: nullptr";
        } else {
          VPackBuilder builder;
          {
            VPackObjectBuilder guard(&builder);
            result->toVelocyPack(_trx, builder);
          }
          LOG_TOPIC(INFO, Logger::QUERIES)
          << "getSome type=" << node->getTypeString()
          << " result: " << builder.toJson();
        }
      }
    }
  }
}

/// @brief getSome, gets some more items, semantic is as follows: not
/// more than atMost items may be delivered. The method tries to
/// return a block of at most atMost items, however, it may return
/// less (for example if there are not enough items to come). However,
/// if it returns an actual block, it must contain at least one item.
/// getSome() also takes care of tracing and clearing registers; don't do it
/// in getOrSkipSome() implementations.
// TODO Blocks overriding getSome (and skipSome) instead of getOrSkipSome should
//      still not have to call traceGetSomeBegin/~End and clearRegisters on
//      their own. This can be solved by adding one level of indirection via a
//      method _getSome(), which by default only calls
//      getSomeWithoutRegisterClearout() and which can be overridden instead.
//      Or maybe overriding getSomeWithoutRegisterClearout() instead is better.
std::pair<ExecutionState, std::unique_ptr<AqlItemBlock>>
ExecutionBlock::getSome(size_t atMost) {
  DEBUG_BEGIN_BLOCK();
  traceGetSomeBegin(atMost);
    
  auto res = getSomeWithoutRegisterClearout(atMost);
  if (res.first == ExecutionState::WAITING) {
    traceGetSomeEnd(nullptr, res.first);
    return {ExecutionState::WAITING, nullptr};
  }

  clearRegisters(res.second.get());
  traceGetSomeEnd(res.second.get(), res.first);
  return res;
  DEBUG_END_BLOCK();
}

/// @brief request an AqlItemBlock from the memory manager
AqlItemBlock* ExecutionBlock::requestBlock(size_t nrItems, RegisterId nrRegs) {
  return _engine->_itemBlockManager.requestBlock(nrItems, nrRegs);
}

/// @brief return an AqlItemBlock to the memory manager
void ExecutionBlock::returnBlock(AqlItemBlock*& block) {
  _engine->_itemBlockManager.returnBlock(block);
}

/// @brief copy register data from one block (src) into another (dst)
/// register values are cloned
void ExecutionBlock::inheritRegisters(AqlItemBlock const* src,
                                      AqlItemBlock* dst, size_t srcRow,
                                      size_t dstRow) {
  DEBUG_BEGIN_BLOCK();
  RegisterId const n = src->getNrRegs();
  auto planNode = getPlanNode();

  for (RegisterId i = 0; i < n; i++) {
    if (planNode->_regsToClear.find(i) == planNode->_regsToClear.end()) {
      auto const& value = src->getValueReference(srcRow, i);

      if (!value.isEmpty()) {
        AqlValue a = value.clone();
        AqlValueGuard guard(a, true);

        dst->setValue(dstRow, i, a);
        guard.steal();
      }
    }
  }
  DEBUG_END_BLOCK();
}

/// @brief copy register data from one block (src) into another (dst)
/// register values are cloned
void ExecutionBlock::inheritRegisters(AqlItemBlock const* src,
                                      AqlItemBlock* dst, size_t row) {
  DEBUG_BEGIN_BLOCK();
  RegisterId const n = src->getNrRegs();
  auto planNode = getPlanNode();

  for (RegisterId i = 0; i < n; i++) {
    if (planNode->_regsToClear.find(i) == planNode->_regsToClear.end()) {
      auto const& value = src->getValueReference(row, i);

      if (!value.isEmpty()) {
        AqlValue a = value.clone();
        AqlValueGuard guard(a, true);

        TRI_IF_FAILURE("ExecutionBlock::inheritRegisters") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }

        dst->setValue(0, i, a);
        guard.steal();
      }
    }
  }

  DEBUG_END_BLOCK();
}

/// @brief the following is internal to pull one more block and append it to
/// our _buffer deque. Returns true if a new block was appended and false if
/// the dependent node is exhausted.
std::pair<ExecutionState, bool> ExecutionBlock::getBlock(size_t atMost) {
  DEBUG_BEGIN_BLOCK();
  throwIfKilled();  // check if we were aborted

  auto res = _dependencies[0]->getSome(atMost);
  if (res.first == ExecutionState::WAITING) {
    return {res.first, false};
  }

  TRI_IF_FAILURE("ExecutionBlock::getBlock") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  _upstreamState = res.first;

  if (res.second != nullptr) {
    _buffer.emplace_back(res.second.get());
    res.second.release();
    return {res.first, true};
  }

  return {res.first, false};
  DEBUG_END_BLOCK();
}

/// @brief getSomeWithoutRegisterClearout, same as above, however, this
/// is the actual worker which does not clear out registers at the end
/// the idea is that somebody who wants to call the generic functionality
/// in a derived class but wants to modify the results before the register
/// cleanup can use this method, internal use only
std::pair<ExecutionState, std::unique_ptr<AqlItemBlock>>
ExecutionBlock::getSomeWithoutRegisterClearout(size_t atMost) {
  DEBUG_BEGIN_BLOCK();
  TRI_ASSERT(atMost > 0);

  std::unique_ptr<AqlItemBlock> result;
  ExecutionState state;
  Result res;

  {
    AqlItemBlock* resultPtr = nullptr;
    size_t skipped = 0;
    std::tie(state, res) = getOrSkipSome(atMost, false, resultPtr, skipped);
    result.reset(resultPtr);
  }

  if (state == ExecutionState::WAITING) {
    TRI_ASSERT(result == nullptr);
    return {ExecutionState::WAITING, nullptr};
  }

  if (res.fail()) {
    THROW_ARANGO_EXCEPTION(res);
  }

  return {state, std::move(result)};
  DEBUG_END_BLOCK();
}

void ExecutionBlock::clearRegisters(AqlItemBlock* result) {
  DEBUG_BEGIN_BLOCK();
  // Clear out registers not needed later on:
  if (result != nullptr) {
    result->clearRegisters(getPlanNode()->_regsToClear);
  }
  DEBUG_END_BLOCK();
}

std::pair<ExecutionState, size_t> ExecutionBlock::skipSome(size_t atMost) {
  size_t skipped = 0;
  AqlItemBlock* result = nullptr;
  auto res = getOrSkipSome(atMost, true, result, skipped);
  TRI_ASSERT(result == nullptr);

  if (res.first == ExecutionState::WAITING) {
    TRI_ASSERT(skipped == 0);
    return {ExecutionState::WAITING, skipped};
  }

  if (res.second.fail()) {
    THROW_ARANGO_EXCEPTION(res.second);
  }

  return {res.first, skipped};
}

ExecutionState ExecutionBlock::hasMoreState() {
  if (_done) {
    return ExecutionState::DONE;
  }
  if (!_buffer.empty()) {
    return ExecutionState::HASMORE;
  }
  ExecutionState state;
  bool blockAppended;
  std::tie(state, blockAppended) = getBlock(DefaultBatchSize());
  if (state == ExecutionState::WAITING) {
    TRI_ASSERT(!blockAppended);
    return ExecutionState::WAITING;
  }
  if (blockAppended) {
    _pos = 0;
    return ExecutionState::HASMORE;
  }
  _done = true;
  return ExecutionState::DONE;
}

std::pair<ExecutionState, arangodb::Result> ExecutionBlock::getOrSkipSome(
    size_t atMost, bool skipping, AqlItemBlock*& result, size_t& skipped_) {
  TRI_ASSERT(result == nullptr && skipped_ == 0);

  // if _buffer.size() is > 0 then _pos points to a valid place . . .

  while (!_done && _skipped < atMost) {
    if (_buffer.empty()) {
      if (skipping) {
        ExecutionState state;
        size_t numActuallySkipped;
        std::tie(state, numActuallySkipped) =
            _dependencies[0]->skipSome(atMost);
        if (state == ExecutionState::WAITING) {
          TRI_ASSERT(numActuallySkipped == 0);
          return {state, TRI_ERROR_NO_ERROR};
        }
        _skipped += numActuallySkipped;
        break;
      } else {
        ExecutionState state;
        bool blockAppended;
        std::tie(state, blockAppended) = getBlock(atMost - _skipped);
        if (state == ExecutionState::WAITING) {
          TRI_ASSERT(!blockAppended);
          TRI_ASSERT(result == nullptr);
          TRI_ASSERT(skipped_ == 0);
          return {ExecutionState::WAITING, TRI_ERROR_NO_ERROR};
        }

        if (!blockAppended) {
          _done = true;
          break;  // must still put things in the result from the collector .
                  // . .
        }
        _pos = 0;
      }
    }

    AqlItemBlock* cur = _buffer.front();

    if (cur->size() - _pos > atMost - _skipped) {
      // The current block is too large for atMost:
      if (!skipping) {
        std::unique_ptr<AqlItemBlock> more(
            cur->slice(_pos, _pos + (atMost - _skipped)));

        TRI_IF_FAILURE("ExecutionBlock::getOrSkipSome1") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }

        _collector.add(std::move(more));
      }
      _pos += atMost - _skipped;
      _skipped = atMost;
    } else if (_pos > 0) {
      // The current block fits into our result, but it is already
      // half-eaten:
      if (!skipping) {
        std::unique_ptr<AqlItemBlock> more(cur->slice(_pos, cur->size()));

        TRI_IF_FAILURE("ExecutionBlock::getOrSkipSome2") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }

        _collector.add(std::move(more));
      }
      _skipped += cur->size() - _pos;
      returnBlock(cur);
      _buffer.pop_front();
      _pos = 0;
    } else {
      // The current block fits into our result and is fresh:
      _skipped += cur->size();
      if (!skipping) {
        // if any of the following statements throw, then cur is not lost,
        // as it is still contained in _buffer
        TRI_IF_FAILURE("ExecutionBlock::getOrSkipSome3") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }
        _collector.add(cur);
      } else {
        returnBlock(cur);
      }
      _buffer.pop_front();
      _pos = 0;
    }
  }

  TRI_ASSERT(result == nullptr);
  
  if (!skipping) {
    result = _collector.steal();
  }
  skipped_ = _skipped;
  _skipped = 0;

  if (skipping && skipped_ == 0) {
    return {ExecutionState::DONE, TRI_ERROR_NO_ERROR};
  }
  if (!skipping && result == nullptr) {
    return {ExecutionState::DONE, TRI_ERROR_NO_ERROR};
  }
  if (_done) {
    return {ExecutionState::DONE, TRI_ERROR_NO_ERROR};
  }
  return {ExecutionState::HASMORE, TRI_ERROR_NO_ERROR};
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
