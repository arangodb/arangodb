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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/AqlValue.h"
#include "Aql/Functions.h"
#include "Basics/ResultT.h"
#include "Basics/system-compiler.h"

#include <unicode/locid.h>

#include <string_view>

namespace arangodb {
struct ICancellationToken;
}

namespace arangodb::aql {
class AqlFunctionsInternalCache;

namespace functions {

enum class StringFunctionsName {
  kSubstitute,
  kToString,
  kToChar,
  kRepeat,
  kFindFirst,
  kFindLast,
  kConcatSeparator,
  kCharLength,
  kLower,
  kUpper,
  kSubstring,
  kSubstringBytes,
  kLeft,
  kRight,
  kTrim,
  kLTrim,
  kRTrim,
  kConcat,
  kContains,
  kLike,
  kSplit,
};

constexpr std::string_view functionName(StringFunctionsName name) noexcept {
  switch (name) {
    case StringFunctionsName::kSubstitute:
      return "SUBSTITUTE";
    case StringFunctionsName::kToString:
      return "TO_STRING";
    case StringFunctionsName::kToChar:
      return "TO_CHAR";
    case StringFunctionsName::kRepeat:
      return "REPEAT";
    case StringFunctionsName::kFindFirst:
      return "FIND_FIRST";
    case StringFunctionsName::kFindLast:
      return "FIND_LAST";
    case StringFunctionsName::kConcatSeparator:
      return "CONCAT_SEPARATOR";
    case StringFunctionsName::kCharLength:
      return "CHAR_LENGTH";
    case StringFunctionsName::kLower:
      return "LOWER";
    case StringFunctionsName::kUpper:
      return "UPPER";
    case StringFunctionsName::kSubstring:
      return "SUBSTRING";
    case StringFunctionsName::kSubstringBytes:
      return "SUBSTRING_BYTES";
    case StringFunctionsName::kLeft:
      return "LEFT";
    case StringFunctionsName::kRight:
      return "RIGHT";
    case StringFunctionsName::kTrim:
      return "TRIM";
    case StringFunctionsName::kLTrim:
      return "LTRIM";
    case StringFunctionsName::kRTrim:
      return "RTRIM";
    case StringFunctionsName::kConcat:
      return "CONCAT";
    case StringFunctionsName::kContains:
      return "CONTAINS";
    case StringFunctionsName::kLike:
      return "LIKE";
    case StringFunctionsName::kSplit:
      return "SPLIT";
  }
  ADB_UNREACHABLE;
}

struct StringFunctionContext {
  // the transaction's options resolve custom types (e.g. _id) and translated
  // attribute names in place, without copying the document first
  velocypack::Options const& vopts;
  icu_64_64::Locale const& locale;
  // like and split string function save their built regexes into a cache
  AqlFunctionsInternalCache& cache;
  // lets long regex matches stop when the query is killed
  ICancellationToken const* cancellationToken = nullptr;
};

ResultT<AqlValue> runStringFunction(StringFunctionsName name,
                                    VPackFunctionParametersView parameters,
                                    StringFunctionContext const& env);

// registers a failed result as a warning and returns null
AqlValue callStringFunction(ExpressionContext* ctx, StringFunctionsName name,
                            VPackFunctionParametersView parameters);

// explicit name: under CALL/APPLY the node is the CALL/APPLY node
template<StringFunctionsName Name>
AqlValue adapt(ExpressionContext* ctx, AstNode const&,
               VPackFunctionParametersView parameters) {
  return callStringFunction(ctx, Name, parameters);
}

}  // namespace functions
}  // namespace arangodb::aql
