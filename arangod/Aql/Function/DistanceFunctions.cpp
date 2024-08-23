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
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Sink.h>
#include <velocypack/Slice.h>

#include <algorithm>
#include <cmath>
#include <unordered_map>

using namespace arangodb;

namespace arangodb::aql {
namespace {
template<typename F>
AqlValue DistanceImpl(aql::ExpressionContext* expressionContext,
                      AstNode const& node,
                      aql::functions::VPackFunctionParametersView parameters,
                      F&& distanceFunc) {
  auto calculateDistance = [distanceFunc = std::forward<F>(distanceFunc),
                            expressionContext,
                            &node](VPackSlice lhs, VPackSlice rhs) {
    TRI_ASSERT(lhs.isArray());
    TRI_ASSERT(rhs.isArray());

    auto lhsIt = VPackArrayIterator(lhs);
    auto rhsIt = VPackArrayIterator(rhs);

    if (lhsIt.size() != rhsIt.size()) {
      aql::functions::registerWarning(
          expressionContext, aql::functions::getFunctionName(node).data(),
          TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
      return AqlValue(AqlValueHintNull());
    }

    return distanceFunc(lhsIt, rhsIt);
  };

  // extract arguments
  AqlValue const& argLhs =
      aql::functions::extractFunctionParameterValue(parameters, 0);
  AqlValue const& argRhs =
      aql::functions::extractFunctionParameterValue(parameters, 1);

  // check type of arguments
  if (!argLhs.isArray() || !argRhs.isArray() || argLhs.length() == 0 ||
      argRhs.length() == 0) {
    aql::functions::registerInvalidArgumentWarning(
        expressionContext, aql::functions::getFunctionName(node).data());
    return AqlValue(AqlValueHintNull());
  }

  auto lhsFirstElem = argLhs.slice().at(0);
  auto rhsFirstElem = argRhs.slice().at(0);

  // one of the args is matrix
  if (lhsFirstElem.isArray() || rhsFirstElem.isArray()) {
    decltype(lhsFirstElem) matrix;
    decltype(lhsFirstElem) array;

    if (lhsFirstElem.isArray()) {
      matrix = argLhs.slice();
      array = argRhs.slice();
    } else {
      matrix = argRhs.slice();
      array = argLhs.slice();
    }

    VPackBuilder builder;
    {
      VPackArrayBuilder arrayBuilder(&builder);
      for (VPackSlice currRow : VPackArrayIterator(matrix)) {
        if (!currRow.isArray()) {
          aql::functions::registerWarning(
              expressionContext, aql::functions::getFunctionName(node).data(),
              TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
          return AqlValue(AqlValueHintNull());
        }

        AqlValue dist = calculateDistance(currRow, array);
        if (!dist.isNumber()) {
          return AqlValue(AqlValueHintNull());
        }

        builder.add(dist.slice());
      }
    }
    // return array with values
    return AqlValue(std::move(*builder.steal()));

  } else {
    // calculate dist between 2 vectors and return number
    return calculateDistance(argLhs.slice(), argRhs.slice());
  }
}

}  // namespace

/// @brief function LEVENSHTEIN_DISTANCE
AqlValue functions::LevenshteinDistance(
    ExpressionContext* expr, AstNode const&,
    VPackFunctionParametersView parameters) {
  auto& trx = expr->trx();
  AqlValue const& value1 = extractFunctionParameterValue(parameters, 0);
  AqlValue const& value2 = extractFunctionParameterValue(parameters, 1);

  // we use one buffer to stringify both arguments
  transaction::StringLeaser buffer(&trx);
  velocypack::StringSink adapter(buffer.get());

  // stringify argument 1
  appendAsString(trx.vpackOptions(), adapter, value1);

  // note split position
  size_t const split = buffer->length();

  // stringify argument 2
  appendAsString(trx.vpackOptions(), adapter, value2);

  int encoded = basics::StringUtils::levenshteinDistance(
      buffer->data(), split, buffer->data() + split, buffer->length() - split);

  return AqlValue(AqlValueHintInt(encoded));
}

AqlValue functions::CosineSimilarity(aql::ExpressionContext* expressionContext,
                                     AstNode const& node,
                                     VPackFunctionParametersView parameters) {
  auto cosineSimilarityFunc = [expressionContext, &node](
                                  VPackArrayIterator lhsIt,
                                  VPackArrayIterator rhsIt) {
    double numerator{};
    double lhsSum{};
    double rhsSum{};

    TRI_ASSERT(lhsIt.size() == rhsIt.size());

    while (lhsIt.valid()) {
      auto lhsSlice = lhsIt.value();
      auto rhsSlice = rhsIt.value();
      if (!lhsSlice.isNumber() || !rhsSlice.isNumber()) {
        registerWarning(expressionContext, getFunctionName(node).data(),
                        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
        return AqlValue(AqlValueHintNull());
      }

      double lhsVal = lhsSlice.getNumber<double>();
      double rhsVal = rhsSlice.getNumber<double>();

      numerator += lhsVal * rhsVal;
      lhsSum += lhsVal * lhsVal;
      rhsSum += rhsVal * rhsVal;

      lhsIt.next();
      rhsIt.next();
    }

    double denominator = std::sqrt(lhsSum) * std::sqrt(rhsSum);
    if (denominator == 0.0) {
      registerWarning(expressionContext, getFunctionName(node).data(),
                      TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE);
      return AqlValue(AqlValueHintNull());
    }

    return numberValue(std::clamp(numerator / denominator, -1.0, 1.0), true);
  };

  return DistanceImpl(expressionContext, node, parameters,
                      cosineSimilarityFunc);
}

AqlValue functions::L1Distance(aql::ExpressionContext* expressionContext,
                               AstNode const& node,
                               VPackFunctionParametersView parameters) {
  auto L1DistFunc = [expressionContext, &node](VPackArrayIterator lhsIt,
                                               VPackArrayIterator rhsIt) {
    double dist{};
    TRI_ASSERT(lhsIt.size() == rhsIt.size());

    while (lhsIt.valid()) {
      auto lhsSlice = lhsIt.value();
      auto rhsSlice = rhsIt.value();
      if (!lhsSlice.isNumber() || !rhsSlice.isNumber()) {
        registerWarning(expressionContext, getFunctionName(node).data(),
                        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
        return AqlValue(AqlValueHintNull());
      }

      dist +=
          std::abs(lhsSlice.getNumber<double>() - rhsSlice.getNumber<double>());

      lhsIt.next();
      rhsIt.next();
    }

    return numberValue(dist, true);
  };

  return DistanceImpl(expressionContext, node, parameters, L1DistFunc);
}

AqlValue functions::L2Distance(aql::ExpressionContext* expressionContext,
                               AstNode const& node,
                               VPackFunctionParametersView parameters) {
  auto L2DistFunc = [expressionContext, &node](VPackArrayIterator lhsIt,
                                               VPackArrayIterator rhsIt) {
    double dist{};

    TRI_ASSERT(lhsIt.size() == rhsIt.size());

    while (lhsIt.valid()) {
      auto lhsSlice = lhsIt.value();
      auto rhsSlice = rhsIt.value();
      if (!lhsSlice.isNumber() || !rhsSlice.isNumber()) {
        registerWarning(expressionContext, getFunctionName(node).data(),
                        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
        return AqlValue(AqlValueHintNull());
      }

      double diff = lhsSlice.getNumber<double>() - rhsSlice.getNumber<double>();

      dist += std::pow(diff, 2);

      lhsIt.next();
      rhsIt.next();
    }

    return numberValue(std::sqrt(dist), true);
  };

  return DistanceImpl(expressionContext, node, parameters, L2DistFunc);
}

/// @brief function JACCARD
AqlValue functions::Jaccard(ExpressionContext* ctx, AstNode const&,
                            VPackFunctionParametersView args) {
  static char const* AFN = "JACCARD";

  using ValuesMap = std::unordered_map<VPackSlice, size_t,
                                       basics::VelocyPackHelper::VPackHash,
                                       basics::VelocyPackHelper::VPackEqual>;

  transaction::Methods* trx = &ctx->trx();
  auto* vopts = &trx->vpackOptions();
  ValuesMap values(512, basics::VelocyPackHelper::VPackHash(),
                   basics::VelocyPackHelper::VPackEqual(vopts));

  AqlValue const& lhs = aql::functions::extractFunctionParameterValue(args, 0);

  if (ADB_UNLIKELY(!lhs.isArray())) {
    // not an array
    registerWarning(ctx, AFN, TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(AqlValueHintNull());
  }

  AqlValue const& rhs = aql::functions::extractFunctionParameterValue(args, 1);

  if (ADB_UNLIKELY(!rhs.isArray())) {
    // not an array
    registerWarning(ctx, AFN, TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(AqlValueHintNull());
  }

  AqlValueMaterializer lhsMaterializer(vopts);
  AqlValueMaterializer rhsMaterializer(vopts);

  VPackSlice lhsSlice = lhsMaterializer.slice(lhs);
  VPackSlice rhsSlice = rhsMaterializer.slice(rhs);

  size_t cardinality = 0;  // cardinality of intersection

  for (VPackSlice slice : VPackArrayIterator(lhsSlice)) {
    values.try_emplace(slice, 1);
  }

  for (VPackSlice slice : VPackArrayIterator(rhsSlice)) {
    auto& count = values.try_emplace(slice, 0).first->second;
    cardinality += count;
    count = 0;
  }

  auto const jaccard =
      values.empty() ? 1.0 : double_t(cardinality) / values.size();

  return AqlValue{AqlValueHintDouble{jaccard}};
}

}  // namespace arangodb::aql
