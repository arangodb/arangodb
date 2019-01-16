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


using namespace arangodb::aql;

InAndOutRowExpressionContext::InAndOutRowExpressionContext(Query* query, size_t inputRowId, AqlItemBlock const* inputBlock,
                               size_t outputRowId, AqlItemBlock const* outputBlock,
                               std::vector<Variable const*> const& vars,
                               std::vector<RegisterId> const& regs)
      : QueryExpressionContext(query),
        _inputRowId(inputRowId),
        _inputBlock(inputBlock),
        _outputRowId(outputRowId),
        _outputBlock(outputBlock),
        _endOfInput(_inputBlock->getNrRegs()),
        _vars(vars),
        _regs(regs) {
          TRI_ASSERT(_inputBlock != nullptr);
          TRI_ASSERT(_outputBlock != nullptr);
          // The currently implemented RegisterPlanning in AQL guarantees that
          // new variables are only appended to the end of the registers.
          // Old variables will stay
          // If this does not hold true anymore we need to fix _endOfInput
          // decider variable.
          TRI_ASSERT(_inputBlock->getNrRegs() <= _outputBlock->getNrRegs());
        }


Variable const* InAndOutRowExpressionContext::getVariable(size_t i) const {
  return _vars[i];
}

AqlValue const& InAndOutRowExpressionContext::getRegisterValue(size_t i) const {
  TRI_ASSERT(i < _outputBlock->getNrRegs());
  TRI_ASSERT(i < _regs.size());
  RegisterId const& regId = _regs[i];
  if (regId < _endOfInput) {
    return _inputBlock->getValueReference(_inputRowId, regId);
  }
  return _outputBlock->getValueReference(_outputRowId, regId);
}

AqlValue InAndOutRowExpressionContext::getVariableValue(Variable const* variable, bool doCopy,
                                                 bool& mustDestroy) const {
  size_t i = 0;
  for (auto const& v : _vars) {
    if (v->id == variable->id) {
      TRI_ASSERT(i < _regs.size());
      RegisterId const& regId = _regs[i];
      if (regId < _endOfInput) {
        if (doCopy) {
          mustDestroy = true;  // as we are copying
          return _inputBlock->getValueReference(_inputRowId, regId).clone();
        }
        mustDestroy = false;
        return _inputBlock->getValueReference(_inputRowId, regId);
      } else {
        if (doCopy) {
          mustDestroy = true;  // as we are copying
          return _outputBlock->getValueReference(_outputRowId, regId).clone();
        }
        mustDestroy = false;
        return _outputBlock->getValueReference(_outputRowId, regId);
      }
    }
    ++i;
  }

  std::string msg("variable not found '");
  msg.append(variable->name);
  // NOTE: PRUNE is the only feature using this context.
  msg.append("' in PRUNE statement");
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, msg.c_str());
}

