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

#include "Basics/Exceptions.h"
#include "Basics/voc-errors.h"

namespace arangodb::aql::functions {

AqlValue ApproxNearCosine(ExpressionContext* expressionContext,
                          AstNode const& node,
                          VPackFunctionParametersView parameters) {
  THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_QUERY_VECTOR_SEARCH_NOT_APPLIED,
      "Vector search could not be applied. Please ensure a vector index has "
      "been created and that your query uses the correct syntax for vector "
      "search.  Note that filtering is currently not supported "
      "with vector search.");
}

AqlValue ApproxNearL2(ExpressionContext* expressionContext, AstNode const& node,
                      VPackFunctionParametersView parameters) {
  THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_QUERY_VECTOR_SEARCH_NOT_APPLIED,
      "Vector search could not be applied. Please ensure a vector index has "
      "been created and that your query uses the correct syntax for vector "
      "search.  Note that filtering is currently not supported "
      "with vector search.");
}

AqlValue ApproxNearInnerProduct(ExpressionContext* expressionContext,
                                AstNode const& node,
                                VPackFunctionParametersView parameters) {
  THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_QUERY_VECTOR_SEARCH_NOT_APPLIED,
      "Vector search could not be applied. Please ensure a vector index has "
      "been created and that your query uses the correct syntax for vector "
      "search.  Note that filtering is currently not supported "
      "with vector search.");
}

}  // namespace arangodb::aql::functions
