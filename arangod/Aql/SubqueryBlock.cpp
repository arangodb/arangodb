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

#include "SubqueryBlock.h"
#include "Aql/AqlItemBlock.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/Query.h"
#include "Basics/Exceptions.h"
#include "VocBase/vocbase.h"

using namespace arangodb::aql;

SubqueryBlock::SubqueryBlock(ExecutionEngine* engine, SubqueryNode const* en,
                             ExecutionBlock* subquery)
    : ExecutionBlock(engine, en),
      _outReg(ExecutionNode::MaxRegisterId),
      _subquery(subquery),
      _subqueryIsConst(const_cast<SubqueryNode*>(en)->isConst()),
      _subqueryReturnsData(_subquery->getPlanNode()->getType() == ExecutionNode::RETURN),
      _result(nullptr),
      _subqueryPos(0) {
  auto it = en->getRegisterPlan()->varInfo.find(en->_outVariable->id);
  TRI_ASSERT(it != en->getRegisterPlan()->varInfo.end());
  _outReg = it->second.registerId;
  TRI_ASSERT(_outReg < ExecutionNode::MaxRegisterId);
}

ExecutionState SubqueryBlock::initSubquery(size_t position) {
  auto ret = _subquery->initializeCursor(_result.get(), position);
  if (ret.first == ExecutionState::WAITING) {
    // Position is captured, we can continue from here again
    return ret.first;
  }
  _subqueryInitialized = true;

  if (!ret.second.ok()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(ret.second.errorNumber(),
                                   ret.second.errorMessage());
  }
  return ExecutionState::DONE;
}

ExecutionState SubqueryBlock::getSomeConstSubquery(size_t atMost) {
  if (_result->size() == 0) {
    // NOTHING to loop
    return ExecutionState::DONE;
  }
  if (!_subqueryInitialized) {
    auto state = initSubquery(0);
    if (state == ExecutionState::WAITING) {
      TRI_ASSERT(!_subqueryInitialized);
      return state;
    }
    TRI_ASSERT(state == ExecutionState::DONE);
  }
  if (!_subqueryCompleted) {
    auto state = executeSubquery();
    if (state == ExecutionState::WAITING) {
      // If this assert is violated we will not end up in executeSubQuery again.
      TRI_ASSERT(!_subqueryCompleted);
      // We need to wait
      return state;
    }
    // Subquery does not allow to HASMORE!
    TRI_ASSERT(state == ExecutionState::DONE);
  }

  // We have exactly one constant result just reuse it.
  TRI_ASSERT(_subqueryCompleted);
  TRI_ASSERT(_subqueryResults != nullptr);

  for (; _subqueryPos < _result->size(); _subqueryPos++) {
    TRI_IF_FAILURE("SubqueryBlock::getSome") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
    _result->emplaceValue(_subqueryPos, _outReg, _subqueryResults.get());
    _subqueryResults.release();
    throwIfKilled();
  }
  // We are done for this _result. Fetch next _result from upstream
  // to determine if we are DONE or HASMORE
  return ExecutionState::DONE;
}


ExecutionState SubqueryBlock::getSomeNonConstSubquery(size_t atMost) {
  if (_result->size() == 0) {
    // NOTHING to loop
    return ExecutionState::DONE;
  }
  for (; _subqueryPos < _result->size(); _subqueryPos++) {
    if (!_subqueryInitialized) {
      auto state = initSubquery(_subqueryPos);
      if (state == ExecutionState::WAITING) {
        TRI_ASSERT(!_subqueryInitialized);
        return state;
      }
      TRI_ASSERT(state == ExecutionState::DONE);
    }
    if (!_subqueryCompleted) {
      auto state = executeSubquery();
      if (state == ExecutionState::WAITING) {
        // If this assert is violated we will not end up in executeSubQuery again.
        TRI_ASSERT(!_subqueryCompleted);
        // We need to wait
        return state;
      }
      // Subquery does not allow to HASMORE!
      TRI_ASSERT(state == ExecutionState::DONE);
    }

    // We have exactly one constant result just reuse it.
    TRI_ASSERT(_subqueryCompleted);
    TRI_ASSERT(_subqueryResults != nullptr);

    TRI_IF_FAILURE("SubqueryBlock::getSome") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
    _result->emplaceValue(_subqueryPos, _outReg, _subqueryResults.get());
    // Responsibility is handed over
    _subqueryResults.reset();
    TRI_ASSERT(_subqueryResults == nullptr);
    _subqueryCompleted = false;
    _subqueryInitialized = false;
    throwIfKilled();
  }

  // We are done for this _result. Fetch next _result from upstream
  // to determine if we are DONE or HASMORE
  return ExecutionState::DONE;
}

/// @brief getSome
std::pair<ExecutionState, std::unique_ptr<AqlItemBlock>> SubqueryBlock::getSome(size_t atMost) {
  DEBUG_BEGIN_BLOCK();
  traceGetSomeBegin(atMost);
  if (_result.get() == nullptr) {
    auto res = ExecutionBlock::getSomeWithoutRegisterClearout(atMost);
    if (res.first == ExecutionState::WAITING) {
      // NOTE: _result stays a nullptr! We end up in here again!
      return res;
    }
    _result.swap(res.second);

    if (_result.get() == nullptr) {
      traceGetSomeEnd(nullptr);
      return {ExecutionState::DONE, nullptr};
    }
    _upstreamState = res.first;
  }

  ExecutionState state;
  if (_subqueryIsConst) {
    state = getSomeConstSubquery(atMost);
  } else {
    state = getSomeNonConstSubquery(atMost);
  }

  if (state == ExecutionState::WAITING) {
    // We need to wait, please call again
    return {state, nullptr};
  }

  // Need to reset to position zero here
  _subqueryPos = 0;

  // Clear out registers no longer needed later:
  clearRegisters(_result.get());
  traceGetSomeEnd(_result.get());
  // Resets _result to nullptr
  return {_upstreamState, std::move(_result)};
  // cppcheck-suppress style
  DEBUG_END_BLOCK();
}

/// @brief shutdown, tell dependency and the subquery
int SubqueryBlock::shutdown(int errorCode) {
  int res = ExecutionBlock::shutdown(errorCode);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  return getSubquery()->shutdown(errorCode);
}

/// @brief execute the subquery and store it's results in _subqueryResults
ExecutionState SubqueryBlock::executeSubquery() {
  DEBUG_BEGIN_BLOCK();
  if (_subqueryResults == nullptr) {
    _subqueryResults = std::make_unique<std::vector<std::unique_ptr<AqlItemBlock>>>();
  }
  TRI_ASSERT(!_subqueryCompleted);

  TRI_ASSERT(_subqueryResults != nullptr);
  do {
    auto res = _subquery->getSome(DefaultBatchSize());
    if (res.first == ExecutionState::WAITING) {
      TRI_ASSERT(res.second == nullptr);
      return res.first;
    }
    if (res.second != nullptr) {
      TRI_IF_FAILURE("SubqueryBlock::executeSubquery") {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }
      TRI_ASSERT(_subqueryResults != nullptr);
      if (_subqueryReturnsData) {
        _subqueryResults->emplace_back(std::move(res.second));
      }
    }
    if (res.first == ExecutionState::DONE) {
      _subqueryCompleted = true;
      return ExecutionState::DONE;
    }
    // If this ASSERTION kicks in one Block has a logic bug
    // and returns no results in conjunction with HASMORE.
    TRI_ASSERT(res.second != nullptr);
  } while (true);
  DEBUG_END_BLOCK();
}
