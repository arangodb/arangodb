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

#ifndef ARANGOD_AQL_IN_AND_OUT_ROW_EXPRESSION_CONTEXT_H
#define ARANGOD_AQL_IN_AND_OUT_ROW_EXPRESSION_CONTEXT_H 1

#include "ExpressionContext.h"

#include "Aql/AqlValue.h"
#include "Aql/AqlItemBlock.h"

namespace arangodb {
namespace aql {

/**
 * @brief Context for expression evaluation that allows
 * to read registers from Input and Output.
 * This is useful for expressions that need
 * to evaluate variables created by the active block.
 * User needs to make sure that outputblock is written to!
 */
class InAndOutRowExpressionContext final : public ExpressionContext {
 public:
  InAndOutRowExpressionContext(std::vector<Variable const*> const&& vars,
                               std::vector<RegisterId> const&& regs, size_t vertexVarIdx,
                               size_t edgeVarIdx, size_t pathVarIdx);

  ~InAndOutRowExpressionContext() {}

  // NOTE: This block will not take responsibilty for the input block
  void setInputRow(AqlItemBlock const* inputBlock, size_t inputRow);

  size_t numRegisters() const override { return _regs.size(); }

  AqlValue const& getRegisterValue(size_t i) const override;

  Variable const* getVariable(size_t i) const override { return _vars[i]; }

  AqlValue getVariableValue(Variable const* variable, bool doCopy,
                            bool& mustDestroy) const override;

  bool needsVertexValue() const { return _vertexVarIdx < _regs.size(); }
  bool needsEdgeValue() const { return _edgeVarIdx < _regs.size(); }
  bool needsPathValue() const { return _pathVarIdx < _regs.size(); }

  /// @brief inject the result value when asked for the Vertex data
  /// This will not copy ownership of slice content. caller needs to make sure
  /// that the buffer stays valid until evaluate is called
  void setVertexValue(velocypack::Slice v) {
    _vertexValue = AqlValue(AqlValueHintDocumentNoCopy(v.begin()));
  }

  /// @brief inject the result value when asked for the Edge data
  /// This will not copy ownership of slice content. caller needs to make sure
  /// that the buffer stays valid until evaluate is called
  void setEdgeValue(velocypack::Slice e) {
    _edgeValue = AqlValue(AqlValueHintDocumentNoCopy(e.begin()));
  }

  /// @brief inject the result value when asked for the Path data
  /// This will not copy ownership of slice content. caller needs to make sure
  /// that the buffer stays valid until evaluate is called
  void setPathValue(velocypack::Slice p) {
    _pathValue = AqlValue(AqlValueHintDocumentNoCopy(p.begin()));
  }

 private:
  AqlItemBlock const* _inputBlock;
  size_t _inputRow;
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
