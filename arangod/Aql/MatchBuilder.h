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

#pragma once

#include "Aql/types.h"

#include <cstddef>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace arangodb::aql {

class Ast;
struct AstNode;
class CalculationNode;
class ExecutionNode;
class ExecutionPlan;
class FilterNode;
struct Variable;

/// @brief Lowers AQL MATCH pattern AST nodes into ExecutionPlan fragments.
/// Extracted from ExecutionPlan::fromNodeMatch so capturing lambdas become
/// named methods without changing lowering semantics or call order.
class MatchBuilder {
 public:
  MatchBuilder(ExecutionPlan& plan, Ast* ast);

  /// @brief Lower a NODE_TYPE_MATCH AST node, chaining onto @p previous.
  ExecutionNode* build(ExecutionNode* previous, AstNode const* matchNode);

 private:
  AstNode* createPropertyAccess(Variable const* variable,
                                std::string_view property);

  static size_t patternEdgeCollectionCount(AstNode const* edgeLabelMember);
  static AstNode const* getPatternEdgeCollection(AstNode const* edgeLabelMember,
                                                 size_t index);
  AstNode* buildPatternEdgeCollectionList(AstNode const* edgeLabelMember);

  std::tuple<CalculationNode*, FilterNode*> createPropertiesFilter(
      Variable const* variable, AstNode* properties,
      AstNode* additionalExpression);

  std::tuple<ExecutionNode*, ExecutionNode*, Variable const*>
  createCollectionAccess(
      AstNode const* member, Variable const* fullDocumentVariable,
      std::unordered_map<VariableId, Variable const*> const& subst);

  ExecutionNode* createPatternProjection(
      AstNode const* member, Variable const* fullDocumentVar,
      std::unordered_map<VariableId, Variable const*> const& subst);

  std::tuple<ExecutionNode*, ExecutionNode*, Variable const*>
  createPatternEdgeEnumerateAccess(
      AstNode const* edge, Variable const* outputVariable,
      std::unordered_map<VariableId, Variable const*> const& subst);

  std::tuple<CalculationNode*, FilterNode*> createVertexEdgeFilter(
      Variable const* leftVertex, Variable const* edge,
      Variable const* rightVertex, int direction);

  std::tuple<ExecutionNode*, ExecutionNode*, Variable const*>
  createTraversalForPattern(Variable const* startNodeVar, AstNode const* edge,
                            AstNode const* node);

  AstNode* constructArray(std::vector<AstNode const*> const& vars);

  CalculationNode* constructPathObject(
      Variable const* outVariable, std::vector<AstNode const*> const& vertices,
      std::vector<AstNode const*> const& edges);

  ExecutionPlan& _plan;
  Ast* _ast;
};

}  // namespace arangodb::aql
