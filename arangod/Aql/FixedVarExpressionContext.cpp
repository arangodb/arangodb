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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "FixedVarExpressionContext.h"
#include "Aql/AqlValue.h"
#include "Aql/Variable.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

size_t FixedVarExpressionContext::numRegisters() const {
  return 0;
}

AqlValue const& FixedVarExpressionContext::getRegisterValue(size_t i) const {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

Variable const* FixedVarExpressionContext::getVariable(size_t i) const {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

AqlValue FixedVarExpressionContext::getVariableValue(Variable const* variable, bool doCopy, bool& mustDestroy) const {
  mustDestroy = false;
  auto it = _vars.find(variable);
  if (it == _vars.end()) {
    TRI_ASSERT(false);
    return AqlValue(AqlValueHintNull());
  }
  if (doCopy) {
    mustDestroy = true;
    return it->second.clone();
  }
  return it->second;
}

void FixedVarExpressionContext::clearVariableValues() {
  _vars.clear();
}

void FixedVarExpressionContext::setVariableValue(Variable const* var,
                                                 AqlValue const& value) {
  _vars.emplace(var, value);
}

void FixedVarExpressionContext::serializeAllVariables(
    transaction::Methods* trx, VPackBuilder& builder) const {
  TRI_ASSERT(builder.isOpenArray());
  for (auto const& it : _vars) {
    builder.openArray();
    it.first->toVelocyPack(builder);
    it.second.toVelocyPack(trx, builder, true);
    builder.close();
  }
}
