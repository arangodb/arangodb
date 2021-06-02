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

#pragma once

#include <memory>
#include <numeric>
#include <unordered_map>

#include "Aql/AqlFunctionsInternalCache.h"
#include "Aql/FixedVarExpressionContext.h"
#include "Transaction/Methods.h"

namespace arangodb {
namespace aql {
class Expression;
class ExpressionContext;
class QueryContext;
struct Variable;
}  // namespace aql

// TODO the context and transaction is right now optional and potentially dangerous.
// However it is actually not needed often so we may want to split this into two classes
// or simplify the expression context to make it creatable with a query context.
namespace graph {
class PathValidatorOptions {
 public:
  PathValidatorOptions();
  explicit PathValidatorOptions(aql::QueryContext& query);
  ~PathValidatorOptions();
  PathValidatorOptions(PathValidatorOptions&&);

  /**
   * @brief Set the expression that needs to hold true for ALL edges on the path.
   */
  void setAllEdgesExpression(std::unique_ptr<aql::Expression> expression);

  /**
   * @brief Set the expression that needs to hold true for the edge on the given
   * depth. NOTE: This will overrule the ALL edge expression, so make sure this
   * expression contains everything the ALL expression covers.
   */
  void setEdgeExpression(uint64_t depth, std::unique_ptr<aql::Expression> expression);

  /**
   * @brief Get the Expression an edge needs to hold if defined on the given
   * depth. It may return a nullptr if all edges are valid.
   * Caller does NOT take responsibilty. Do not delete this pointer.
   */
  aql::Expression* getEdgeExpression(uint64_t depth) const;

  aql::Variable const* getTempVar() const;

  aql::ExpressionContext* getExpressionContext();

 private:
  std::unique_ptr<aql::Expression> _allEdgesExpression;
  std::unordered_map<uint64_t, std::unique_ptr<aql::Expression>> _edgeExpressionOnDepth;

  aql::Variable const* _tmpVar;

  // Only needed for the fixed var expression context below. No real usage here.
  std::optional<transaction::Methods> _trx;
  std::optional<arangodb::aql::AqlFunctionsInternalCache> _aqlFunctionsInternalCache;
  // TODO: FixedVarExpressionContext is a bit overkill here, a lot of methods are implemented but not used.
  // We may be able to simplify this and reduce the InputParameters for ValidatorOptions;
  std::optional<arangodb::aql::FixedVarExpressionContext> _expressionCtx;
};
}  // namespace graph
}  // namespace arangodb
