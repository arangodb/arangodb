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
#include "Basics/VelocyPackHelper.h"
#include "Transaction/Context.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>

#include <algorithm>
#include <vector>

using namespace arangodb;

namespace arangodb::aql {
namespace {
/// @brief converts a value into a number value
double valueToNumber(VPackSlice slice, bool& isValid) {
  if (slice.isNull()) {
    isValid = true;
    return 0.0;
  }
  if (slice.isBoolean()) {
    isValid = true;
    return (slice.getBoolean() ? 1.0 : 0.0);
  }
  if (slice.isNumber()) {
    isValid = true;
    return slice.getNumericValue<double>();
  }
  if (slice.isString()) {
    std::string const str = slice.copyString();
    try {
      if (str.empty()) {
        isValid = true;
        return 0.0;
      }
      size_t behind = 0;
      double value = std::stod(str, &behind);
      while (behind < str.size()) {
        char c = str[behind];
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n' && c != '\f') {
          isValid = false;
          return 0.0;
        }
        ++behind;
      }
      isValid = true;
      return value;
    } catch (...) {
      size_t behind = 0;
      while (behind < str.size()) {
        char c = str[behind];
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n' && c != '\f') {
          isValid = false;
          return 0.0;
        }
        ++behind;
      }
      // A string only containing whitespae-characters is valid and should
      // return 0.0
      // It throws in std::stod
      isValid = true;
      return 0.0;
    }
  }
  if (slice.isArray()) {
    VPackValueLength const n = slice.length();
    if (n == 0) {
      isValid = true;
      return 0.0;
    }
    if (n == 1) {
      return valueToNumber(slice.at(0), isValid);
    }
  }

  // All other values are invalid
  isValid = false;
  return 0.0;
}

/// @brief Computes the Variance of the given list.
///        If successful value will contain the variance and count
///        will contain the number of elements.
///        If not successful value and count contain garbage.
bool variance(VPackOptions const* vopts, AqlValue const& values, double& value,
              size_t& count) {
  TRI_ASSERT(values.isArray());
  value = 0.0;
  count = 0;
  bool unused = false;
  double mean = 0.0;

  AqlValueMaterializer materializer(vopts);
  VPackSlice slice = materializer.slice(values);

  for (VPackSlice element : VPackArrayIterator(slice)) {
    if (!element.isNull()) {
      if (!element.isNumber()) {
        return false;
      }
      double current = valueToNumber(element, unused);
      count++;
      double delta = current - mean;
      mean += delta / count;
      value += delta * (current - mean);
    }
  }
  return true;
}

/// @brief Sorts the given list of Numbers in ASC order.
///        Removes all null entries.
///        Returns false if the list contains non-number values.
bool sortNumberList(VPackOptions const* vopts, AqlValue const& values,
                    std::vector<double>& result) {
  TRI_ASSERT(values.isArray());
  TRI_ASSERT(result.empty());
  bool unused;
  AqlValueMaterializer materializer(vopts);
  VPackSlice slice = materializer.slice(values);

  VPackArrayIterator it(slice);
  result.reserve(it.size());
  for (auto const& element : it) {
    if (!element.isNull()) {
      if (!element.isNumber()) {
        return false;
      }
      result.emplace_back(valueToNumber(element, unused));
    }
  }
  std::sort(result.begin(), result.end());
  return true;
}

}  // namespace

/// @brief function VARIANCE_SAMPLE
AqlValue functions::VarianceSample(ExpressionContext* expressionContext,
                                   AstNode const&,
                                   VPackFunctionParametersView parameters) {
  static char const* AFN = "VARIANCE_SAMPLE";

  AqlValue const& list =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  if (!list.isArray()) {
    registerWarning(expressionContext, AFN, TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(AqlValueHintNull());
  }

  double value = 0.0;
  size_t count = 0;

  transaction::Methods* trx = &expressionContext->trx();
  auto* vopts = &trx->vpackOptions();
  if (!variance(vopts, list, value, count)) {
    registerWarning(expressionContext, AFN,
                    TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE);
    return AqlValue(AqlValueHintNull());
  }

  if (count < 2) {
    return AqlValue(AqlValueHintNull());
  }

  return numberValue(value / (count - 1), true);
}

/// @brief function VARIANCE_POPULATION
AqlValue functions::VariancePopulation(ExpressionContext* expressionContext,
                                       AstNode const&,
                                       VPackFunctionParametersView parameters) {
  static char const* AFN = "VARIANCE_POPULATION";

  AqlValue const& list =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  if (!list.isArray()) {
    registerWarning(expressionContext, AFN, TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(AqlValueHintNull());
  }

  double value = 0.0;
  size_t count = 0;

  transaction::Methods* trx = &expressionContext->trx();
  auto* vopts = &trx->vpackOptions();
  if (!variance(vopts, list, value, count)) {
    registerWarning(expressionContext, AFN,
                    TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE);
    return AqlValue(AqlValueHintNull());
  }

  if (count < 1) {
    return AqlValue(AqlValueHintNull());
  }

  return numberValue(value / count, true);
}

/// @brief function STDDEV_SAMPLE
AqlValue functions::StdDevSample(ExpressionContext* expressionContext,
                                 AstNode const&,
                                 VPackFunctionParametersView parameters) {
  static char const* AFN = "STDDEV_SAMPLE";

  AqlValue const& list =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  if (!list.isArray()) {
    registerWarning(expressionContext, AFN, TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(AqlValueHintNull());
  }

  double value = 0.0;
  size_t count = 0;

  transaction::Methods* trx = &expressionContext->trx();
  auto* vopts = &trx->vpackOptions();
  if (!variance(vopts, list, value, count)) {
    registerWarning(expressionContext, AFN,
                    TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE);
    return AqlValue(AqlValueHintNull());
  }

  if (count < 2) {
    return AqlValue(AqlValueHintNull());
  }

  return numberValue(std::sqrt(value / (count - 1)), true);
}

/// @brief function STDDEV_POPULATION
AqlValue functions::StdDevPopulation(ExpressionContext* expressionContext,
                                     AstNode const&,
                                     VPackFunctionParametersView parameters) {
  static char const* AFN = "STDDEV_POPULATION";

  AqlValue const& list =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  if (!list.isArray()) {
    registerWarning(expressionContext, AFN, TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(AqlValueHintNull());
  }

  double value = 0.0;
  size_t count = 0;

  transaction::Methods* trx = &expressionContext->trx();
  auto* vopts = &trx->vpackOptions();
  if (!variance(vopts, list, value, count)) {
    registerWarning(expressionContext, AFN,
                    TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE);
    return AqlValue(AqlValueHintNull());
  }

  if (count < 1) {
    return AqlValue(AqlValueHintNull());
  }

  return numberValue(std::sqrt(value / count), true);
}

/// @brief function MEDIAN
AqlValue functions::Median(ExpressionContext* expressionContext, AstNode const&,
                           VPackFunctionParametersView parameters) {
  static char const* AFN = "MEDIAN";

  AqlValue const& list =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  if (!list.isArray()) {
    registerWarning(expressionContext, AFN, TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(AqlValueHintNull());
  }

  transaction::Methods* trx = &expressionContext->trx();
  auto* vopts = &trx->vpackOptions();

  std::vector<double> values;
  if (!sortNumberList(vopts, list, values)) {
    registerWarning(expressionContext, AFN,
                    TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE);
    return AqlValue(AqlValueHintNull());
  }

  if (values.empty()) {
    return AqlValue(AqlValueHintNull());
  }
  size_t const l = values.size();
  size_t midpoint = l / 2;

  if (l % 2 == 0) {
    return numberValue((values[midpoint - 1] + values[midpoint]) / 2, true);
  }
  return numberValue(values[midpoint], true);
}

/// @brief function PERCENTILE
AqlValue functions::Percentile(ExpressionContext* expressionContext,
                               AstNode const&,
                               VPackFunctionParametersView parameters) {
  static char const* AFN = "PERCENTILE";

  AqlValue const& list =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  if (!list.isArray()) {
    registerWarning(expressionContext, AFN, TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(AqlValueHintNull());
  }

  AqlValue const& border =
      aql::functions::extractFunctionParameterValue(parameters, 1);

  if (!border.isNumber()) {
    registerWarning(expressionContext, AFN,
                    TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(AqlValueHintNull());
  }

  double p = border.toDouble();
  if (p <= 0.0 || p > 100.0) {
    registerWarning(expressionContext, AFN,
                    TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(AqlValueHintNull());
  }

  bool useInterpolation = false;

  if (parameters.size() == 3) {
    AqlValue const& methodValue =
        aql::functions::extractFunctionParameterValue(parameters, 2);
    if (!methodValue.isString()) {
      registerWarning(expressionContext, AFN,
                      TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
      return AqlValue(AqlValueHintNull());
    }
    std::string_view method = methodValue.slice().stringView();
    if (method == "interpolation") {
      useInterpolation = true;
    } else if (method == "rank") {
      useInterpolation = false;
    } else {
      registerWarning(expressionContext, AFN,
                      TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
      return AqlValue(AqlValueHintNull());
    }
  }

  transaction::Methods* trx = &expressionContext->trx();
  auto* vopts = &trx->vpackOptions();

  std::vector<double> values;
  if (!sortNumberList(vopts, list, values)) {
    registerWarning(expressionContext, AFN,
                    TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE);
    return AqlValue(AqlValueHintNull());
  }

  if (values.empty()) {
    return AqlValue(AqlValueHintNull());
  }

  size_t l = values.size();
  if (l == 1) {
    return numberValue(values[0], true);
  }

  TRI_ASSERT(l > 1);

  if (useInterpolation) {
    double const idx = p * (l + 1) / 100.0;
    double const pos = floor(idx);

    if (pos >= l) {
      return numberValue(values[l - 1], true);
    }
    if (pos <= 0) {
      return AqlValue(AqlValueHintNull());
    }

    double const delta = idx - pos;
    return numberValue(delta * (values[static_cast<size_t>(pos)] -
                                values[static_cast<size_t>(pos) - 1]) +
                           values[static_cast<size_t>(pos) - 1],
                       true);
  }

  double const idx = p * l / 100.0;
  double const pos = ceil(idx);
  if (pos >= l) {
    return numberValue(values[l - 1], true);
  }
  if (pos <= 0) {
    return AqlValue(AqlValueHintNull());
  }

  return numberValue(values[static_cast<size_t>(pos) - 1], true);
}

/// @brief function MIN
AqlValue functions::Min(ExpressionContext* expressionContext, AstNode const&,
                        VPackFunctionParametersView parameters) {
  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  if (!value.isArray()) {
    // not an array
    registerWarning(expressionContext, "MIN", TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(AqlValueHintNull());
  }

  transaction::Methods* trx = &expressionContext->trx();
  auto* vopts = &trx->vpackOptions();

  AqlValueMaterializer materializer(vopts);
  VPackSlice slice = materializer.slice(value);

  VPackSlice minValue;
  auto options = trx->transactionContextPtr()->getVPackOptions();
  for (VPackSlice it : VPackArrayIterator(slice)) {
    if (it.isNull()) {
      continue;
    }
    if (minValue.isNone() ||
        basics::VelocyPackHelper::compare(it, minValue, true, options) < 0) {
      minValue = it;
    }
  }
  if (minValue.isNone()) {
    return AqlValue(AqlValueHintNull());
  }
  return AqlValue(minValue);
}

/// @brief function MAX
AqlValue functions::Max(ExpressionContext* expressionContext, AstNode const&,
                        VPackFunctionParametersView parameters) {
  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  if (!value.isArray()) {
    // not an array
    registerWarning(expressionContext, "MAX", TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(AqlValueHintNull());
  }

  transaction::Methods* trx = &expressionContext->trx();
  auto* vopts = &trx->vpackOptions();

  AqlValueMaterializer materializer(vopts);
  VPackSlice slice = materializer.slice(value);
  VPackSlice maxValue;
  auto options = trx->transactionContextPtr()->getVPackOptions();
  for (VPackSlice it : VPackArrayIterator(slice)) {
    if (maxValue.isNone() ||
        basics::VelocyPackHelper::compare(it, maxValue, true, options) > 0) {
      maxValue = it;
    }
  }
  if (maxValue.isNone()) {
    return AqlValue(AqlValueHintNull());
  }
  return AqlValue(maxValue);
}

/// @brief function SUM
AqlValue functions::Sum(ExpressionContext* expressionContext, AstNode const&,
                        VPackFunctionParametersView parameters) {
  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  if (!value.isArray()) {
    // not an array
    registerWarning(expressionContext, "SUM", TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(AqlValueHintNull());
  }

  transaction::Methods* trx = &expressionContext->trx();
  auto* vopts = &trx->vpackOptions();

  AqlValueMaterializer materializer(vopts);
  VPackSlice slice = materializer.slice(value);
  double sum = 0.0;
  for (VPackSlice it : VPackArrayIterator(slice)) {
    if (it.isNull()) {
      continue;
    }
    if (!it.isNumber()) {
      return AqlValue(AqlValueHintNull());
    }
    double const number = it.getNumericValue<double>();

    if (!std::isnan(number) && number != HUGE_VAL && number != -HUGE_VAL) {
      sum += number;
    }
  }

  return numberValue(sum, false);
}

/// @brief function AVERAGE
AqlValue functions::Average(ExpressionContext* expressionContext,
                            AstNode const&,
                            VPackFunctionParametersView parameters) {
  static char const* AFN = "AVERAGE";
  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  if (!value.isArray()) {
    // not an array
    registerWarning(expressionContext, AFN, TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(AqlValueHintNull());
  }

  transaction::Methods* trx = &expressionContext->trx();
  auto* vopts = &trx->vpackOptions();

  AqlValueMaterializer materializer(vopts);
  VPackSlice slice = materializer.slice(value);

  double sum = 0.0;
  size_t count = 0;
  for (VPackSlice v : VPackArrayIterator(slice)) {
    if (v.isNull()) {
      continue;
    }
    if (!v.isNumber()) {
      registerWarning(expressionContext, AFN, TRI_ERROR_QUERY_ARRAY_EXPECTED);
      return AqlValue(AqlValueHintNull());
    }

    // got a numeric value
    double const number = v.getNumericValue<double>();

    if (!std::isnan(number) && number != HUGE_VAL && number != -HUGE_VAL) {
      sum += number;
      ++count;
    }
  }

  if (count > 0 && !std::isnan(sum) && sum != HUGE_VAL && sum != -HUGE_VAL) {
    return numberValue(sum / static_cast<size_t>(count), false);
  }

  return AqlValue(AqlValueHintNull());
}

/// @brief function PRODUCT
AqlValue functions::Product(ExpressionContext* expressionContext,
                            AstNode const&,
                            VPackFunctionParametersView parameters) {
  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  if (!value.isArray()) {
    // not an array
    registerWarning(expressionContext, "PRODUCT",
                    TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(AqlValueHintNull());
  }

  transaction::Methods* trx = &expressionContext->trx();
  auto* vopts = &trx->vpackOptions();

  AqlValueMaterializer materializer(vopts);
  VPackSlice slice = materializer.slice(value);
  double product = 1.0;
  for (VPackSlice it : VPackArrayIterator(slice)) {
    if (it.isNull()) {
      continue;
    }
    if (!it.isNumber()) {
      return AqlValue(AqlValueHintNull());
    }
    double const number = it.getNumericValue<double>();

    if (!std::isnan(number) && number != HUGE_VAL && number != -HUGE_VAL) {
      product *= number;
    }
  }

  return numberValue(product, false);
}

}  // namespace arangodb::aql
