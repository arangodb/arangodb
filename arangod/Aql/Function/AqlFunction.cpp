////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "Aql/Function/AqlFunction.h"

#include "Aql/ExpressionContext.h"

namespace arangodb::aql::functions {

auto detail::toAqlValue(ExpressionContext& expressionContext,
                        std::string_view functionName, ResultT<AqlValue> result)
    -> AqlValue {
  if (result.ok()) {
    return std::move(*result);
  }
  auto const code = result.errorNumber();
  // argument mismatch warnings have their own message format
  if (code == TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH ||
      code == TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH) {
    registerWarning(&expressionContext, functionName, code);
  } else {
    registerWarning(&expressionContext, functionName, result.result());
  }
  return AqlValue{AqlValueHintNull{}};
}

}  // namespace arangodb::aql::functions
