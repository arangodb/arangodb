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
#include "Aql/ExecutionEngine.h"
#include "Aql/Functions.h"
#include "Aql/Query.h"
#include "Basics/Exceptions.h"
#include "Basics/ScopeGuard.h"
#include "Basics/VelocyPackHelper.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Methods.h"
#include "V8/v8-globals.h"
#include "VocBase/vocbase.h"

using namespace arangodb::aql;

CalculationBlock::CalculationBlock(ExecutionEngine* engine,
                                   CalculationNode const* en)
    : ExecutionBlock(engine, en),
      _expression(en->expression()),
      _inVars(),
      _inRegs(),
      _outReg(ExecutionNode::MaxRegisterId),
      _conditionReg(ExecutionNode::MaxRegisterId) {
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
  DEBUG_BEGIN_BLOCK();
  bool const hasCondition = (static_cast<CalculationNode const*>(_exeNode)
                                 ->_conditionVariable != nullptr);
  TRI_ASSERT(!hasCondition); // currently not implemented

  size_t const n = result->size();

  for (size_t i = 0; i < n; i++) {
    // check the condition variable (if any)
    if (hasCondition) {
      AqlValue const& conditionResult = result->getValueReference(i, _conditionReg);

      if (!conditionResult.toBoolean()) {
        TRI_IF_FAILURE("CalculationBlock::executeExpressionWithCondition") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }
        result->setValue(i, _outReg, AqlValue(arangodb::basics::VelocyPackHelper::NullValue()));
        continue;
      }
    }

    // execute the expression
    bool mustDestroy;
    AqlValue a = _expression->execute(_trx, result, i, _inVars, _inRegs, mustDestroy);
    AqlValueGuard guard(a, mustDestroy);

    TRI_IF_FAILURE("CalculationBlock::executeExpression") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
    result->setValue(i, _outReg, a);
    guard.steal(); // itemblock has taken over now
    throwIfKilled();  // check if we were aborted
  }
  DEBUG_END_BLOCK();
}

/// @brief doEvaluation, private helper to do the work
void CalculationBlock::doEvaluation(AqlItemBlock* result) {
  DEBUG_BEGIN_BLOCK();
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

  if (!_expression->isV8()) {
    // an expression that does not require V8
    executeExpression(result);
  } else {
    bool const isRunningInCluster = transaction()->state()->isRunningInCluster();

    // must have a V8 context here to protect Expression::execute()
    arangodb::basics::ScopeGuard guard{
        [&]() -> void { _engine->getQuery()->enterContext(); },
        [&]() -> void {
          if (isRunningInCluster) {
            // must invalidate the expression now as we might be called from
            // different threads
            _expression->invalidate();

            _engine->getQuery()->exitContext();
          }
        }};

    ISOLATE;
    v8::HandleScope scope(isolate);  // do not delete this!

    // do not merge the following function call with the same function call
    // above!
    // the V8 expression execution must happen in the scope that contains
    // the V8 handle scope and the scope guard
    executeExpression(result);
  }
  DEBUG_END_BLOCK();
}

AqlItemBlock* CalculationBlock::getSome(size_t atLeast, size_t atMost) {
  DEBUG_BEGIN_BLOCK();
  traceGetSomeBegin();
  std::unique_ptr<AqlItemBlock> res(
      ExecutionBlock::getSomeWithoutRegisterClearout(atLeast, atMost));

  if (res.get() == nullptr) {
    traceGetSomeEnd(nullptr);
    return nullptr;
  }

  doEvaluation(res.get());
  // Clear out registers no longer needed later:
  clearRegisters(res.get());
  traceGetSomeEnd(res.get());
  return res.release();

  // cppcheck-suppress *
  DEBUG_END_BLOCK();
}
