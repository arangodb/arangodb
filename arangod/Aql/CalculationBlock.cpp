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

#include "CalculationBlock.h"
#include "Aql/AqlItemBlock.h"
#include "Aql/BaseExpressionContext.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/Functions.h"
#include "Aql/Query.h"
#include "Basics/Exceptions.h"
#include "Basics/VelocyPackHelper.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Methods.h"
#include "V8/v8-globals.h"
#include "VocBase/vocbase.h"

using namespace arangodb::aql;

CalculationBlock::CalculationBlock(ExecutionEngine* engine, CalculationNode const* en)
    : ExecutionBlock(engine, en),
      _expression(en->expression()),
      _inVars(),
      _inRegs(),
      _outReg(ExecutionNode::MaxRegisterId),
      _conditionReg(ExecutionNode::MaxRegisterId),
      _isRunningInCluster(ServerState::instance()->isRunningInCluster()) {
  std::unordered_set<Variable const*> inVars;
  _expression->variables(inVars);

  _inVars.reserve(inVars.size());
  _inRegs.reserve(inVars.size());

  for (auto& it : inVars) {
    _inVars.emplace_back(it);
    auto it2 = en->getRegisterPlan()->varInfo.find(it->id);

    TRI_ASSERT(it2 != en->getRegisterPlan()->varInfo.end());
    TRI_ASSERT(it2->second.registerId < ExecutionNode::MaxRegisterId);
    _inRegs.emplace_back(it2->second.registerId);
  }

  // check if the expression is "only" a reference to another variable
  // this allows further optimizations inside doEvaluation
  _isReference = (_expression->node()->type == NODE_TYPE_REFERENCE);

  if (_isReference) {
    TRI_ASSERT(_inRegs.size() == 1);
  }

  auto it3 = en->getRegisterPlan()->varInfo.find(en->_outVariable->id);
  TRI_ASSERT(it3 != en->getRegisterPlan()->varInfo.end());
  _outReg = it3->second.registerId;
  TRI_ASSERT(_outReg < ExecutionNode::MaxRegisterId);

  if (en->_conditionVariable != nullptr) {
    auto it = en->getRegisterPlan()->varInfo.find(en->_conditionVariable->id);
    TRI_ASSERT(it != en->getRegisterPlan()->varInfo.end());
    _conditionReg = it->second.registerId;
    TRI_ASSERT(_conditionReg < ExecutionNode::MaxRegisterId);
  }
}

CalculationBlock::~CalculationBlock() {}

/// @brief fill the target register in the item block with a reference to
/// another variable
void CalculationBlock::fillBlockWithReference(AqlItemBlock* result) {
  size_t const n = result->size();
  for (size_t i = 0; i < n; i++) {
    // need not clone to avoid a copy, the AqlItemBlock's hash takes
    // care of correct freeing:
    auto a = result->getValueReference(i, _inRegs[0]);

    TRI_IF_FAILURE("CalculationBlock::fillBlockWithReference") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
    result->setValue(i, _outReg, a);
  }
}

/// @brief shared code for executing a simple or a V8 expression
void CalculationBlock::executeExpression(AqlItemBlock* result) {
  bool const hasCondition =
      (ExecutionNode::castTo<CalculationNode const*>(_exeNode)->_conditionVariable != nullptr);
  TRI_ASSERT(!hasCondition);  // currently not implemented

  Query* query = _engine->getQuery();
  size_t const n = result->size();

  for (size_t i = 0; i < n; i++) {
    // check the condition variable (if any)
    if (hasCondition) {
      AqlValue const& conditionResult = result->getValueReference(i, _conditionReg);

      if (!conditionResult.toBoolean()) {
        TRI_IF_FAILURE("CalculationBlock::executeExpressionWithCondition") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }
        result->emplaceValue(i, _outReg, arangodb::velocypack::Slice::nullSlice());
        continue;
      }
    }

    // execute the expression
    bool mustDestroy;
    BaseExpressionContext ctx(query, i, result, _inVars, _inRegs);
    AqlValue a = _expression->execute(_trx, &ctx, mustDestroy);
    AqlValueGuard guard(a, mustDestroy);

    TRI_IF_FAILURE("CalculationBlock::executeExpression") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
    result->setValue(i, _outReg, a);
    guard.steal();    // itemblock has taken over now
    throwIfKilled();  // check if we were aborted
  }
}

/// @brief doEvaluation, private helper to do the work
void CalculationBlock::doEvaluation(AqlItemBlock* result) {
  TRI_ASSERT(result != nullptr);

  if (_isReference) {
    // the calculation is a reference to a variable only.
    // no need to execute the expression at all
    fillBlockWithReference(result);
    throwIfKilled();  // check if we were aborted
    return;
  }

  // non-reference expression

  TRI_ASSERT(_expression != nullptr);

  if (!_expression->willUseV8()) {
    // an expression that does not require V8
    executeExpression(result);
  } else {
    auto cleanup = [this]() {
      if (_isRunningInCluster) {
        // must invalidate the expression now as we might be called from
        // different threads
        _expression->invalidate();

        _engine->getQuery()->exitContext();
      }
    };

    // must have a V8 context here to protect Expression::execute()
    _engine->getQuery()->enterContext();
    TRI_DEFER(cleanup());

    ISOLATE;
    v8::HandleScope scope(isolate);  // do not delete this!

    // do not merge the following function call with the same function call
    // above!
    // the V8 expression execution must happen in the scope that contains
    // the V8 handle scope and the scope guard
    executeExpression(result);
  }
}

std::pair<ExecutionState, std::unique_ptr<AqlItemBlock>> CalculationBlock::getSome(size_t atMost) {
  traceGetSomeBegin(atMost);

  if (_done) {
    traceGetSomeEnd(nullptr, ExecutionState::DONE);
    return {ExecutionState::DONE, nullptr};
  }

  auto res = ExecutionBlock::getSomeWithoutRegisterClearout(atMost);
  if (res.first == ExecutionState::WAITING) {
    traceGetSomeEnd(nullptr, ExecutionState::WAITING);
    return res;
  }
  if (res.second == nullptr) {
    TRI_ASSERT(res.first == ExecutionState::DONE);
    traceGetSomeEnd(nullptr, res.first);
    return res;
  }

  doEvaluation(res.second.get());
  // Clear out registers no longer needed later:
  clearRegisters(res.second.get());
  traceGetSomeEnd(res.second.get(), res.first);
  return res;
}
