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
/// @author Jure Bajic
////////////////////////////////////////////////////////////////////////////////

#include "Aql/Functions.h"

#include "Aql/AqlValue.h"
#include "Aql/AstNode.h"
#include "Aql/ExpressionContext.h"

#include "Logger/LogMacros.h"

namespace arangodb::aql::functions {

AqlValue ApproxNearCosine(ExpressionContext* expressionContext,
                          AstNode const& node,
                          VPackFunctionParametersView parameters) {
  LOG_TOPIC("1d8b9", WARN, Logger::AQL) << "vector index is not being used";
  return CosineSimilarity(expressionContext, node, parameters);
}

AqlValue ApproxNearL2(ExpressionContext* expressionContext, AstNode const& node,
                      VPackFunctionParametersView parameters) {
  LOG_TOPIC("fe17d", WARN, Logger::AQL) << "vector index is not being used";
  return L2Distance(expressionContext, node, parameters);
}

}  // namespace arangodb::aql::functions
