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

using namespace arangodb;

namespace arangodb::aql {
namespace {
template<typename F>
AqlValue decayFuncImpl(aql::ExpressionContext* expressionContext,
                       AstNode const& node,
                       aql::functions::VPackFunctionParametersView parameters,
                       F&& decayFuncFactory) {
  AqlValue const& argValue =
      aql::functions::extractFunctionParameterValue(parameters, 0);
  AqlValue const& originValue =
      aql::functions::extractFunctionParameterValue(parameters, 1);
  AqlValue const& scaleValue =
      aql::functions::extractFunctionParameterValue(parameters, 2);
  AqlValue const& offsetValue =
      aql::functions::extractFunctionParameterValue(parameters, 3);
  AqlValue const& decayValue =
      aql::functions::extractFunctionParameterValue(parameters, 4);

  // check type of arguments
  if ((!argValue.isRange() && !argValue.isArray() && !argValue.isNumber()) ||
      !originValue.isNumber() || !scaleValue.isNumber() ||
      !offsetValue.isNumber() || !decayValue.isNumber()) {
    aql::functions::registerInvalidArgumentWarning(
        expressionContext, aql::functions::getFunctionName(node).data());
    return AqlValue(AqlValueHintNull());
  }

  // extracting values
  bool failed;
  bool error = false;
  double origin = originValue.toDouble(failed);
  error |= failed;
  double scale = scaleValue.toDouble(failed);
  error |= failed;
  double offset = offsetValue.toDouble(failed);
  error |= failed;
  double decay = decayValue.toDouble(failed);
  error |= failed;

  if (error) {
    aql::functions::registerWarning(
        expressionContext, aql::functions::getFunctionName(node).data(),
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(AqlValueHintNull());
  }

  // check that parameters are correct
  if (scale <= 0 || offset < 0 || decay <= 0 || decay >= 1) {
    aql::functions::registerWarning(
        expressionContext, aql::functions::getFunctionName(node).data(),
        TRI_ERROR_QUERY_NUMBER_OUT_OF_RANGE);
    return AqlValue(AqlValueHintNull());
  }

  // get lambda for further calculation
  auto decayFunc = decayFuncFactory(origin, scale, offset, decay);

  // argument is number
  if (argValue.isNumber()) {
    double arg = argValue.slice().getNumber<double>();
    double funcRes = decayFunc(arg);
    return aql::functions::numberValue(funcRes, true);
  } else {
    // argument is array or range
    auto* trx = &expressionContext->trx();
    AqlValueMaterializer materializer(&trx->vpackOptions());
    VPackSlice slice = materializer.slice(argValue);
    TRI_ASSERT(slice.isArray());

    VPackBuilder builder;
    {
      VPackArrayBuilder arrayBuilder(&builder);
      for (VPackSlice currArg : VPackArrayIterator(slice)) {
        if (!currArg.isNumber()) {
          aql::functions::registerWarning(
              expressionContext, aql::functions::getFunctionName(node).data(),
              TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
          return AqlValue(AqlValueHintNull());
        }
        double arg = currArg.getNumber<double>();
        double funcRes = decayFunc(arg);
        builder.add(VPackValue(funcRes));
      }
    }

    return AqlValue(std::move(*builder.steal()));
  }
}

}  // namespace

AqlValue functions::DecayGauss(aql::ExpressionContext* expressionContext,
                               AstNode const& node,
                               VPackFunctionParametersView parameters) {
  auto gaussDecayFactory = [](const double origin, const double scale,
                              const double offset, const double decay) {
    const double sigmaSqr = -(scale * scale) / (2 * std::log(decay));
    return [=](double arg) {
      double max = std::max(0.0, std::fabs(arg - origin) - offset);
      double numerator = max * max;
      double val = std::exp(-numerator / (2 * sigmaSqr));
      return val;
    };
  };

  return decayFuncImpl(expressionContext, node, parameters, gaussDecayFactory);
}

AqlValue functions::DecayExp(aql::ExpressionContext* expressionContext,
                             AstNode const& node,
                             VPackFunctionParametersView parameters) {
  auto expDecayFactory = [](const double origin, const double scale,
                            const double offset, const double decay) {
    const double lambda = std::log(decay) / scale;
    return [=](double arg) {
      double numerator =
          lambda * std::max(0.0, std::abs(arg - origin) - offset);
      double val = std::exp(numerator);
      return val;
    };
  };

  return decayFuncImpl(expressionContext, node, parameters, expDecayFactory);
}

AqlValue functions::DecayLinear(aql::ExpressionContext* expressionContext,
                                AstNode const& node,
                                VPackFunctionParametersView parameters) {
  auto linearDecayFactory = [](const double origin, const double scale,
                               const double offset, const double decay) {
    const double s = scale / (1.0 - decay);
    return [=](double arg) {
      double max = std::max(0.0, std::fabs(arg - origin) - offset);
      double val = std::max((s - max) / s, 0.0);
      return val;
    };
  };

  return decayFuncImpl(expressionContext, node, parameters, linearDecayFactory);
}

}  // namespace arangodb::aql
