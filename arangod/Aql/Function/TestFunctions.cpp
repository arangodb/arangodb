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
#include "Basics/Exceptions.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>

using namespace arangodb;

namespace arangodb::aql {

AqlValue functions::Assert(ExpressionContext* expressionContext, AstNode const&,
                           VPackFunctionParametersView parameters) {
  // cppcheck-suppress variableScope
  static char const* AFN = "ASSERT";

  auto const expr =
      aql::functions::extractFunctionParameterValue(parameters, 0);
  auto const message =
      aql::functions::extractFunctionParameterValue(parameters, 1);

  if (!message.isString()) {
    registerInvalidArgumentWarning(expressionContext, AFN);
    return AqlValue(AqlValueHintNull());
  }
  if (!expr.toBoolean()) {
    expressionContext->registerError(TRI_ERROR_QUERY_USER_ASSERT,
                                     message.slice().stringView());
  }
  return AqlValue(AqlValueHintBool(true));
}

AqlValue functions::Warn(ExpressionContext* expressionContext, AstNode const&,
                         VPackFunctionParametersView parameters) {
  // cppcheck-suppress variableScope
  static char const* AFN = "WARN";

  auto const expr =
      aql::functions::extractFunctionParameterValue(parameters, 0);
  auto const message =
      aql::functions::extractFunctionParameterValue(parameters, 1);

  if (!message.isString()) {
    registerInvalidArgumentWarning(expressionContext, AFN);
    return AqlValue(AqlValueHintNull());
  }

  if (!expr.toBoolean()) {
    expressionContext->registerWarning(TRI_ERROR_QUERY_USER_WARN,
                                       message.slice().stringView());
    return AqlValue(AqlValueHintBool(false));
  }
  return AqlValue(AqlValueHintBool(true));
}

AqlValue functions::Fail(ExpressionContext* expressionContext, AstNode const&,
                         VPackFunctionParametersView parameters) {
  if (parameters.size() == 0) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FAIL_CALLED, "");
  }

  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  if (!value.isString()) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FAIL_CALLED, "");
  }

  transaction::Methods* trx = &expressionContext->trx();
  auto* vopts = &trx->vpackOptions();
  AqlValueMaterializer materializer(vopts);
  VPackSlice str = materializer.slice(value);
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_FAIL_CALLED, str.copyString());
}

}  // namespace arangodb::aql
