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

using namespace arangodb;
using namespace arangodb::aql;

SubqueryBlock::SubqueryBlock(ExecutionEngine* engine, SubqueryNode const* en,
                             ExecutionBlock* subquery)
    : ExecutionBlock(engine, en),
      _outReg(ExecutionNode::MaxRegisterId),
      _subquery(subquery),
      _subqueryIsConst(const_cast<SubqueryNode*>(en)->isConst()),
      _subqueryReturnsData(_subquery->getPlanNode()->getType() == ExecutionNode::RETURN),
      _result(nullptr),
      _subqueryResults(nullptr),
      _subqueryPos(0),
      _subqueryInitialized(false),
      _subqueryCompleted(false),
      _hasShutdownMainQuery(false) {
  auto it = en->getRegisterPlan()->varInfo.find(en->_outVariable->id);
  TRI_ASSERT(it != en->getRegisterPlan()->varInfo.end());
  _outReg = it->second.registerId;
  TRI_ASSERT(_outReg < ExecutionNode::MaxRegisterId);
}

ExecutionState SubqueryBlock::initSubquery(size_t position) {
  TRI_ASSERT(!_subqueryInitialized);
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
  auto subResult = _subqueryResults.get();

  for (; _subqueryPos < _result->size(); _subqueryPos++) {
    TRI_IF_FAILURE("SubqueryBlock::getSome") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
    _result->emplaceValue(_subqueryPos, _outReg, subResult);
    // From now on we need to forget this query as only this one block
    // is responsible. Unfortunately we need to recompute the subquery
    // next time again, otherwise the memory management is broken
    // and creates double-free or even worse use-after-free.
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
    _subqueryResults.release();
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
  traceGetSomeBegin(atMost);
  if (_result.get() == nullptr) {
    auto res = ExecutionBlock::getSomeWithoutRegisterClearout(atMost);
    if (res.first == ExecutionState::WAITING) {
      // NOTE: _result stays a nullptr! We end up in here again!
      traceGetSomeEnd(nullptr, ExecutionState::WAITING);
      return res;
    }

    _result.swap(res.second);
    _upstreamState = res.first;

    if (_result.get() == nullptr) {
      TRI_ASSERT(getHasMoreState() == ExecutionState::DONE);
      traceGetSomeEnd(nullptr, ExecutionState::DONE);
      return {ExecutionState::DONE, nullptr};
    }
  }

  ExecutionState state;
  if (_subqueryIsConst) {
    state = getSomeConstSubquery(atMost);
  } else {
    state = getSomeNonConstSubquery(atMost);
  }

  if (state == ExecutionState::WAITING) {
    // We need to wait, please call again
    traceGetSomeEnd(nullptr, ExecutionState::WAITING);
    return {state, nullptr};
  }

  // Need to reset to position zero here
  _subqueryPos = 0;

  // Clear out registers no longer needed later:
  clearRegisters(_result.get());
  // If we get here, we have handed over responsibilty for all subquery results
  // computed here to this specific result. We cannot reuse them in the next
  // getSome call, hence we need to reset it.
  _subqueryInitialized = false;
  _subqueryCompleted = false;
  _subqueryResults.release();


  traceGetSomeEnd(_result.get(), getHasMoreState());

  // Resets _result to nullptr
  return {getHasMoreState(), std::move(_result)};
}

/// @brief shutdown, tell dependency and the subquery
std::pair<ExecutionState, Result> SubqueryBlock::shutdown(int errorCode) {
  if (!_hasShutdownMainQuery) {
    ExecutionState state;
    Result res;
    std::tie(state, res) = ExecutionBlock::shutdown(errorCode);
    if (state == ExecutionState::WAITING) {
      TRI_ASSERT(res.ok());
      return {state, res};
    }
    TRI_ASSERT(state == ExecutionState::DONE);
    _hasShutdownMainQuery = true;
    _mainQueryShutdownResult = res;
  }

  ExecutionState state;
  Result res;
  std::tie(state, res) = getSubquery()->shutdown(errorCode);

  if (state == ExecutionState::WAITING) {
    TRI_ASSERT(res.ok());
    return {state, res};
  }
  TRI_ASSERT(state == ExecutionState::DONE);

  if (_mainQueryShutdownResult.fail()) {
    return {state, _mainQueryShutdownResult};
  }

  return {state, res};
}

/// @brief execute the subquery and store it's results in _subqueryResults
ExecutionState SubqueryBlock::executeSubquery() {
  TRI_ASSERT(!_subqueryCompleted);
  if (_subqueryResults == nullptr) {
    _subqueryResults = std::make_unique<std::vector<std::unique_ptr<AqlItemBlock>>>();
  }

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
  } while (true);
}
