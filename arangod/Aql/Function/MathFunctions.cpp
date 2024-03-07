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

#include <cmath>

using namespace arangodb;

namespace arangodb::aql {

/// @brief function ROUND
AqlValue functions::Round(ExpressionContext*, AstNode const&,
                          VPackFunctionParametersView parameters) {
  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  double input = value.toDouble();

  // Rounds down for < x.4999 and up for > x.50000
  return numberValue(std::floor(input + 0.5), true);
}

/// @brief function ABS
AqlValue functions::Abs(ExpressionContext*, AstNode const&,
                        VPackFunctionParametersView parameters) {
  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  double input = value.toDouble();
  return numberValue(std::abs(input), true);
}

/// @brief function CEIL
AqlValue functions::Ceil(ExpressionContext*, AstNode const&,
                         VPackFunctionParametersView parameters) {
  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  double input = value.toDouble();
  return numberValue(std::ceil(input), true);
}

/// @brief function FLOOR
AqlValue functions::Floor(ExpressionContext*, AstNode const&,
                          VPackFunctionParametersView parameters) {
  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  double input = value.toDouble();
  return numberValue(std::floor(input), true);
}

/// @brief function SQRT
AqlValue functions::Sqrt(ExpressionContext*, AstNode const&,
                         VPackFunctionParametersView parameters) {
  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  double input = value.toDouble();
  return numberValue(std::sqrt(input), true);
}

/// @brief function POW
AqlValue functions::Pow(ExpressionContext*, AstNode const&,
                        VPackFunctionParametersView parameters) {
  AqlValue const& baseValue =
      aql::functions::extractFunctionParameterValue(parameters, 0);
  AqlValue const& expValue =
      aql::functions::extractFunctionParameterValue(parameters, 1);

  double base = baseValue.toDouble();
  double exp = expValue.toDouble();

  return numberValue(std::pow(base, exp), true);
}

/// @brief function LOG
AqlValue functions::Log(ExpressionContext*, AstNode const&,
                        VPackFunctionParametersView parameters) {
  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  double input = value.toDouble();
  return numberValue(std::log(input), true);
}

/// @brief function LOG2
AqlValue functions::Log2(ExpressionContext*, AstNode const&,
                         VPackFunctionParametersView parameters) {
  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  double input = value.toDouble();
  return numberValue(std::log2(input), true);
}

/// @brief function LOG10
AqlValue functions::Log10(ExpressionContext*, AstNode const&,
                          VPackFunctionParametersView parameters) {
  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  double input = value.toDouble();
  return numberValue(std::log10(input), true);
}

/// @brief function EXP
AqlValue functions::Exp(ExpressionContext*, AstNode const&,
                        VPackFunctionParametersView parameters) {
  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  double input = value.toDouble();
  return numberValue(std::exp(input), true);
}

/// @brief function EXP2
AqlValue functions::Exp2(ExpressionContext*, AstNode const&,
                         VPackFunctionParametersView parameters) {
  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  double input = value.toDouble();
  return numberValue(std::exp2(input), true);
}

/// @brief function SIN
AqlValue functions::Sin(ExpressionContext*, AstNode const&,
                        VPackFunctionParametersView parameters) {
  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  double input = value.toDouble();
  return numberValue(std::sin(input), true);
}

/// @brief function COS
AqlValue functions::Cos(ExpressionContext*, AstNode const&,
                        VPackFunctionParametersView parameters) {
  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  double input = value.toDouble();
  return numberValue(std::cos(input), true);
}

/// @brief function TAN
AqlValue functions::Tan(ExpressionContext*, AstNode const&,
                        VPackFunctionParametersView parameters) {
  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  double input = value.toDouble();
  return numberValue(std::tan(input), true);
}

/// @brief function ASIN
AqlValue functions::Asin(ExpressionContext*, AstNode const&,
                         VPackFunctionParametersView parameters) {
  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  double input = value.toDouble();
  return numberValue(std::asin(input), true);
}

/// @brief function ACOS
AqlValue functions::Acos(ExpressionContext*, AstNode const&,
                         VPackFunctionParametersView parameters) {
  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  double input = value.toDouble();
  return numberValue(std::acos(input), true);
}

/// @brief function ATAN
AqlValue functions::Atan(ExpressionContext*, AstNode const&,
                         VPackFunctionParametersView parameters) {
  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  double input = value.toDouble();
  return numberValue(std::atan(input), true);
}

/// @brief function ATAN2
AqlValue functions::Atan2(ExpressionContext*, AstNode const&,
                          VPackFunctionParametersView parameters) {
  AqlValue value1 =
      aql::functions::extractFunctionParameterValue(parameters, 0);
  AqlValue value2 =
      aql::functions::extractFunctionParameterValue(parameters, 1);

  double input1 = value1.toDouble();
  double input2 = value2.toDouble();
  return numberValue(std::atan2(input1, input2), true);
}

/// @brief function RADIANS
AqlValue functions::Radians(ExpressionContext*, AstNode const&,
                            VPackFunctionParametersView parameters) {
  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  double degrees = value.toDouble();
  // acos(-1) == PI
  return numberValue(degrees * (std::acos(-1.0) / 180.0), true);
}

/// @brief function DEGREES
AqlValue functions::Degrees(ExpressionContext*, AstNode const&,
                            VPackFunctionParametersView parameters) {
  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  double radians = value.toDouble();
  // acos(-1) == PI
  return numberValue(radians * (180.0 / std::acos(-1.0)), true);
}

/// @brief function PI
AqlValue functions::Pi(ExpressionContext*, AstNode const&,
                       VPackFunctionParametersView parameters) {
  // acos(-1) == PI
  return numberValue(std::acos(-1.0), true);
}

}  // namespace arangodb::aql
