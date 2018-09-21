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

#include "BasicBlocks.h"
#include "Aql/AqlItemBlock.h"
#include "Aql/ExecutionEngine.h"
#include "Basics/Exceptions.h"
#include "VocBase/vocbase.h"

using namespace arangodb::aql;

SingletonBlock::SingletonBlock(ExecutionEngine* engine, SingletonNode const* ep)
    : ExecutionBlock(engine, ep) {
  auto en = ExecutionNode::castTo<SingletonNode const*>(getPlanNode());
  auto const& registerPlan = en->getRegisterPlan()->varInfo;
  std::unordered_set<Variable const*> const& varsUsedLater = en->getVarsUsedLater();

  for (auto const& it : varsUsedLater) {
    auto it2 = registerPlan.find(it->id);

    if (it2 != registerPlan.end()) {
      _whitelist.emplace((*it2).second.registerId);
    }
  }
}
  
/// @brief initializeCursor, store a copy of the register values coming from
/// above
std::pair<ExecutionState, arangodb::Result> SingletonBlock::initializeCursor(
    AqlItemBlock* items, size_t pos) {
  // Create a deep copy of the register values given to us:
  if (items != nullptr) {
    // build a whitelist with all the registers that we will copy from above
    _inputRegisterValues.reset(items->slice(pos, _whitelist));
  }

  _done = false;
  return {ExecutionState::DONE, TRI_ERROR_NO_ERROR};
}

std::pair<ExecutionState, arangodb::Result> SingletonBlock::getOrSkipSome(
    size_t atMost, bool skipping, AqlItemBlock*& result, size_t& skipped) {
  TRI_ASSERT(result == nullptr && skipped == 0);

  if (_done) {
    TRI_ASSERT(getHasMoreState() == ExecutionState::DONE);
    return {ExecutionState::DONE, TRI_ERROR_NO_ERROR};
  }

  if (!skipping) {
    result = requestBlock(1, getNrOutputRegisters());

    try {
      if (_inputRegisterValues != nullptr) {
        skipped++;
        for (RegisterId reg = 0; reg < _inputRegisterValues->getNrRegs();
             ++reg) {
          if (_whitelist.find(reg) == _whitelist.end()) {
            continue;
          }
          TRI_IF_FAILURE("SingletonBlock::getOrSkipSome") {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
          }

          AqlValue a = _inputRegisterValues->getValue(0, reg);
          if (a.isEmpty()) {
            continue;
          }
          _inputRegisterValues->steal(a);

          try {
            TRI_IF_FAILURE("SingletonBlock::getOrSkipSomeSet") {
              THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
            }

            result->setValue(0, reg, a);
          } catch (...) {
            a.destroy();
            throw;
          }
          _inputRegisterValues->eraseValue(0, reg);
        }
      }
    } catch (...) {
      delete result;
      result = nullptr;
      throw;
    }
  } else {
    if (_inputRegisterValues != nullptr) {
      skipped++;
    }
  }

  _done = true;
  TRI_ASSERT(getHasMoreState() == ExecutionState::DONE);
  return {ExecutionState::DONE, TRI_ERROR_NO_ERROR};
}

std::pair<ExecutionState, arangodb::Result> LimitBlock::initializeCursor(
    AqlItemBlock* items, size_t pos) {
  _state = State::INITFULLCOUNT;
  _count = 0;
  _remainingOffset = _offset;
  _result = nullptr;
  _limitSkipped = 0;

  return ExecutionBlock::initializeCursor(items, pos);
}

std::pair<ExecutionState, arangodb::Result> LimitBlock::getOrSkipSome(
    size_t atMost, bool skipping, AqlItemBlock*& result_, size_t& skipped_) {
  TRI_ASSERT(result_ == nullptr && skipped_ == 0);

  switch (_state) {
    case State::DONE:
      TRI_ASSERT(getHasMoreState() == ExecutionState::DONE);
      return {ExecutionState::DONE, TRI_ERROR_NO_ERROR};
    case State::INITFULLCOUNT: {
      if (_fullCount) {
        _engine->_stats.fullCount = 0;
      }
      _state = State::SKIPPING;
    }
    // intentionally falls through
    case State::SKIPPING: {
      while (_remainingOffset > 0) {
        auto res = _dependencies[0]->skipSome(_remainingOffset);
        if (res.first == ExecutionState::WAITING) {
          return {ExecutionState::WAITING, TRI_ERROR_NO_ERROR};
        }
        if (_fullCount) {
          _engine->_stats.fullCount += static_cast<int64_t>(res.second);
        }
        TRI_ASSERT(_remainingOffset >= res.second);
        _remainingOffset -= res.second;
        if (res.first == ExecutionState::DONE) {
          break;
        }
      }
      _count = 0;
      if (_limit == 0 && !_fullCount) {
        // quick exit for limit == 0
        _state = State::DONE;
        TRI_ASSERT(getHasMoreState() == ExecutionState::DONE);
        return {ExecutionState::DONE, TRI_ERROR_NO_ERROR};
      }
      _state = State::RETURNING;
    }
    // intentionally falls through
    case State::RETURNING: {
      if (_count < _limit) {
        if (atMost > _limit - _count) {
          atMost = _limit - _count;
        }
        AqlItemBlock* res = nullptr;
        TRI_ASSERT(_limitSkipped == 0);
        auto state = ExecutionBlock::getOrSkipSome(atMost, skipping, res,
          _limitSkipped);
        TRI_ASSERT(_result == nullptr);
        _result.reset(res);
        if (state.first == ExecutionState::WAITING) {
          TRI_ASSERT(result_ == nullptr);
          // On WAITING we will continue here, on error we bail out
          return state;
        }

        if (_limitSkipped == 0) {
          _state = State::DONE;
          TRI_ASSERT(getHasMoreState() == ExecutionState::DONE);
          result_ = _result.release();
          // there should be no need to assign skipped_ = _limitSkipped and
          // _limitSkipped = 0 here:
          TRI_ASSERT(skipped_ == 0);
          return {ExecutionState::DONE, TRI_ERROR_NO_ERROR};
        }

        _count += _limitSkipped;
        if (_fullCount) {
          _engine->_stats.fullCount += static_cast<int64_t>(_limitSkipped);
        }
      }
      if (_count >= _limit) {
        if (!_fullCount) {
          _state = State::DONE;
        } else {
          // if fullCount is set, we must fetch all elements from the
          // dependency.

          // suck out all data from the dependencies
          while (_state != State::DONE) {
            AqlItemBlock* ignore = nullptr;
            // local skipped count
            size_t skipped = 0;
            // We're only counting here, so skip. Using the DefaultBatchSize
            // instead of atMost because we need to skip everything if we have
            // to calculate fullCount.
            auto res = ExecutionBlock::getOrSkipSome(DefaultBatchSize(), true,
                                                     ignore, skipped);
            TRI_ASSERT(ignore == nullptr);

            ExecutionState state = res.first;
            Result result = res.second;
            if (state == ExecutionState::WAITING || result.fail()) {
              // On WAITING we will continue here, on error we bail out
              return res;
            }

            _engine->_stats.fullCount += skipped;

            if (skipped == 0) {
              _state = State::DONE;
            }
          }
        }
      }
    }
  }

  result_ = _result.release();
  skipped_ = _limitSkipped;
  _limitSkipped = 0;

  return {getHasMoreState(), TRI_ERROR_NO_ERROR};
}

ExecutionState LimitBlock::getHasMoreState() {
  if (_state == State::DONE) {
    return ExecutionState::DONE;
  }
  return ExecutionState::HASMORE;
}

std::pair<ExecutionState, std::unique_ptr<AqlItemBlock>> ReturnBlock::getSome(
    size_t atMost) {
  traceGetSomeBegin(atMost);
  
  auto ep = ExecutionNode::castTo<ReturnNode const*>(getPlanNode());

  auto res = ExecutionBlock::getSomeWithoutRegisterClearout(atMost);
  if (res.first == ExecutionState::WAITING) {
    traceGetSomeEnd(nullptr, ExecutionState::WAITING);
    return res;
  }

  if (res.second == nullptr) {
    traceGetSomeEnd(nullptr, ExecutionState::DONE);
    return {ExecutionState::DONE, nullptr};
  }

  if (_returnInheritedResults) {
    if (ep->_count) {
    _engine->_stats.count += static_cast<int64_t>(res.second->size());
    }
    traceGetSomeEnd(res.second.get(), res.first);
    return res;
  }

  size_t const n = res.second->size();

  // Let's steal the actual result and throw away the vars:
  auto it = ep->getRegisterPlan()->varInfo.find(ep->_inVariable->id);
  TRI_ASSERT(it != ep->getRegisterPlan()->varInfo.end());
  RegisterId const registerId = it->second.registerId;

  std::unique_ptr<AqlItemBlock> stripped(requestBlock(n, 1));

  for (size_t i = 0; i < n; i++) {
    auto a = res.second->getValueReference(i, registerId);

    if (!a.isEmpty()) {
      if (a.requiresDestruction()) {
        res.second->steal(a);

        try {
          TRI_IF_FAILURE("ReturnBlock::getSome") {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
          }

          stripped->setValue(i, 0, a);
        } catch (...) {
          a.destroy();
          throw;
        }
        // If the following does not go well, we do not care, since
        // the value is already stolen and installed in stripped
        res.second->eraseValue(i, registerId);
      } else {
        stripped->setValue(i, 0, a);
      }
    }
  }
        
  if (ep->_count) {
    _engine->_stats.count += static_cast<int64_t>(n);
  }

  traceGetSomeEnd(stripped.get(), res.first);
  return {res.first, std::move(stripped)};
}

/// @brief make the return block return the results inherited from above,
/// without creating new blocks
/// returns the id of the register the final result can be found in
RegisterId ReturnBlock::returnInheritedResults() {
  _returnInheritedResults = true;

  auto ep = ExecutionNode::castTo<ReturnNode const*>(getPlanNode());
  auto it = ep->getRegisterPlan()->varInfo.find(ep->_inVariable->id);
  TRI_ASSERT(it != ep->getRegisterPlan()->varInfo.end());

  return it->second.registerId;
}

/// @brief initializeCursor, only call base
std::pair<ExecutionState, arangodb::Result> NoResultsBlock::initializeCursor(
    AqlItemBlock* items, size_t pos) {
  _done = true;
  return {ExecutionState::DONE, TRI_ERROR_NO_ERROR};
}

std::pair<ExecutionState, arangodb::Result> NoResultsBlock::getOrSkipSome(
    size_t,  // atMost
    bool,    // skipping
    AqlItemBlock*& result, size_t& skipped) {
  TRI_ASSERT(result == nullptr && skipped == 0);
  return {ExecutionState::DONE, TRI_ERROR_NO_ERROR};
}
