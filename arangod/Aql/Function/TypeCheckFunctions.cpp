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

#include "Aql/AqlValue.h"
#include "Aql/AqlValueMaterializer.h"
#include "Aql/AstNode.h"
#include "Aql/ExpressionContext.h"
#include "Aql/Function.h"
#include "Aql/Functions.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>

using namespace arangodb;

namespace arangodb::aql {

/// @brief function IS_NULL
AqlValue functions::IsNull(ExpressionContext*, AstNode const&,
                           VPackFunctionParametersView parameters) {
  AqlValue const& a = extractFunctionParameterValue(parameters, 0);
  return AqlValue(AqlValueHintBool(a.isNull(true)));
}

/// @brief function IS_BOOL
AqlValue functions::IsBool(ExpressionContext*, AstNode const&,
                           VPackFunctionParametersView parameters) {
  AqlValue const& a = extractFunctionParameterValue(parameters, 0);
  return AqlValue(AqlValueHintBool(a.isBoolean()));
}

/// @brief function IS_NUMBER
AqlValue functions::IsNumber(ExpressionContext*, AstNode const&,
                             VPackFunctionParametersView parameters) {
  AqlValue const& a = extractFunctionParameterValue(parameters, 0);
  return AqlValue(AqlValueHintBool(a.isNumber()));
}

/// @brief function IS_STRING
AqlValue functions::IsString(ExpressionContext*, AstNode const&,
                             VPackFunctionParametersView parameters) {
  AqlValue const& a = extractFunctionParameterValue(parameters, 0);
  return AqlValue(AqlValueHintBool(a.isString()));
}

/// @brief function IS_ARRAY
AqlValue functions::IsArray(ExpressionContext*, AstNode const&,
                            VPackFunctionParametersView parameters) {
  AqlValue const& a = extractFunctionParameterValue(parameters, 0);
  return AqlValue(AqlValueHintBool(a.isArray()));
}

/// @brief function IS_OBJECT
AqlValue functions::IsObject(ExpressionContext*, AstNode const&,
                             VPackFunctionParametersView parameters) {
  AqlValue const& a = extractFunctionParameterValue(parameters, 0);
  return AqlValue(AqlValueHintBool(a.isObject()));
}

/// @brief function TYPENAME
AqlValue functions::Typename(ExpressionContext*, AstNode const&,
                             VPackFunctionParametersView parameters) {
  AqlValue const& value = extractFunctionParameterValue(parameters, 0);
  return AqlValue(value.getTypeString());
}

}  // namespace arangodb::aql
