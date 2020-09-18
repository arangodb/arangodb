////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_AQL_IN_AND_OUT_ROW_EXPRESSION_CONTEXT_H
#define ARANGOD_AQL_IN_AND_OUT_ROW_EXPRESSION_CONTEXT_H 1

#include "QueryExpressionContext.h"

#include "Aql/AqlValue.h"
#include "Aql/InputAqlItemRow.h"

namespace arangodb {
namespace aql {
class Query;

/**
 * @brief Context for expression evaluation that allows
 * to read registers from Input and Output.
 * This is useful for expressions that need
 * to evaluate variables created by the active block.
 * User needs to make sure that outputblock is written to!
 */
class InAndOutRowExpressionContext final : public QueryExpressionContext {
 public:
  InAndOutRowExpressionContext(transaction::Methods& trx,
                               QueryContext& query,
                               AqlFunctionsInternalCache& cache,
                               std::vector<Variable const*> vars,
                               std::vector<RegisterId> regs, size_t vertexVarIdx,
                               size_t edgeVarIdx, size_t pathVarIdx);

  ~InAndOutRowExpressionContext() override = default;

  void setInputRow(InputAqlItemRow input);

  void invalidateInputRow();

  bool isDataFromCollection(Variable const* variable) const override;

  AqlValue getVariableValue(Variable const* variable, bool doCopy,
                            bool& mustDestroy) const override;

  bool needsVertexValue() const;
  bool needsEdgeValue() const;
  bool needsPathValue() const;

  /// @brief inject the result value when asked for the Vertex data
  /// This will not copy ownership of slice content. caller needs to make sure
  /// that the buffer stays valid until evaluate is called
  void setVertexValue(velocypack::Slice v);

  /// @brief inject the result value when asked for the Edge data
  /// This will not copy ownership of slice content. caller needs to make sure
  /// that the buffer stays valid until evaluate is called
  void setEdgeValue(velocypack::Slice e);

  /// @brief inject the result value when asked for the Path data
  /// This will not copy ownership of slice content. caller needs to make sure
  /// that the buffer stays valid until evaluate is called
  void setPathValue(velocypack::Slice p);

 private:
  InputAqlItemRow _input;
  std::vector<Variable const*> const _vars;
  std::vector<RegisterId> const _regs;
  size_t const _vertexVarIdx;
  size_t const _edgeVarIdx;
  size_t const _pathVarIdx;
  AqlValue _vertexValue;
  AqlValue _edgeValue;
  AqlValue _pathValue;
};
}  // namespace aql
}  // namespace arangodb
#endif
