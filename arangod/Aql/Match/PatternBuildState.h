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

#include "Aql/Match/PatternTypes.h"
#include "Aql/Match/VariableScope.h"
#include "Aql/types.h"

#include <optional>
#include <unordered_map>
#include <vector>

namespace arangodb::aql {
class Ast;
struct AstNode;
class ExecutionNode;
struct Variable;
}  // namespace arangodb::aql

namespace arangodb::aql::match {

class ProjectionBuilder;

/// @brief User-facing pattern variable plus the variable that holds the full
/// document during enumeration/traversal (a temporary when projecting).
/// @p projection points into the NormalizedStatement owned for the duration of
/// build(); null when not projecting.
struct ProjectionBinding {
  Variable const* destination{nullptr};
  Variable const* fullDocument{nullptr};
  Projection const* projection{nullptr};

  [[nodiscard]] bool hasProjection() const noexcept {
    return projection != nullptr;
  }
};

/// @brief When @p projection is set, create a temporary full-document variable
/// and register destination→temp in @p subst; otherwise enumerate directly into
/// @p destination (COR-888).
ProjectionBinding bindProjectedVariable(
    Ast* ast, Variable const* destination,
    std::optional<Projection> const& projection,
    std::unordered_map<VariableId, Variable const*>& subst);

void maybeQueueDocumentProjection(
    ProjectionBuilder& projections, std::vector<ExecutionNode*>& queued,
    ProjectionBinding const& binding,
    std::unordered_map<VariableId, Variable const*> const& subst);

void maybeQueueEdgeProjection(
    ProjectionBuilder& projections, std::vector<ExecutionNode*>& queued,
    ProjectionBinding const& binding,
    std::unordered_map<VariableId, Variable const*> const& subst);

/// @brief Mutable state while lowering one NormalizedPattern.
struct PatternBuildState {
  ExecutionNode* previous{nullptr};
  ExecutionNode* en{nullptr};
  Variable const* prevVar{nullptr};
  std::vector<AstNode const*> pathVertices;
  std::vector<AstNode const*> pathEdges;
  std::vector<ExecutionNode*> projections;
  VariableScope variableScope;
};

}  // namespace arangodb::aql::match
