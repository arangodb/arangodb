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

#include <span>

using namespace arangodb;

namespace arangodb::aql {

/// @brief function PASSTHRU
AqlValue functions::Passthru(ExpressionContext*, AstNode const&,
                             VPackFunctionParametersView parameters) {
  if (parameters.empty()) {
    return AqlValue(AqlValueHintNull());
  }

  return aql::functions::extractFunctionParameterValue(parameters, 0).clone();
}

AqlValue functions::NotImplemented(ExpressionContext* expressionContext,
                                   AstNode const&,
                                   VPackFunctionParametersView) {
  registerError(expressionContext, "UNKNOWN", TRI_ERROR_NOT_IMPLEMENTED);
  return AqlValue(AqlValueHintNull());
}

aql::AqlValue functions::NotImplementedEE(aql::ExpressionContext*,
                                          aql::AstNode const& node,
                                          std::span<aql::AqlValue const>) {
  THROW_ARANGO_EXCEPTION_FORMAT(
      TRI_ERROR_NOT_IMPLEMENTED,
      "Function '%s' is available in ArangoDB Enterprise Edition only.",
      getFunctionName(node).data());
}

#ifndef USE_ENTERPRISE
aql::AqlValue functions::MinHashCount(aql::ExpressionContext* ctx,
                                      aql::AstNode const& node,
                                      std::span<aql::AqlValue const> values) {
  return NotImplementedEE(ctx, node, values);
}

aql::AqlValue functions::MinHash(aql::ExpressionContext* ctx,
                                 aql::AstNode const& node,
                                 std::span<aql::AqlValue const> values) {
  return NotImplementedEE(ctx, node, values);
}

aql::AqlValue functions::MinHashError(aql::ExpressionContext* ctx,
                                      aql::AstNode const& node,
                                      std::span<aql::AqlValue const> values) {
  return NotImplementedEE(ctx, node, values);
}

aql::AqlValue functions::MinHashMatch(aql::ExpressionContext* ctx,
                                      aql::AstNode const& node,
                                      std::span<aql::AqlValue const> values) {
  return NotImplementedEE(ctx, node, values);
}
#endif

}  // namespace arangodb::aql
