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

namespace arangodb {
namespace velocypack {
class Slice;
}
namespace transaction {
class Methods;
}

namespace aql {
class Expression;
class AqlItemBlock;

class PruneExpressionEvaluator {
 public:
  PruneExpressionEvaluator(transaction::Methods* trx,
                           std::vector<Variable const*> const&& vars,
                           std::vector<RegisterId> const&& regs, size_t vertexVarIdx,
                           size_t edgeVarIdx, size_t pathVarIdx, Expression* expr);

  ~PruneExpressionEvaluator();

  bool evaluate();
  void prepareContext(AqlItemBlock const* inputBlock, size_t inputRow) {
    _ctx.setInputRow(inputBlock, inputRow);
  }

  bool needsVertex() const { return _ctx.needsVertexValue(); }
  void injectVertex(velocypack::Slice v) { _ctx.setVertexValue(v); }
  bool needsEdge() const { return _ctx.needsEdgeValue(); }
  void injectEdge(velocypack::Slice e) { _ctx.setEdgeValue(e); }
  bool needsPath() const { return _ctx.needsPathValue(); }
  void injectPath(velocypack::Slice p) { _ctx.setPathValue(p); }

 private:
  transaction::Methods* _trx;

  /// @brief The condition given in PRUNE (might be empty)
  ///        The Node keeps responsibility
  aql::Expression* _pruneExpression;

  /// @brief The context used to inject variables
  InAndOutRowExpressionContext _ctx;
};

}  // namespace aql
}  // namespace arangodb

#endif
