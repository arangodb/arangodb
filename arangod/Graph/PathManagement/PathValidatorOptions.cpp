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

PathValidatorOptions::PathValidatorOptions() : _tmpVar(nullptr) {}

PathValidatorOptions::PathValidatorOptions(aql::QueryContext& query, aql::Variable const* tmpVar)
    : _tmpVar(tmpVar),
      _trx{std::make_unique<transaction::Methods>(query.newTrxContext())},
      _expressionCtx{std::make_unique<aql::FixedVarExpressionContext>(
          *_trx.get(), query, _aqlFunctionsInternalCache)} {}

PathValidatorOptions::~PathValidatorOptions() = default;

PathValidatorOptions::PathValidatorOptions(PathValidatorOptions&&) = default;

void PathValidatorOptions::setAllVerticesExpression(std::unique_ptr<aql::Expression> expression) {
  // All edge expression should not be set before
  TRI_ASSERT(_allVerticesExpression == nullptr);
  _allVerticesExpression = std::move(expression);
}

void PathValidatorOptions::setVertexExpression(uint64_t depth,
                                               std::unique_ptr<aql::Expression> expression) {
  // Should not respecifiy the condition on a certain depth
  TRI_ASSERT(_vertexExpressionOnDepth.find(depth) == _vertexExpressionOnDepth.end());
  _vertexExpressionOnDepth.emplace(depth, std::move(expression));
}

aql::Expression* PathValidatorOptions::getVertexExpression(uint64_t depth) const {
  auto const& it = _vertexExpressionOnDepth.find(depth);
  if (it != _vertexExpressionOnDepth.end()) {
    return it->second.get();
  }
  return _allVerticesExpression.get();
}

aql::Variable const* PathValidatorOptions::getTempVar() const {
  return _tmpVar;
}

aql::FixedVarExpressionContext* PathValidatorOptions::getExpressionContext() {
  // We can only call this if we have been called with QueryContext contructor.
  // Otherwise this object could not be constructed.
  // However it is also only necessary if we actually have expressions to check.
  return _expressionCtx.get();
}
