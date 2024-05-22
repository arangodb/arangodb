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
#include "Basics/Utf8Helper.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"

#include <velocypack/Builder.h>
#include <velocypack/Sink.h>
#include <velocypack/Slice.h>

#include <absl/strings/str_cat.h>

#include <unicode/schriter.h>
#include <unicode/stsearch.h>
#include <unicode/uchar.h>
#include <unicode/unistr.h>

#include <string_view>

using namespace arangodb;

namespace arangodb::aql {
namespace {

void registerICUWarning(ExpressionContext* expressionContext,
                        std::string_view functionName, UErrorCode status) {
  std::string msg = absl::StrCat("in function '", functionName, "()': ");
  msg.append(basics::Exception::FillExceptionString(TRI_ERROR_ARANGO_ICU_ERROR,
                                                    u_errorName_64_64(status)));
  expressionContext->registerWarning(TRI_ERROR_ARANGO_ICU_ERROR, msg);
}

}  // namespace

/// @brief function REGEX_MATCHES
AqlValue functions::RegexMatches(ExpressionContext* expressionContext,
                                 AstNode const&,
                                 VPackFunctionParametersView parameters) {
  static char const* AFN = "REGEX_MATCHES";

  transaction::Methods* trx = &expressionContext->trx();
  auto const& vopts = trx->vpackOptions();
  AqlValue const& aqlValueToMatch =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  if (parameters.size() == 1) {
    transaction::BuilderLeaser builder(trx);
    builder->openArray();
    builder->add(aqlValueToMatch.slice());
    builder->close();
    return AqlValue(builder->slice(), builder->size());
  }

  bool const caseInsensitive = getBooleanParameter(parameters, 2, false);

  // build pattern from parameter #1
  transaction::StringLeaser buffer(trx);
  velocypack::StringSink adapter(buffer.get());

  AqlValue const& regex =
      aql::functions::extractFunctionParameterValue(parameters, 1);
  appendAsString(vopts, adapter, regex);
  bool isEmptyExpression = (buffer->length() == 0);

  // the matcher is owned by the context!
  icu_64_64::RegexMatcher* matcher =
      expressionContext->buildRegexMatcher(*buffer, caseInsensitive);

  if (matcher == nullptr) {
    registerWarning(expressionContext, AFN, TRI_ERROR_QUERY_INVALID_REGEX);
    return AqlValue(AqlValueHintNull());
  }

  buffer->clear();
  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);
  appendAsString(vopts, adapter, value);
  icu_64_64::UnicodeString valueToMatch(
      buffer->data(), static_cast<uint32_t>(buffer->length()));

  transaction::BuilderLeaser result(trx);
  result->openArray();

  if (!isEmptyExpression && (buffer->length() == 0)) {
    // Edge case: splitting an empty string by non-empty expression produces an
    // empty string again.
    result->add(VPackValue(""));
    result->close();
    return AqlValue(result->slice(), result->size());
  }

  UErrorCode status = U_ZERO_ERROR;

  matcher->reset(valueToMatch);
  bool find = matcher->find();
  if (!find) {
    return AqlValue(AqlValueHintNull());
  }

  for (int i = 0; i <= matcher->groupCount(); i++) {
    icu_64_64::UnicodeString match = matcher->group(i, status);
    if (U_FAILURE(status)) {
      registerICUWarning(expressionContext, AFN, status);
      return AqlValue(AqlValueHintNull());
    } else {
      std::string s;
      match.toUTF8String(s);
      result->add(VPackValue(s));
    }
  }

  result->close();
  return AqlValue(result->slice(), result->size());
}

/// @brief function REGEX_SPLIT
AqlValue functions::RegexSplit(ExpressionContext* expressionContext,
                               AstNode const&,
                               VPackFunctionParametersView parameters) {
  static char const* AFN = "REGEX_SPLIT";

  transaction::Methods* trx = &expressionContext->trx();
  auto const& vopts = trx->vpackOptions();
  int64_t limitNumber = -1;
  if (parameters.size() == 4) {
    AqlValue const& aqlLimit =
        aql::functions::extractFunctionParameterValue(parameters, 3);
    if (aqlLimit.isNumber()) {
      limitNumber = aqlLimit.toInt64();
    } else {
      registerInvalidArgumentWarning(expressionContext, AFN);
      return AqlValue(AqlValueHintNull());
    }

    if (limitNumber < 0) {
      return AqlValue(AqlValueHintNull());
    }
    if (limitNumber == 0) {
      return AqlValue(AqlValueHintEmptyArray());
    }
  }

  AqlValue const& aqlValueToSplit =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  if (parameters.size() == 1) {
    // pre-documented edge-case: if we only have the first parameter, return it.
    transaction::BuilderLeaser builder(trx);
    builder->openArray();
    builder->add(aqlValueToSplit.slice());
    builder->close();
    return AqlValue(builder->slice(), builder->size());
  }

  bool const caseInsensitive = getBooleanParameter(parameters, 2, false);

  // build pattern from parameter #1
  transaction::StringLeaser buffer(trx);
  velocypack::StringSink adapter(buffer.get());

  AqlValue const& regex =
      aql::functions::extractFunctionParameterValue(parameters, 1);
  appendAsString(vopts, adapter, regex);
  bool isEmptyExpression = (buffer->length() == 0);

  // the matcher is owned by the context!
  icu_64_64::RegexMatcher* matcher =
      expressionContext->buildRegexMatcher(*buffer, caseInsensitive);

  if (matcher == nullptr) {
    registerWarning(expressionContext, AFN, TRI_ERROR_QUERY_INVALID_REGEX);
    return AqlValue(AqlValueHintNull());
  }

  buffer->clear();
  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);
  appendAsString(vopts, adapter, value);
  icu_64_64::UnicodeString valueToSplit(buffer->data(),
                                        static_cast<int32_t>(buffer->length()));

  transaction::BuilderLeaser result(trx);
  result->openArray();
  if (!isEmptyExpression && (buffer->length() == 0)) {
    // Edge case: splitting an empty string by non-empty expression produces an
    // empty string again.
    result->add(VPackValue(""));
    result->close();
    return AqlValue(result->slice(), result->size());
  }

  std::string utf8;
  static const uint16_t nrResults = 16;
  icu_64_64::UnicodeString uResults[nrResults];
  int64_t totalCount = 0;
  while (true) {
    UErrorCode errorCode = U_ZERO_ERROR;
    auto uCount = matcher->split(valueToSplit, uResults, nrResults, errorCode);
    uint16_t copyThisTime = uCount;

    if (U_FAILURE(errorCode)) {
      registerWarning(expressionContext, AFN, TRI_ERROR_QUERY_INVALID_REGEX);
      return AqlValue(AqlValueHintNull());
    }

    if ((copyThisTime > 0) && (copyThisTime > nrResults)) {
      // last hit is the remaining string to be fed into split in a subsequent
      // invocation
      copyThisTime--;
    }

    if ((copyThisTime > 0) &&
        ((copyThisTime == nrResults) || isEmptyExpression)) {
      // ICU will give us a traling empty string we don't care for if we split
      // with empty strings.
      copyThisTime--;
    }

    int64_t i = 0;
    while (i < copyThisTime && (limitNumber < 0 || totalCount < limitNumber)) {
      if ((i == 0) && isEmptyExpression) {
        // ICU will give us an empty string that we don't care for
        // as first value of one match-chunk
        i++;
        continue;
      }
      uResults[i].toUTF8String(utf8);
      result->add(VPackValue(utf8));
      utf8.clear();
      i++;
      totalCount++;
    }

    if (uCount != nrResults ||  // fetch any / found less then N
        (limitNumber >= 0 && totalCount >= limitNumber)) {  // fetch N
      break;
    }
    // ok, we have more to parse in the last result slot, reiterate with it:
    if (uCount == nrResults) {
      valueToSplit = uResults[nrResults - 1];
    } else {
      // should not go beyound the last match!
      TRI_ASSERT(false);
      break;
    }
  }

  result->close();
  return AqlValue(result->slice(), result->size());
}

/// @brief function REGEX_TEST
AqlValue functions::RegexTest(ExpressionContext* expressionContext,
                              AstNode const&,
                              VPackFunctionParametersView parameters) {
  static char const* AFN = "REGEX_TEST";

  transaction::Methods* trx = &expressionContext->trx();
  auto const& vopts = trx->vpackOptions();
  bool const caseInsensitive = getBooleanParameter(parameters, 2, false);
  transaction::StringLeaser buffer(trx);
  velocypack::StringSink adapter(buffer.get());

  // build pattern from parameter #1
  AqlValue const& regex =
      aql::functions::extractFunctionParameterValue(parameters, 1);
  appendAsString(vopts, adapter, regex);

  // the matcher is owned by the context!
  icu_64_64::RegexMatcher* matcher =
      expressionContext->buildRegexMatcher(*buffer, caseInsensitive);

  if (matcher == nullptr) {
    // compiling regular expression failed
    registerWarning(expressionContext, AFN, TRI_ERROR_QUERY_INVALID_REGEX);
    return AqlValue(AqlValueHintNull());
  }

  // extract value
  buffer->clear();
  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);
  appendAsString(vopts, adapter, value);

  bool error = false;
  bool const result = basics::Utf8Helper::DefaultUtf8Helper.matches(
      matcher, buffer->data(), buffer->length(), true, error);

  if (error) {
    // compiling regular expression failed
    registerWarning(expressionContext, AFN, TRI_ERROR_QUERY_INVALID_REGEX);
    return AqlValue(AqlValueHintNull());
  }

  return AqlValue(AqlValueHintBool(result));
}

/// @brief function REGEX_REPLACE
AqlValue functions::RegexReplace(ExpressionContext* expressionContext,
                                 AstNode const&,
                                 VPackFunctionParametersView parameters) {
  static char const* AFN = "REGEX_REPLACE";

  transaction::Methods* trx = &expressionContext->trx();
  auto const& vopts = trx->vpackOptions();
  bool const caseInsensitive = getBooleanParameter(parameters, 3, false);
  transaction::StringLeaser buffer(trx);
  velocypack::StringSink adapter(buffer.get());

  // build pattern from parameter #1
  AqlValue const& regex =
      aql::functions::extractFunctionParameterValue(parameters, 1);
  appendAsString(vopts, adapter, regex);

  // the matcher is owned by the context!
  icu_64_64::RegexMatcher* matcher =
      expressionContext->buildRegexMatcher(*buffer, caseInsensitive);

  if (matcher == nullptr) {
    // compiling regular expression failed
    registerWarning(expressionContext, AFN, TRI_ERROR_QUERY_INVALID_REGEX);
    return AqlValue(AqlValueHintNull());
  }

  // extract value
  buffer->clear();
  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);
  appendAsString(vopts, adapter, value);

  size_t const split = buffer->length();
  AqlValue const& replace =
      aql::functions::extractFunctionParameterValue(parameters, 2);
  appendAsString(vopts, adapter, replace);

  bool error = false;
  std::string result = basics::Utf8Helper::DefaultUtf8Helper.replace(
      matcher, buffer->data(), split, buffer->data() + split,
      buffer->length() - split, false, error);

  if (error) {
    // compiling regular expression failed
    registerWarning(expressionContext, AFN, TRI_ERROR_QUERY_INVALID_REGEX);
    return AqlValue(AqlValueHintNull());
  }

  return AqlValue(result);
}

}  // namespace arangodb::aql
