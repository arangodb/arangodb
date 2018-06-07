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
  DEBUG_BEGIN_BLOCK();
  // Create a deep copy of the register values given to us:
  if (items != nullptr) {
    // build a whitelist with all the registers that we will copy from above
    _inputRegisterValues.reset(items->slice(pos, _whitelist));
  }

  _done = false;
  return {ExecutionState::DONE, TRI_ERROR_NO_ERROR};

  // cppcheck-suppress style
  DEBUG_END_BLOCK();  
}

/// @brief shutdown the singleton block
int SingletonBlock::shutdown(int errorCode) {
  _inputRegisterValues.reset();
  return ExecutionBlock::shutdown(errorCode);
}

std::pair<ExecutionState, arangodb::Result> SingletonBlock::getOrSkipSome(
    size_t atMost, bool skipping, AqlItemBlock*& result, size_t& skipped) {
  DEBUG_BEGIN_BLOCK();  
  traceGetSomeBegin(atMost);
  TRI_ASSERT(result == nullptr && skipped == 0);

  if (_done) {
    TRI_ASSERT(getHasMoreState() == ExecutionState::DONE);
    traceGetSomeEnd(nullptr, ExecutionState::DONE);
    return {ExecutionState::DONE, TRI_ERROR_NO_ERROR};
  }

  if (!skipping) {
    result = requestBlock(1, getPlanNode()->getRegisterPlan()->nrRegs[getPlanNode()->getDepth()]);

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
  traceGetSomeEnd(result, ExecutionState::DONE);
  return {ExecutionState::DONE, TRI_ERROR_NO_ERROR};

  // cppcheck-suppress style
  DEBUG_END_BLOCK();  
}

FilterBlock::FilterBlock(ExecutionEngine* engine, FilterNode const* en)
    : ExecutionBlock(engine, en), 
      _inReg(ExecutionNode::MaxRegisterId),
      _collector(&engine->_itemBlockManager),
      _inflight(0) {

  auto it = en->getRegisterPlan()->varInfo.find(en->_inVariable->id);
  TRI_ASSERT(it != en->getRegisterPlan()->varInfo.end());
  _inReg = it->second.registerId;
  TRI_ASSERT(_inReg < ExecutionNode::MaxRegisterId);
}

FilterBlock::~FilterBlock() {}

std::pair<ExecutionState, arangodb::Result> FilterBlock::initializeCursor(
    AqlItemBlock* items, size_t pos) {
  _inflight = 0;
  return ExecutionBlock::initializeCursor(items, pos);
}
  
/// @brief internal function to actually decide if the document should be used
bool FilterBlock::takeItem(AqlItemBlock* items, size_t index) const {
  return items->getValueReference(index, _inReg).toBoolean();
}

/// @brief internal function to get another block
std::pair<ExecutionState, bool> FilterBlock::getBlock(size_t atMost) {
  DEBUG_BEGIN_BLOCK();  
  while (true) {  // will be left by break or return
    auto res = ExecutionBlock::getBlock(atMost);
    if (res.first == ExecutionState::WAITING ||
        !res.second) {
      return res;
    }

    if (_buffer.size() > 1) {
      break;  // Already have a current block
    }

    // Now decide about these docs:
    AqlItemBlock* cur = _buffer.front();

    _chosen.clear();
    _chosen.reserve(cur->size());
    for (size_t i = 0; i < cur->size(); ++i) {
      if (takeItem(cur, i)) {
        TRI_IF_FAILURE("FilterBlock::getBlock") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }

        _chosen.emplace_back(i);
      }
    }

    _engine->_stats.filtered += (cur->size() - _chosen.size());

    if (!_chosen.empty()) {
      break;  // OK, there are some docs in the result
    }

    _buffer.pop_front();  // Block was useless, just try again
    returnBlock(cur);     // free this block
  
    throwIfKilled();  // check if we were aborted
  }

  return {_upstreamState, true};

  // cppcheck-suppress style
  DEBUG_END_BLOCK();  
}

std::pair<ExecutionState, arangodb::Result> FilterBlock::getOrSkipSome(
    size_t atMost, bool skipping, AqlItemBlock*& result, size_t& skipped) {
  DEBUG_BEGIN_BLOCK();
  TRI_ASSERT(result == nullptr && skipped == 0);

  if (_done) {
    return {ExecutionState::DONE, TRI_ERROR_NO_ERROR};
  }

  while (_inflight < atMost) {
    if (_buffer.empty()) {
      auto upstreamRes = getBlock(atMost - _inflight);
      if (upstreamRes.first == ExecutionState::WAITING) {
        // We have not modified result or skipped up to now.
        // Make sure the caller does not have to retain it.
        TRI_ASSERT(result == nullptr && skipped == 0);
        return {ExecutionState::WAITING, TRI_ERROR_NO_ERROR};
      }

      if (!upstreamRes.second) {
        _done = true;
        break;
      }
      _pos = 0;
    }

    // If we get here, then _buffer.size() > 0 and _pos points to a
    // valid place in it.
    AqlItemBlock* cur = _buffer.front();
    if (_chosen.size() - _pos + _inflight > atMost) {
      // The current block of chosen ones is too large for atMost:
      if (!skipping) {
        std::unique_ptr<AqlItemBlock> more(
            cur->slice(_chosen, _pos, _pos + (atMost - _inflight)));

        TRI_IF_FAILURE("FilterBlock::getOrSkipSome1") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }

        _collector.add(std::move(more));
      }
      _pos += atMost - _inflight;
      _inflight = atMost;
    } else if (_pos > 0 || _chosen.size() < cur->size()) {
      // The current block fits into our result, but it is already
      // half-eaten or needs to be copied anyway:
      if (!skipping) {
        std::unique_ptr<AqlItemBlock> more(
            cur->steal(_chosen, _pos, _chosen.size()));

        TRI_IF_FAILURE("FilterBlock::getOrSkipSome2") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }

        _collector.add(std::move(more));
      }
      _inflight += _chosen.size() - _pos;
      returnBlock(cur);
      _buffer.pop_front();
      _chosen.clear();
      _pos = 0;
    } else {
      // The current block fits into our result and is fresh and
      // takes them all, so we can just hand it on:
      _inflight += cur->size();
      if (!skipping) {
        // if any of the following statements throw, then cur is not lost,
        // as it is still contained in _buffer
        TRI_IF_FAILURE("FilterBlock::getOrSkipSome3") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }
        _collector.add(cur);
      } else {
        returnBlock(cur);
      }
      _buffer.pop_front();
      _chosen.clear();
      _pos = 0;
    }
  }

  if (!skipping) {
    result = _collector.steal();
  }
  skipped = _inflight;

  // if _buffer.size() is > 0 then _pos is valid
  _collector.clear();
  _inflight = 0;

  return {getHasMoreState(), TRI_ERROR_NO_ERROR};

  // cppcheck-suppress style
  DEBUG_END_BLOCK();  
}

std::pair<ExecutionState, arangodb::Result> LimitBlock::initializeCursor(
    AqlItemBlock* items, size_t pos) {
  DEBUG_BEGIN_BLOCK();  

  _state = INITFULLCOUNT;
  _count = 0;
  return ExecutionBlock::initializeCursor(items, pos);

  // cppcheck-suppress style
  DEBUG_END_BLOCK();  
}

std::pair<ExecutionState, arangodb::Result> LimitBlock::getOrSkipSome(
    size_t atMost, bool skipping, AqlItemBlock*& result, size_t& skipped) {
  DEBUG_BEGIN_BLOCK();
  TRI_ASSERT(result == nullptr && skipped == 0);

  switch (_state) {
    case DONE:
      TRI_ASSERT(getHasMoreState() == ExecutionState::DONE);
      return {ExecutionState::DONE, TRI_ERROR_NO_ERROR};
    case INITFULLCOUNT: {
      if (_fullCount) {
        _engine->_stats.fullCount = 0;
      }
      _state = SKIPPING;
      // Fallthrough intententional
    }
    case SKIPPING: {
      if (_offset > 0) {
        size_t numActuallySkipped = 0;
        _dependencies[0]->skip(_offset, numActuallySkipped);
        if (_fullCount) {
          _engine->_stats.fullCount += static_cast<int64_t>(numActuallySkipped);
        }
      }
      _count = 0;
      if (_limit == 0 && !_fullCount) {
        // quick exit for limit == 0
        _state = DONE;
        TRI_ASSERT(getHasMoreState() == ExecutionState::DONE);
        return {ExecutionState::DONE, TRI_ERROR_NO_ERROR};
      }
      _state = RETURNING;
      // Fallthrough intententional
    }
    case RETURNING: {
      if (_count < _limit) {
        if (atMost > _limit - _count) {
          atMost = _limit - _count;
        }
        auto state = ExecutionBlock::getOrSkipSome(atMost, skipping, result, skipped);
        if (state.first == ExecutionState::WAITING) {
          // On WAITING we will continue here, on error we bail out
          return state;
        }

        if (skipped == 0) {
          _state = DONE;
          TRI_ASSERT(getHasMoreState() == ExecutionState::DONE);
          return {ExecutionState::DONE, TRI_ERROR_NO_ERROR};
        }

        _count += skipped;
        if (_fullCount) {
          _engine->_stats.fullCount += static_cast<int64_t>(skipped);
        }
      }
      if (_count >= _limit) {
        if (_fullCount) {
          // if fullCount is set, we must fetch all elements from the
          // dependency. we'll use the default batch size for this
          atMost = DefaultBatchSize();

          // suck out all data from the dependencies
          while (true) {
            skipped = 0;
            AqlItemBlock* ignore = nullptr;
            auto res = ExecutionBlock::getOrSkipSome(atMost, skipping, ignore, skipped);
            if (res.first == ExecutionState::WAITING || !res.second.ok()) {
              TRI_ASSERT(ignore == nullptr);
              // On WAITING we will continue here, on error we bail out
              return res;
            }

            TRI_ASSERT(ignore == nullptr || ignore->size() == skipped);
            delete ignore;
            _engine->_stats.fullCount += skipped;

            if (skipped == 0) {
              _state = DONE;
              break;
            }
          }
        } else {
          _state = DONE;
        }
      }
    }
  }

  return {getHasMoreState(), TRI_ERROR_NO_ERROR};

  // cppcheck-suppress style
  DEBUG_END_BLOCK();
}

ExecutionState LimitBlock::getHasMoreState() {
  if (_state == DONE) {
    return ExecutionState::DONE;
  }
  return ExecutionState::HASMORE;
}

std::pair<ExecutionState, std::unique_ptr<AqlItemBlock>> ReturnBlock::getSome(
    size_t atMost) {
  DEBUG_BEGIN_BLOCK();
  traceGetSomeBegin(atMost);
  
  auto ep = ExecutionNode::castTo<ReturnNode const*>(getPlanNode());

  auto res = ExecutionBlock::getSomeWithoutRegisterClearout(atMost);
  if (res.first == ExecutionState::WAITING) {
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

  // cppcheck-suppress style
  DEBUG_END_BLOCK();
}

/// @brief make the return block return the results inherited from above,
/// without creating new blocks
/// returns the id of the register the final result can be found in
RegisterId ReturnBlock::returnInheritedResults() {
  DEBUG_BEGIN_BLOCK();
  _returnInheritedResults = true;

  auto ep = ExecutionNode::castTo<ReturnNode const*>(getPlanNode());
  auto it = ep->getRegisterPlan()->varInfo.find(ep->_inVariable->id);
  TRI_ASSERT(it != ep->getRegisterPlan()->varInfo.end());

  return it->second.registerId;

  // cppcheck-suppress style
  DEBUG_END_BLOCK();
}

/// @brief initializeCursor, only call base
std::pair<ExecutionState, arangodb::Result> NoResultsBlock::initializeCursor(
    AqlItemBlock* items, size_t pos) {
  DEBUG_BEGIN_BLOCK();  
  _done = true;
  return {ExecutionState::DONE, TRI_ERROR_NO_ERROR};

  // cppcheck-suppress style
  DEBUG_END_BLOCK();  
}

std::pair<ExecutionState, arangodb::Result> NoResultsBlock::getOrSkipSome(
    size_t,  // atMost
    bool,    // skipping
    AqlItemBlock*& result, size_t& skipped) {
  TRI_ASSERT(result == nullptr && skipped == 0);
  return {ExecutionState::DONE, TRI_ERROR_NO_ERROR};
}
