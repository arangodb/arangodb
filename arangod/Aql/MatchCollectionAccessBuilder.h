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

#include <string>
#include <tuple>
#include <unordered_map>

namespace arangodb::aql {

class Ast;
struct AstNode;
class ExecutionNode;
class ExecutionPlan;
class MatchFilterBuilder;
struct Variable;

/// @brief Resolves MATCH collections and builds EnumerateCollection access.
class MatchCollectionAccessBuilder {
 public:
  MatchCollectionAccessBuilder(ExecutionPlan& plan, Ast* ast,
                               MatchFilterBuilder& filters);

  [[nodiscard]] static std::string requireCollectionName(
      MatchDataSource const& ds);

  AstNode* buildEdgeCollectionList(NormalizedEdge const& edge);

  std::tuple<ExecutionNode*, ExecutionNode*, Variable const*>
  createCollectionAccess(
      NormalizedVertex const& vertex, Variable const* fullDocumentVariable,
      std::unordered_map<VariableId, Variable const*> const& subst);

  std::tuple<ExecutionNode*, ExecutionNode*, Variable const*>
  createPatternEdgeEnumerateAccess(
      NormalizedEdge const& edge, Variable const* outputVariable,
      std::unordered_map<VariableId, Variable const*> const& subst);

 private:
  ExecutionPlan& _plan;
  Ast* _ast;
  MatchFilterBuilder& _filters;
};

}  // namespace arangodb::aql
