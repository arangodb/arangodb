////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "CalculationExecutor.h"

#include "Aql/AqlValue.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/Query.h"
#include "Basics/Common.h"
#include "Basics/Exceptions.h"
#include "ExecutorExpressionContext.h"
#include "V8/v8-globals.h"
#include "Cluster/ServerState.h"

#include <lib/Logger/LogMacros.h>

using namespace arangodb;
using namespace arangodb::aql;

namespace _detail {
  void doEvaluation(CalculationExecutorInfos& info, InputAqlItemRow& input, OutputAqlItemRow& output);
  void executeExpression(CalculationExecutorInfos& info, InputAqlItemRow& input, OutputAqlItemRow& output);
}


CalculationExecutorInfos::CalculationExecutorInfos( RegisterId outputRegister_
                                                  , RegisterId nrInputRegisters
                                                  , RegisterId nrOutputRegisters
                                                  , std::unordered_set<RegisterId> registersToClear

                                                  , Query* query
                                                  , Expression* expression
                                                  , std::vector<Variable const*>&& expInVars
                                                  , std::vector<RegisterId>&& expInRegs
                                                  , Variable const* conditionVar
                                                  )
    : ExecutorInfos({expInRegs.begin(), expInRegs.end()}, {outputRegister_}, nrInputRegisters, nrOutputRegisters, std::move(registersToClear))
    ,  _outputRegister(outputRegister_)
    ,  _query(query)
    ,  _expression(expression)
    ,  _expInVars(std::move(expInVars))
    ,  _expInRegs(std::move(expInRegs))
    ,  _conditionVariable(conditionVar)
{


  TRI_ASSERT(_query->trx() != nullptr);

  _isReference = (_expression->node()->type == NODE_TYPE_REFERENCE);
  if (_isReference) {
    TRI_ASSERT(_inRegs.size() == 1);
  }

}


CalculationExecutor::CalculationExecutor(Fetcher& fetcher ,CalculationExecutorInfos& infos)
    : _infos(infos)
    , _fetcher(fetcher)
    , _rowState(ExecutionState::HASMORE) {};

std::pair<ExecutionState, NoStats> CalculationExecutor::produceRow(OutputAqlItemRow& output) {

  ExecutionState state;
  InputAqlItemRow row = InputAqlItemRow{CreateInvalidInputRowHint{}};
  std::tie(state, row) = _fetcher.fetchRow();

  if (state == ExecutionState::WAITING) {
    TRI_ASSERT(!row);
    return {state, NoStats{}};
  }

  if (!row) {
    TRI_ASSERT(state == ExecutionState::DONE);
    return {state, NoStats{}};
  }

  _detail::doEvaluation(_infos, row, output);

  return  {state, NoStats{}};
}


namespace _detail {

void doEvaluation(CalculationExecutorInfos& info, InputAqlItemRow& input, OutputAqlItemRow& output) {
  static const bool isRunningInCluster = ServerState::instance()->isRunningInCluster();
  // non-reference expression
  TRI_ASSERT(info._expression != nullptr);

  if (info._isReference) {
    // the calculation is a reference to a variable only.
    // no need to execute the expression at all
    TRI_ASSERT(false); //not implemented
    //fillBlockWithReference(result);
    if (info._query->killed()){
      THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_KILLED);
    }
    return;
  }

  if (!info._expression->willUseV8()) {
    // an expression that does not require V8
    executeExpression(info, input, output);
  } else {
    auto cleanup = [&]() {
      if (isRunningInCluster) {
        // must invalidate the expression now as we might be called from
        // different threads
        info._expression->invalidate();
        info._query->exitContext();
      }
    };

    // must have a V8 context here to protect Expression::execute()
    info._query->enterContext();
    TRI_DEFER(cleanup());

    ISOLATE;
    v8::HandleScope scope(isolate);  // do not delete this!

    // do not merge the following function call with the same function call
    // above!
    // the V8 expression execution must happen in the scope that contains
    // the V8 handle scope and the scope guard
    executeExpression(info, input, output);
  }
}

void executeExpression(CalculationExecutorInfos& info, InputAqlItemRow& input, OutputAqlItemRow& output) {

  bool const hasCondition = info._conditionVariable != nullptr;
  TRI_ASSERT(!hasCondition);
  if (hasCondition) {
    //TODO not implemented -- see old CalcualtionBlock.cpp for details -- below old impl

    //if (hasCondition) {
    //  AqlValue const& conditionResult = result->getValueReference(i, _conditionReg);

    //  if (!conditionResult.toBoolean()) {
    //    TRI_IF_FAILURE("CalculationBlock::executeExpressionWithCondition") {
    //      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    //    }
    //    result->emplaceValue(i, _outReg, arangodb::velocypack::Slice::nullSlice());
    //    continue;
    //  }
    //}
  }

  // execute the expression
  ExecutorExpressionContext ctx(info._query, input, info._expInVars, info._expInRegs);

  bool mustDestroy; // will get filled by execution
  AqlValue a = info._expression->execute(info._query->trx(), &ctx, mustDestroy);
  AqlValueGuard guard(a, mustDestroy);

  TRI_IF_FAILURE("CalculationBlock::executeExpression") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  output.setValue(info._outputRegister, input ,a);
  guard.steal(); // itemblock has taken over now

  if (info._query->killed()){
    THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_KILLED);
  }
}

}
