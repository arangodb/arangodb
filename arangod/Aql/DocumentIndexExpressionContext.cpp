////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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

#include "DocumentIndexExpressionContext.h"
#include "Aql/AqlValue.h"

using namespace arangodb::aql;

DocumentIndexExpressionContext::DocumentIndexExpressionContext(
    Query* query, AqlValue (*getValue)(void const* ctx, Variable const* var, bool doCopy),
    void const* ctx)
  : QueryExpressionContext(query), _getValue(getValue), _ctx(ctx) {}

size_t DocumentIndexExpressionContext::numRegisters() const {
  // hard-coded
  return 1;
}

AqlValue DocumentIndexExpressionContext::getVariableValue(Variable const* variable, bool doCopy,
                                                          bool& mustDestroy) const {
  if (doCopy) {
    mustDestroy = true;  // as we are copying
  } else {
    mustDestroy = false;
  }
  return _getValue(_ctx, variable, doCopy);
}
