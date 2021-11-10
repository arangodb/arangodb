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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "ExpressionContextMock.h"
#include "Aql/Variable.h"
#include "Basics/Exceptions.h"

ExpressionContextMock ExpressionContextMock::EMPTY;

ExpressionContextMock::~ExpressionContextMock() {
  for (auto& entry : vars) {
    entry.second.destroy();
  }
}

arangodb::aql::AqlValue ExpressionContextMock::getVariableValue(
    arangodb::aql::Variable const* variable,
    bool doCopy,
    bool& mustDestroy) const {
  auto it = vars.find(variable->name);
  if (vars.end() == it) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "Can't find variable " + variable->name);
//    return {};
  }

  if (doCopy) {
    mustDestroy = true;
    return it->second.clone();
  }

  mustDestroy = false;
  return it->second;
}
  
//TRI_vocbase_t& ExpressionContextMock::vocbase() const {
//  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
//}
//
//arangodb::aql::Query* ExpressionContextMock::query() const {
//  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
//}
