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

#include "QueryExpressionContext.h"

namespace arangodb {
namespace aql {
class AqlItemBlock;
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
  InAndOutRowExpressionContext(Query* query, size_t inputRowId, AqlItemBlock const* inputBlock,
                               size_t outputRowId, AqlItemBlock const* outputBlock,
                               std::vector<Variable const*> const& vars,
                               std::vector<RegisterId> const& regs);

  ~InAndOutRowExpressionContext() {}

  size_t numRegisters() const override { return _regs.size(); }

  AqlValue const& getRegisterValue(size_t i) const override;

  Variable const* getVariable(size_t i) const override;

  AqlValue getVariableValue(Variable const* variable, bool doCopy,
                            bool& mustDestroy) const override;

 private:
  size_t _inputRowId;
  AqlItemBlock const* _inputBlock;
  size_t _outputRowId;
  AqlItemBlock const* _outputBlock;
  RegisterId const _endOfInput;
  std::vector<Variable const*> const& _vars;
  std::vector<RegisterId> const& _regs;
};
}  // namespace aql
}  // namespace arangodb
#endif
