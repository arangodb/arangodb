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

#include "Aql/Match/Builder.h"

#include "Aql/Match/PatternBuildState.h"
#include "Aql/Match/PatternNormalizer.h"

namespace arangodb::aql::match {

Builder::Builder(ExecutionPlan& plan, Ast* ast)
    : _ast(ast),
      _filters(plan, ast),
      _collections(plan, ast, _filters),
      _projections(plan, ast),
      _paths(plan, ast),
      _vertices(plan, ast, _collections, _projections, _paths),
      _relationships(plan, ast, _filters, _collections, _projections, _paths,
                     _vertices) {}

ExecutionNode* Builder::build(ExecutionNode* previous,
                              ast::MatchNode matchNode) {
  PatternNormalizer normalizer(*_ast);
  NormalizedStatement const statement = normalizer.normalize(matchNode);

  auto* en = previous;

  for (auto const& pattern : statement.patterns) {
    PatternBuildState state;
    state.previous = previous;
    state.en = previous;

    _vertices.emitStart(pattern.start, state);

    for (auto const& segment : pattern.segments) {
      _relationships.emitSegment(segment, state);
    }

    en = _paths.finalizePattern(state.previous, state.projections,
                                pattern.pathVariable, state.pathVertices,
                                state.pathEdges);
    previous = en;
  }

  return en;
}

}  // namespace arangodb::aql::match
