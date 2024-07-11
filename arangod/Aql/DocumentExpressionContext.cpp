////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "DocumentExpressionContext.h"

#include "Aql/AqlValue.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/Variable.h"
#include "Basics/system-compiler.h"

using namespace arangodb::aql;

DocumentExpressionContext::DocumentExpressionContext(
    arangodb::transaction::Methods& trx, QueryContext& query,
    aql::AqlFunctionsInternalCache& cache,
    std::vector<std::pair<VariableId, RegisterId>> const& filterVarsToRegister,
    InputAqlItemRow const& inputRow, Variable const* outputVariable) noexcept
    : DocumentProducingExpressionContext(trx, query, cache,
                                         filterVarsToRegister, inputRow),
      _outputVariable(outputVariable) {
  TRI_ASSERT(_outputVariable != nullptr);
}

SimpleDocumentExpressionContext::SimpleDocumentExpressionContext(
    arangodb::transaction::Methods& trx, QueryContext& query,
    aql::AqlFunctionsInternalCache& cache,
    std::vector<std::pair<VariableId, RegisterId>> const& filterVarsToRegister,
    InputAqlItemRow const& inputRow, Variable const* outputVariable) noexcept
    : DocumentExpressionContext(trx, query, cache, filterVarsToRegister,
                                inputRow, outputVariable) {}

AqlValue SimpleDocumentExpressionContext::getVariableValue(
    Variable const* variable, bool doCopy, bool& mustDestroy) const {
  TRI_ASSERT(variable != nullptr);
  return QueryExpressionContext::getVariableValue(
      variable, doCopy, mustDestroy,
      [this](Variable const* /*variable*/, bool doCopy, bool& mustDestroy) {
        mustDestroy = doCopy;
        // return current document
        if (doCopy) {
          return AqlValue(AqlValueHintSliceCopy(_document));
        }
        return AqlValue(AqlValueHintSliceNoCopy(_document));
      });
}

GenericDocumentExpressionContext::GenericDocumentExpressionContext(
    arangodb::transaction::Methods& trx, QueryContext& query,
    aql::AqlFunctionsInternalCache& cache,
    std::vector<std::pair<VariableId, RegisterId>> const& filterVarsToRegister,
    InputAqlItemRow const& inputRow, Variable const* outputVariable) noexcept
    : DocumentExpressionContext(trx, query, cache, filterVarsToRegister,
                                inputRow, outputVariable) {}

AqlValue GenericDocumentExpressionContext::getVariableValue(
    Variable const* variable, bool doCopy, bool& mustDestroy) const {
  TRI_ASSERT(variable != nullptr);
  return QueryExpressionContext::getVariableValue(
      variable, doCopy, mustDestroy,
      [this](Variable const* variable, bool doCopy, bool& mustDestroy) {
        mustDestroy = doCopy;
        if (variable->id == _outputVariable->id) {
          // return current document
          if (doCopy) {
            return AqlValue(AqlValueHintSliceCopy(_document));
          }
          return AqlValue(AqlValueHintSliceNoCopy(_document));
        }
        auto regId = registerForVariable(variable->id);
        TRI_ASSERT(regId != RegisterId::maxRegisterId)
            << "variable " << variable->name << " (" << variable->id
            << ") not found";
        if (ADB_LIKELY(regId != RegisterId::maxRegisterId)) {
          // we can only get here in a post-filter expression
          TRI_ASSERT(regId.isConstRegister() ||
                     regId < _inputRow.getNumRegisters())
              << "variable " << variable->name << " (" << variable->id
              << "), register " << regId.value() << " not found";
          if (doCopy) {
            return _inputRow.getValue(regId).clone();
          }
          return _inputRow.getValue(regId);
        }

        // referred-to variable not found. should never happen
        TRI_ASSERT(false);
        return AqlValue(AqlValueHintNull{});
      });
}
