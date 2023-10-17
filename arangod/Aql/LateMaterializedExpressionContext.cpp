////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Yuriy Popov
////////////////////////////////////////////////////////////////////////////////

#include "LateMaterializedExpressionContext.h"
#include "Aql/AqlValue.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/Variable.h"

using namespace arangodb::aql;

LateMaterializedExpressionContext::LateMaterializedExpressionContext(
    arangodb::transaction::Methods& trx, QueryContext& query,
    aql::AqlFunctionsInternalCache& cache,
    std::vector<std::pair<VariableId, RegisterId>> const& filterVarsToRegister,
    InputAqlItemRow const& inputRow,
    IndexNode::IndexFilterCoveringVars const&
        outNonMaterializedIndVars) noexcept
    : DocumentProducingExpressionContext(trx, query, cache,
                                         filterVarsToRegister, inputRow),
      _outNonMaterializedIndVars(outNonMaterializedIndVars),
      _covering(nullptr) {}

AqlValue LateMaterializedExpressionContext::getVariableValue(
    Variable const* variable, bool doCopy, bool& mustDestroy) const {
  TRI_ASSERT(variable != nullptr);
  TRI_ASSERT(_covering != nullptr);
  return QueryExpressionContext::getVariableValue(
      variable, doCopy, mustDestroy,
      [this](Variable const* variable, bool doCopy, bool& mustDestroy) {
        mustDestroy = doCopy;
        auto const it = _outNonMaterializedIndVars.find(variable);
        if (it != _outNonMaterializedIndVars.end()) {
          // return data from current index entry
          velocypack::Slice s;
          // hash/skiplist/persistent
          if (_covering->isArray()) {
            TRI_ASSERT(it->second < _covering->length());
            if (ADB_UNLIKELY(it->second >= _covering->length())) {
              return AqlValue();
            }
            s = _covering->at(it->second);
          } else {  // primary/edge
            s = _covering->value();
          }
          if (doCopy) {
            return AqlValue(AqlValueHintSliceCopy(s));
          }
          return AqlValue(AqlValueHintSliceNoCopy(s));
        }

        auto regId = registerForVariable(variable->id);
        TRI_ASSERT(regId != RegisterId::maxRegisterId);
        if (regId != RegisterId::maxRegisterId) {
          // we can only get here in a post-filter expression
          TRI_ASSERT(regId < _inputRow.getNumRegisters());
          if (doCopy) {
            return _inputRow.getValue(regId).clone();
          }
          return _inputRow.getValue(regId);
        }

        // should never happen
        TRI_ASSERT(false);
        return AqlValue(AqlValueHintNull());
      });
}
