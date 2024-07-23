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
////////////////////////////////////////////////////////////////////////////////

#include <cstddef>
#include <vector>

#include "Aql/AqlValue.h"
#include "Aql/AstNode.h"
#include "Aql/ExpressionContext.h"
#include "Aql/Functions.h"
#include "Basics/Exceptions.h"
#include "Enterprise/Vector/LocalSensitiveHashing.h"
#include "Indexes/VectorIndexDefinition.h"
#include "Inspection/VPack.h"
#include "Logger/LogMacros.h"

namespace arangodb::aql::functions {

AqlValue ApproxNear(ExpressionContext*, AstNode const&,
                    VPackFunctionParametersView parameters) {
  AqlValue const& lhs =
      aql::functions::extractFunctionParameterValue(parameters, 0);
  AqlValue const& rhs =
      aql::functions::extractFunctionParameterValue(parameters, 1);

  if (!lhs.isArray() || lhs.isRange() || !rhs.isArray() || rhs.isRange()) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,
        "APPROX_NEAR requires input parameters to be arrays");
  }

  if (lhs.length() != rhs.length()) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,
        "APPROX_NEAR inputs need to be of the same size");
  }

  std::vector<double> lhsVec;
  velocypack::deserialize(lhs.slice(), lhsVec);
  std::vector<double> rhsVec;
  velocypack::deserialize(rhs.slice(), rhsVec);

  // TODO what to do when there is no index, how to compare against fields
  std::vector<arangodb::VectorIndexRandomVector> randomFunctions{
      {.bParam = 0.1, .wParam = 0.004, .Vparam = {0.1, 0.1, 0.1}},
      {.bParam = 0.1, .wParam = 0.004, .Vparam = {0.3, 0.2, 0.1}},
      {.bParam = 0.1, .wParam = 0.004, .Vparam = {0.1, 0.2, 0.5}},
      {.bParam = 0.1, .wParam = 0.004, .Vparam = {0.3, 0.1, 0.2}},
  };
  arangodb::VectorIndexDefinition defaultVectorIndexDefinition{
      .dimensions = rhsVec.size(),
      .min = 0,
      .max = 1,
      .Kparameter = 2,
      .Lparameter = 2,
      .randomFunctions = randomFunctions};

  auto const lhsHashedStrings =
      arangodb::calculateHashedStrings(defaultVectorIndexDefinition, lhsVec);
  auto const rhsHashedStrings =
      arangodb::calculateHashedStrings(defaultVectorIndexDefinition, rhsVec);

  LOG_DEVEL << "COMAPRISO VEC: " << lhsVec << " " << rhsVec;
  for (std::size_t i{0}; i < lhsHashedStrings.size(); ++i) {
    LOG_DEVEL
        << "COMPARISON: " << std::hex
        << *reinterpret_cast<const std::uint16_t*>(lhsHashedStrings[i].data())
        << "  "
        << *reinterpret_cast<const std::uint16_t*>(rhsHashedStrings[i].data());
    if (lhsHashedStrings[i] == rhsHashedStrings[i]) {
      return AqlValue(AqlValueHintBool(true));
    }
  }

  return AqlValue(AqlValueHintBool(false));
}

}  // namespace arangodb::aql::functions
