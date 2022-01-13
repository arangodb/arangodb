////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <memory>
#include <numeric>

#include "Aql/AqlFunctionsInternalCache.h"
#include "Aql/FixedVarExpressionContext.h"
#include "Containers/FlatHashMap.h"
#include "Aql/PruneExpressionEvaluator.h"
#include "Transaction/Methods.h"

namespace arangodb {
namespace aql {
class Expression;
class ExpressionContext;
class QueryContext;
struct Variable;
class InputAqlItemRow;
}  // namespace aql

namespace graph {
class PathValidatorOptions {
 public:
  PathValidatorOptions(
      aql::Variable const* tmpVar,
      arangodb::aql::FixedVarExpressionContext& expressionContext);
  ~PathValidatorOptions() = default;
  PathValidatorOptions(PathValidatorOptions&&) = default;
  PathValidatorOptions(PathValidatorOptions const&) = default;

  // Vertex expression section
  /**
   * @brief Set the expression that needs to hold true for ALL vertices on the
   * path.
   */
  void setAllVerticesExpression(std::unique_ptr<aql::Expression> expression);

  /**
   * @brief Set the expression that needs to hold true for the vertex on the
   * given depth. NOTE: This will overrule the ALL vertex expression, so make
   * sure this expression contains everything the ALL expression covers.
   */
  void setVertexExpression(uint64_t depth,
                           std::unique_ptr<aql::Expression> expression);

  // Prune section
  /**
   * @brief Sets a prune evaluator. This needs to be called from within an Aql
   * Node, as the node itself holds all the expressions.
   */
  void setPruneEvaluator(
      std::shared_ptr<aql::PruneExpressionEvaluator>&& expression);

  void setPostFilterEvaluator(
      std::shared_ptr<aql::PruneExpressionEvaluator>&& expression);

  /**
   * @brief Returns the current prune evaluator. It is possible that no prune
   * validator has been set.
   */
  std::shared_ptr<aql::PruneExpressionEvaluator>& getPruneEvaluator();
  std::shared_ptr<aql::PruneExpressionEvaluator>& getPostFilterEvaluator();

  /**
   * @brief Check whether Prune or PostFilter is enabled or disabled.
   */
  bool usesPrune() const;
  bool usesPostFilter() const;

  /**
   * @brief In case prune or postFilter has been enabled, we need to unprepare
   * the context of the inputRow. See explanation in TraversalExecutor.cpp L200
   */
  void unpreparePruneContext();
  void unpreparePostFilterContext();

  /**
   * @brief In case prune or postFilter is enabled, the prune context must be
   * set during the processing of all DataRows (InputAqlItemRow) from within the
   * specific Executor.
   */
  void setPruneContext(arangodb::aql::InputAqlItemRow& inputRow);
  void setPostFilterContext(arangodb::aql::InputAqlItemRow& inputRow);

  /**
   * @brief Get the Expression a vertex needs to hold if defined on the given
   * depth. It may return a nullptr if all vertices are valid.
   * Caller does NOT take responsibilty. Do not delete this pointer.
   */
  aql::Expression* getVertexExpression(uint64_t depth) const;

  void addAllowedVertexCollection(std::string const& collectionName);

  void addAllowedVertexCollections(
      std::vector<std::string> const& collectionNames);

  std::vector<std::string> const& getAllowedVertexCollections() const;

  aql::Variable const* getTempVar() const;

  aql::FixedVarExpressionContext& getExpressionContext();

  // @brief If a graph is asked for the first vertex and that is filtered
  // it can be removed for 3.9 => nextVersion.
  void setBfsResultHasToIncludeFirstVertex() {
    _bfsResultHasToIncludeFirstVertex = true;
  }

  bool bfsResultHasToIncludeFirstVertex() const {
    return _bfsResultHasToIncludeFirstVertex;
  }

 private:
  // Vertex expression section
  std::shared_ptr<aql::Expression> _allVerticesExpression;
  containers::FlatHashMap<uint64_t, std::shared_ptr<aql::Expression>>
      _vertexExpressionOnDepth;
  std::vector<std::string> _allowedVertexCollections;
  bool _bfsResultHasToIncludeFirstVertex = false;

  // Prune section
  std::shared_ptr<aql::PruneExpressionEvaluator> _pruneEvaluator;
  std::shared_ptr<aql::PruneExpressionEvaluator> _postFilterEvaluator;

  // TODO [GraphRefactor]: Post filter section - to be implemented

  aql::Variable const* _tmpVar;
  arangodb::aql::FixedVarExpressionContext& _expressionCtx;
};
}  // namespace graph
}  // namespace arangodb
