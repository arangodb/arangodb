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
#include "Random/UniformCharacter.h"

using namespace arangodb;

namespace arangodb::aql {

/// @brief function RAND
AqlValue functions::Rand(ExpressionContext*, AstNode const&,
                         VPackFunctionParametersView parameters) {
  // This random functionality is not too good yet...
  return aql::functions::numberValue(
      static_cast<double>(std::rand()) / RAND_MAX, true);
}

/// @brief function RANDOM_TOKEN
AqlValue functions::RandomToken(ExpressionContext*, AstNode const&,
                                VPackFunctionParametersView parameters) {
  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  int64_t const length = value.toInt64();
  if (length < 0 || length > 65536) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "RANDOM_TOKEN");
  }

  UniformCharacter generator(
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
  return AqlValue(generator.random(static_cast<size_t>(length)));
}

}  // namespace arangodb::aql
