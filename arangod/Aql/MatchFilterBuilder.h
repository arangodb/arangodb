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

#include "Aql/MatchPatternTypes.h"
#include "Aql/types.h"

#include <optional>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace arangodb::aql {

class Ast;
struct AstNode;
class CalculationNode;
class ExecutionPlan;
class FilterNode;
struct Variable;

/// @brief Builds MATCH property / edge-direction filter fragments.
class MatchFilterBuilder {
 public:
  MatchFilterBuilder(ExecutionPlan& plan, Ast* ast);

  AstNode* createPropertyAccess(Variable const* variable,
                                std::string_view property);

  std::tuple<CalculationNode*, FilterNode*> createPropertiesFilter(
      Variable const* variable,
      std::vector<MatchPropertyConstraint> const& properties,
      std::optional<MatchExpressionRef> const& additionalFilter,
      std::unordered_map<VariableId, Variable const*> const& subst);

  std::tuple<CalculationNode*, FilterNode*> createVertexEdgeFilter(
      Variable const* leftVertex, Variable const* edge,
      Variable const* rightVertex, MatchEdgeDirection direction);

 private:
  ExecutionPlan& _plan;
  Ast* _ast;
};

}  // namespace arangodb::aql
