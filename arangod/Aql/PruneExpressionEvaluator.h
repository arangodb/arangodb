////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019-2019 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_AQL_PRUNE_EXPRESSION_EVALUATOR_H
#define ARANGOD_AQL_PRUNE_EXPRESSION_EVALUATOR_H 1

#include "Aql/InAndOutRowExpressionContext.h"

#include <velocypack/Slice.h>

#include <utility>

namespace arangodb {
namespace transaction {
class Methods;
}

namespace aql {
class Expression;
class QueryContext;
class InputAqlItemRow;

class PruneExpressionEvaluator {
 public:
  PruneExpressionEvaluator(transaction::Methods& trx,
                           QueryContext& query,
                           AqlFunctionsInternalCache& cache,
                           std::vector<Variable const*> vars,
                           std::vector<RegisterId> regs, size_t vertexVarIdx,
                           size_t edgeVarIdx, size_t pathVarIdx, Expression* expr);

  ~PruneExpressionEvaluator();

  bool evaluate();
  void prepareContext(InputAqlItemRow input) { _ctx.setInputRow(std::move(input)); }
  void unPrepareContext() { _ctx.invalidateInputRow(); }

  bool needsVertex() const { return _ctx.needsVertexValue(); }
  void injectVertex(velocypack::Slice v) { _ctx.setVertexValue(v); }
  bool needsEdge() const { return _ctx.needsEdgeValue(); }
  void injectEdge(velocypack::Slice e) { _ctx.setEdgeValue(e); }
  bool needsPath() const { return _ctx.needsPathValue(); }
  void injectPath(velocypack::Slice p) { _ctx.setPathValue(p); }

 private:
  /// @brief The condition given in PRUNE (might be empty)
  ///        The Node keeps responsibility
  aql::Expression* _pruneExpression;

  /// @brief The context used to inject variables
  InAndOutRowExpressionContext _ctx;
};

}  // namespace aql
}  // namespace arangodb

#endif
