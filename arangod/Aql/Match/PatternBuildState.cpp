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

#include "Aql/Match/PatternBuildState.h"

#include "Aql/Ast.h"
#include "Aql/Match/ProjectionBuilder.h"
#include "Aql/Variable.h"

namespace arangodb::aql::match {

ProjectionBinding bindProjectedVariable(
    Ast* ast, Variable const* destination,
    std::optional<Projection> const& projection,
    std::unordered_map<VariableId, Variable const*>& subst) {
  ProjectionBinding binding;
  binding.destination = destination;
  binding.projection = projection ? &*projection : nullptr;
  if (binding.hasProjection()) {
    binding.fullDocument = ast->variables()->createTemporaryVariable();
    subst.emplace(destination->id, binding.fullDocument);
  } else {
    binding.fullDocument = destination;
  }
  return binding;
}

void maybeQueueDocumentProjection(
    ProjectionBuilder& projections, std::vector<ExecutionNode*>& queued,
    ProjectionBinding const& binding,
    std::unordered_map<VariableId, Variable const*> const& subst) {
  if (!binding.hasProjection()) {
    return;
  }
  queued.push_back(projections.createDocumentPatternProjection(
      binding.destination, binding.fullDocument, *binding.projection, subst));
}

void maybeQueueEdgeProjection(
    ProjectionBuilder& projections, std::vector<ExecutionNode*>& queued,
    ProjectionBinding const& binding,
    std::unordered_map<VariableId, Variable const*> const& subst) {
  if (!binding.hasProjection()) {
    return;
  }
  queued.push_back(projections.createEdgeDocumentPatternProjection(
      binding.destination, binding.fullDocument, *binding.projection, subst));
}

}  // namespace arangodb::aql::match
