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
#include "Basics/Result.h"
#include "Basics/fpconv.h"
#include "Basics/tri-strings.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Sink.h>
#include <velocypack/Slice.h>

#include <unicode/schriter.h>
#include <unicode/unistr.h>

using namespace arangodb;

namespace arangodb::aql {

/// @brief function TO_NUMBER
AqlValue functions::ToNumber(ExpressionContext*, AstNode const&,
                             VPackFunctionParametersView parameters) {
  AqlValue const& a = extractFunctionParameterValue(parameters, 0);
  bool failed;
  double value = a.toDouble(failed);

  if (failed) {
    value = 0.0;
  }

  return AqlValue(AqlValueHintDouble(value));
}

/// @brief function IN_RANGE
AqlValue functions::InRange(ExpressionContext* ctx, AstNode const&,
                            VPackFunctionParametersView args) {
  static char const* AFN = "IN_RANGE";

  auto const argc = args.size();

  if (argc != 5) {
    aql::functions::registerWarning(
        ctx, AFN,
        Result{TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH,
               "5 arguments are expected."});
    return AqlValue(AqlValueHintNull());
  }

  auto* vopts = &ctx->trx().vpackOptions();
  auto const& attributeVal =
      aql::functions::extractFunctionParameterValue(args, 0);
  auto const& lowerVal = aql::functions::extractFunctionParameterValue(args, 1);
  auto const& upperVal = aql::functions::extractFunctionParameterValue(args, 2);
  auto const& includeLowerVal =
      aql::functions::extractFunctionParameterValue(args, 3);
  auto const& includeUpperVal =
      aql::functions::extractFunctionParameterValue(args, 4);

  if (ADB_UNLIKELY(!includeLowerVal.isBoolean())) {
    registerInvalidArgumentWarning(ctx, AFN);
    return aql::AqlValue{aql::AqlValueHintNull{}};
  }

  if (ADB_UNLIKELY(!includeUpperVal.isBoolean())) {
    registerInvalidArgumentWarning(ctx, AFN);
    return aql::AqlValue{aql::AqlValueHintNull{}};
  }

  auto const includeLower = includeLowerVal.toBoolean();
  auto const includeUpper = includeUpperVal.toBoolean();

  // first check lower bound
  {
    auto const compareLowerResult =
        AqlValue::Compare(vopts, lowerVal, attributeVal, true);
    if ((!includeLower && compareLowerResult >= 0) ||
        (includeLower && compareLowerResult > 0)) {
      return AqlValue(AqlValueHintBool(false));
    }
  }

  // lower bound is fine, check upper
  auto const compareUpperResult =
      AqlValue::Compare(vopts, attributeVal, upperVal, true);
  return AqlValue(AqlValueHintBool((includeUpper && compareUpperResult <= 0) ||
                                   (!includeUpper && compareUpperResult < 0)));
}

/// @brief function TO_BOOL
AqlValue functions::ToBool(ExpressionContext*, AstNode const&,
                           VPackFunctionParametersView parameters) {
  AqlValue const& a =
      aql::functions::extractFunctionParameterValue(parameters, 0);
  return AqlValue(AqlValueHintBool(a.toBoolean()));
}

/// @brief function LENGTH
AqlValue functions::Length(ExpressionContext*, AstNode const&,
                           VPackFunctionParametersView parameters) {
  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);
  if (value.isArray()) {
    // shortcut!
    return AqlValue(AqlValueHintUInt(value.length()));
  }

  size_t length = 0;
  if (value.isNull(true)) {
    length = 0;
  } else if (value.isBoolean()) {
    if (value.toBoolean()) {
      length = 1;
    } else {
      length = 0;
    }
  } else if (value.isNumber()) {
    double tmp = value.toDouble();
    if (std::isnan(tmp) || !std::isfinite(tmp)) {
      length = 0;
    } else {
      char buffer[24];
      length = static_cast<size_t>(fpconv_dtoa(tmp, buffer));
    }
  } else if (value.isString()) {
    VPackValueLength l;
    char const* p = value.slice().getStringUnchecked(l);
    length = TRI_CharLengthUtf8String(p, l);
  } else if (value.isObject()) {
    length = static_cast<size_t>(value.length());
  }

  return AqlValue(AqlValueHintUInt(length));
}

/// @brief function REVERSE
AqlValue functions::Reverse(ExpressionContext* expressionContext,
                            AstNode const&,
                            VPackFunctionParametersView parameters) {
  static char const* AFN = "REVERSE";

  transaction::Methods* trx = &expressionContext->trx();
  auto const& vopts = trx->vpackOptions();
  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  if (value.isArray()) {
    transaction::BuilderLeaser builder(trx);
    AqlValueMaterializer materializer(&vopts);
    VPackSlice slice = materializer.slice(value);
    std::vector<VPackSlice> array;
    array.reserve(slice.length());
    for (VPackSlice it : VPackArrayIterator(slice)) {
      array.push_back(it);
    }
    std::reverse(std::begin(array), std::end(array));

    builder->openArray();
    for (auto const& it : array) {
      builder->add(it);
    }
    builder->close();
    return AqlValue(builder->slice(), builder->size());
  } else if (value.isString()) {
    std::string utf8;
    transaction::StringLeaser buf1(trx);
    velocypack::StringSink adapter(buf1.get());
    appendAsString(vopts, adapter, value);
    icu_64_64::UnicodeString uBuf(buf1->data(),
                                  static_cast<int32_t>(buf1->length()));
    // reserve the result buffer, but need to set empty afterwards:
    icu_64_64::UnicodeString result;
    result.getBuffer(uBuf.length());
    result = "";
    icu_64_64::StringCharacterIterator iter(uBuf, uBuf.length());
    UChar c = iter.previous();
    while (c != icu_64_64::CharacterIterator::DONE) {
      result.append(c);
      c = iter.previous();
    }
    result.toUTF8String(utf8);

    return AqlValue(utf8);
  } else {
    // neither array nor string...
    aql::functions::registerWarning(expressionContext, AFN,
                                    TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(AqlValueHintNull());
  }
}

/// @brief function FIRST_DOCUMENT
AqlValue functions::FirstDocument(ExpressionContext*, AstNode const&,
                                  VPackFunctionParametersView parameters) {
  size_t const n = parameters.size();
  for (size_t i = 0; i < n; ++i) {
    AqlValue const& a =
        aql::functions::extractFunctionParameterValue(parameters, i);
    if (a.isObject()) {
      return a.clone();
    }
  }

  return AqlValue(AqlValueHintNull());
}

/// @brief function FIRST_LIST
AqlValue functions::FirstList(ExpressionContext*, AstNode const&,
                              VPackFunctionParametersView parameters) {
  size_t const n = parameters.size();
  for (size_t i = 0; i < n; ++i) {
    AqlValue const& a =
        aql::functions::extractFunctionParameterValue(parameters, i);
    if (a.isArray()) {
      return a.clone();
    }
  }

  return AqlValue(AqlValueHintNull());
}

/// @brief function NOT_NULL
AqlValue functions::NotNull(ExpressionContext*, AstNode const&,
                            VPackFunctionParametersView parameters) {
  size_t const n = parameters.size();
  for (size_t i = 0; i < n; ++i) {
    AqlValue const& element =
        aql::functions::extractFunctionParameterValue(parameters, i);
    if (!element.isNull(true)) {
      return element.clone();
    }
  }
  return AqlValue(AqlValueHintNull());
}
}  // namespace arangodb::aql
