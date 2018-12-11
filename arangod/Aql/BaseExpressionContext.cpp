////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "BaseExpressionContext.h"
#include "Basics/Exceptions.h"
#include "Aql/AqlItemBlock.h"
#include "Aql/AqlValue.h"
#include "Aql/Variable.h"

using namespace arangodb::aql;

AqlValue const& BaseExpressionContext::getRegisterValue(size_t i) const {
  return _argv->getValueReference(_startPos, (*_regs)[i]);
}

Variable const* BaseExpressionContext::getVariable(size_t i) const {
  return (*_vars)[i];
}

AqlValue BaseExpressionContext::getVariableValue(Variable const* variable, bool doCopy, bool& mustDestroy) const {
  TRI_ASSERT(_vars != nullptr);
  TRI_ASSERT(_regs != nullptr);
  TRI_ASSERT(_argv != nullptr);

  size_t i = 0;
  for (auto const& v : *_vars) {
    if (v->id == variable->id) {
      TRI_ASSERT(i < _regs->size());
      if (doCopy) {
        mustDestroy = true;  // as we are copying
        return _argv->getValueReference(_startPos, (*_regs)[i]).clone();
      }
      mustDestroy = false;
      return _argv->getValueReference(_startPos, (*_regs)[i]);
    }
    ++i;
  }

  std::string msg("variable not found '");
  msg.append(variable->name);
  msg.append("' in executeSimpleExpression()");
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, msg.c_str());
}
