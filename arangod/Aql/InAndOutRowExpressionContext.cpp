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

#include "InAndOutRowExpressionContext.h"
#include "Aql/AqlItemBlock.h"
#include "Aql/AqlValue.h"
#include "Aql/Variable.h"
#include "Basics/Exceptions.h"

#include <limits.h>

using namespace arangodb;
using namespace arangodb::aql;

InAndOutRowExpressionContext::InAndOutRowExpressionContext(
    Query* query, std::vector<Variable const*> const&& vars,
    std::vector<RegisterId> const&& regs, size_t vertexVarIdx,
    size_t edgeVarIdx, size_t pathVarIdx)
    : QueryExpressionContext(query),
      _input{CreateInvalidInputRowHint()},
      _vars(std::move(vars)),
      _regs(std::move(regs)),
      _vertexVarIdx(vertexVarIdx),
      _edgeVarIdx(edgeVarIdx),
      _pathVarIdx(pathVarIdx) {
  TRI_ASSERT(_vars.size() == _regs.size());
  TRI_ASSERT(_vertexVarIdx < _vars.size() ||
             _vertexVarIdx == std::numeric_limits<std::size_t>::max());
  TRI_ASSERT(_edgeVarIdx < _vars.size() ||
             _edgeVarIdx == std::numeric_limits<std::size_t>::max());
  TRI_ASSERT(_pathVarIdx < _vars.size() ||
             _pathVarIdx == std::numeric_limits<std::size_t>::max());
}

void InAndOutRowExpressionContext::setInputRow(InputAqlItemRow input) {
  TRI_ASSERT(input.isInitialized());
  _input = input;
}

void InAndOutRowExpressionContext::invalidateInputRow() {
  _input = InputAqlItemRow{CreateInvalidInputRowHint{}};
}

AqlValue const& InAndOutRowExpressionContext::getRegisterValue(size_t i) const {
  TRI_ASSERT(_input.isInitialized());
  TRI_ASSERT(i < _regs.size());
  if (i == _vertexVarIdx) {
    return _vertexValue;
  }
  if (i == _edgeVarIdx) {
    return _edgeValue;
  }
  if (i == _pathVarIdx) {
    return _pathValue;
  }
  // Search InputRow
  RegisterId const& regId = _regs[i];
  TRI_ASSERT(regId < _input.getNrRegisters());
  return _input.getValue(regId);
}

AqlValue InAndOutRowExpressionContext::getVariableValue(Variable const* variable, bool doCopy,
                                                        bool& mustDestroy) const {
  TRI_ASSERT(_input.isInitialized());
  for (size_t i = 0; i < _vars.size(); ++i) {
    auto const& v = _vars[i];
    if (v->id == variable->id) {
      TRI_ASSERT(i < _regs.size());
      if (doCopy) {
        mustDestroy = true;
        if (i == _vertexVarIdx) {
          return _vertexValue.clone();
        }
        if (i == _edgeVarIdx) {
          return _edgeValue.clone();
        }
        if (i == _pathVarIdx) {
          return _pathValue.clone();
        }
        // Search InputRow
        RegisterId const& regId = _regs[i];
        TRI_ASSERT(regId < _input.getNrRegisters());
        return _input.getValue(regId).clone();
      } else {
        mustDestroy = false;
        if (i == _vertexVarIdx) {
          return _vertexValue;
        }
        if (i == _edgeVarIdx) {
          return _edgeValue;
        }
        if (i == _pathVarIdx) {
          return _pathValue;
        }
        // Search InputRow
        RegisterId const& regId = _regs[i];
        TRI_ASSERT(regId < _input.getNrRegisters());
        return _input.getValue(regId);
      }
    }
  }

  std::string msg("variable not found '");
  msg.append(variable->name);
  // NOTE: PRUNE is the only feature using this context.
  msg.append("' in PRUNE statement");
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, msg.c_str());
}
