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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "ExecutorExpressionContext.h"
#include "Aql/AqlItemBlock.h"
#include "Aql/AqlValue.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/Variable.h"
#include "Basics/Exceptions.h"

#include <absl/strings/str_cat.h>

using namespace arangodb::aql;

ExecutorExpressionContext::ExecutorExpressionContext(
    arangodb::transaction::Methods& trx, QueryContext& context,
    AqlFunctionsInternalCache& cache, InputAqlItemRow const& inputRow,
    std::vector<std::pair<VariableId, RegisterId>> const& varsToRegister)
    : QueryExpressionContext(trx, context, cache),
      _inputRow(inputRow),
      _varsToRegister(varsToRegister) {}

void ExecutorExpressionContext::adjustInputRow(
    InputAqlItemRow const& inputRow) noexcept {
  _inputRow = inputRow;
}

AqlValue ExecutorExpressionContext::getVariableValue(Variable const* variable,
                                                     bool doCopy,
                                                     bool& mustDestroy) const {
  return QueryExpressionContext::getVariableValue(
      variable, doCopy, mustDestroy,
      [this](Variable const* variable, bool doCopy, bool& mustDestroy) {
        mustDestroy = doCopy;
        auto const searchId = variable->id;
        for (auto const& [varId, regId] : _varsToRegister) {
          if (varId == searchId) {
            if (doCopy) {
              return _inputRow.get().getValue(regId).clone();
            }
            return _inputRow.get().getValue(regId);
          }
        }
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_INTERNAL,
            absl::StrCat("variable not found '", variable->name,
                         "' in ExecutorExpressionContext"));
      });
}
