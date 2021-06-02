////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "PathValidatorOptions.h"

#include "Aql/Expression.h"
#include "Aql/QueryContext.h"

using namespace arangodb;
using namespace arangodb::graph;

PathValidatorOptions::PathValidatorOptions()
    : _trx{std::nullopt}, _expressionCtx{std::nullopt} {}

PathValidatorOptions::PathValidatorOptions(arangodb::aql::QueryContext& query)
    : _trx{query.newTrxContext()},
      _aqlFunctionsInternalCache({}),
      _expressionCtx(aql::FixedVarExpressionContext{_trx.value(), query,
                                                    _aqlFunctionsInternalCache.value()}) {}

PathValidatorOptions::~PathValidatorOptions() = default;

PathValidatorOptions::PathValidatorOptions(PathValidatorOptions&&) = default;

void PathValidatorOptions::setAllEdgesExpression(std::unique_ptr<aql::Expression> expression) {
  // All edge expression should not be set before
  TRI_ASSERT(_allEdgesExpression == nullptr);
  _allEdgesExpression = std::move(expression);
}

void PathValidatorOptions::setEdgeExpression(uint64_t depth,
                                             std::unique_ptr<aql::Expression> expression) {
  // Should not respecifiy the condition on a certain depth
  TRI_ASSERT(_edgeExpressionOnDepth.find(depth) == _edgeExpressionOnDepth.end());
  _edgeExpressionOnDepth.emplace(depth, std::move(expression));
}

aql::Expression* PathValidatorOptions::getEdgeExpression(uint64_t depth) const {
  auto const& it = _edgeExpressionOnDepth.find(depth);
  if (it != _edgeExpressionOnDepth.end()) {
    return it->second.get();
  }
  return _allEdgesExpression.get();
}

aql::Variable const* PathValidatorOptions::getTempVar() const {
  return _tmpVar;
}

aql::ExpressionContext* PathValidatorOptions::getExpressionContext() {
  // We can only call this if we have been called with QueryContext contructor.
  // Otherwise this object could not be constructed.
  // However it is also only necessary if we actually have expressions to check.
  TRI_ASSERT(_expressionCtx.has_value());
  return &_expressionCtx.value();
}
