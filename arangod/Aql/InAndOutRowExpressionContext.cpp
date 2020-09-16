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

#include "InAndOutRowExpressionContext.h"

#include "Aql/AqlValue.h"
#include "Aql/RegisterPlan.h"
#include "Aql/Variable.h"
#include "Basics/Exceptions.h"

#include <velocypack/Slice.h>

using namespace arangodb;
using namespace arangodb::aql;

static bool testInternalIdValid(size_t id, std::vector<RegisterId> const& regs) {
  if (id == std::numeric_limits<std::size_t>::max()) {
    return true;
  }
  TRI_ASSERT(id < regs.size());
  return regs[id] == RegisterPlan::MaxRegisterId;
}

InAndOutRowExpressionContext::InAndOutRowExpressionContext(
    transaction::Methods& trx,
    QueryContext& context,
    AqlFunctionsInternalCache& cache,
    std::vector<Variable const*> vars,
    std::vector<RegisterId> regs, size_t vertexVarIdx,
    size_t edgeVarIdx, size_t pathVarIdx)
    : QueryExpressionContext(trx, context, cache),
      _input{CreateInvalidInputRowHint()},
      _vars(std::move(vars)),
      _regs(std::move(regs)),
      _vertexVarIdx(vertexVarIdx),
      _edgeVarIdx(edgeVarIdx),
      _pathVarIdx(pathVarIdx) {
  TRI_ASSERT(_vars.size() == _regs.size());
  TRI_ASSERT(testInternalIdValid(_vertexVarIdx, _regs));
  TRI_ASSERT(testInternalIdValid(_edgeVarIdx, _regs));
  TRI_ASSERT(testInternalIdValid(_pathVarIdx, _regs));
}

void InAndOutRowExpressionContext::setInputRow(InputAqlItemRow input) {
  TRI_ASSERT(input.isInitialized());
  _input = input;
}

void InAndOutRowExpressionContext::invalidateInputRow() {
  _input = InputAqlItemRow{CreateInvalidInputRowHint{}};
}

bool InAndOutRowExpressionContext::isDataFromCollection(Variable const* variable) const {
  for (size_t i = 0; i < _vars.size(); ++i) {
    auto const& v = _vars[i];
    if (v->id == variable->id) {
      if (variable->isDataFromCollection) {
        return true;
      }
      TRI_ASSERT(i < _regs.size());
      if (i == _vertexVarIdx ||
          i == _edgeVarIdx ||
          i == _pathVarIdx) {
        return true;
      }
    }
  }
  return false;
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
        TRI_ASSERT(regId < _input.getNumRegisters());
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
        TRI_ASSERT(regId < _input.getNumRegisters());
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

bool InAndOutRowExpressionContext::needsVertexValue() const {
  return _vertexVarIdx < _regs.size();
}

bool InAndOutRowExpressionContext::needsEdgeValue() const {
  return _edgeVarIdx < _regs.size();
}

bool InAndOutRowExpressionContext::needsPathValue() const {
  return _pathVarIdx < _regs.size();
}

void InAndOutRowExpressionContext::setVertexValue(velocypack::Slice v) {
  _vertexValue = AqlValue(AqlValueHintDocumentNoCopy(v.begin()));
}

void InAndOutRowExpressionContext::setEdgeValue(velocypack::Slice e) {
  _edgeValue = AqlValue(AqlValueHintDocumentNoCopy(e.begin()));
}

void InAndOutRowExpressionContext::setPathValue(velocypack::Slice p) {
  _pathValue = AqlValue(AqlValueHintDocumentNoCopy(p.begin()));
}
