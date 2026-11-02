////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////

#include "Aql/Ast.h"
#include "Aql/AqlTransaction.h"
#include "CalculationQueryContext.h"
#include "Aql/Timing.h"

namespace arangodb::aql {

CalculationQueryContext::CalculationQueryContext(
    TRI_vocbase_t& vocbase, transaction::OperationOrigin operationOrigin)
    : QueryContext(vocbase, operationOrigin),
      _resolver(vocbase),
      _transactionContext(vocbase, operationOrigin) {
  _ast = std::make_unique<Ast>(*this, NON_CONST_PARAMETERS);
  _trx = AqlTransaction::create(newTrxContext(), _collections,
                                _queryOptions.transactionOptions,
                                std::unordered_set<std::string>{});
  _trx->addHint(arangodb::transaction::Hints::Hint::FROM_TOPLEVEL_AQL);
  _trx->addHint(
      arangodb::transaction::Hints::Hint::SINGLE_OPERATION);  // to avoid taking
                                                              // db snapshot
  auto res = _trx->begin();
  if (res.fail()) {
    throw basics::Exception(std::move(res));
  }
}

bool CalculationQueryContext::killed() const {
  return (std::numeric_limits<double>::epsilon() < _queryOptions.maxRuntime &&
          _queryOptions.maxRuntime < aql::elapsedSince(_startTime));
}

void CalculationQueryContext::resetRuntimeClock() {
  _startTime = aql::currentSteadyClockValue();
}

void CalculationQueryContext::enterV8Executor() {
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_NOT_IMPLEMENTED,
      "CalculationQueryContext: entering V8 executor is not implemented");
}

}  // namespace arangodb::aql