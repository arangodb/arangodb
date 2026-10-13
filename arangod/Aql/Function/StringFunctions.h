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

#include <unicode/locid.h>

#include <algorithm>
#include <concepts>
#include <string_view>
#include <utility>

namespace arangodb {
struct ICancellationToken;
}

namespace arangodb::aql {
class AqlFunctionsInternalCache;

namespace functions {

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

// the AQL name each function reports its warnings under
template<auto Fn>
inline constexpr std::string_view kStringFunctionName = {};

// optional arguments are nullptr when not passed
ResultT<AqlValue> toString(StringFunctionContext const& ctx,
                           AqlValue const& value);
template<>
inline constexpr std::string_view kStringFunctionName<toString> = "TO_STRING";

ResultT<AqlValue> toChar(StringFunctionContext const& ctx,
                         AqlValue const& codePoint);
template<>
inline constexpr std::string_view kStringFunctionName<toChar> = "TO_CHAR";

ResultT<AqlValue> repeat(StringFunctionContext const& ctx,
                         AqlValue const& value, AqlValue const& repetitions,
                         AqlValue const* separator);
template<>
inline constexpr std::string_view kStringFunctionName<repeat> = "REPEAT";

ResultT<AqlValue> findFirst(StringFunctionContext const& ctx,
                            AqlValue const& text, AqlValue const& search,
                            AqlValue const* start, AqlValue const* end);
template<>
inline constexpr std::string_view kStringFunctionName<findFirst> = "FIND_FIRST";

ResultT<AqlValue> findLast(StringFunctionContext const& ctx,
                           AqlValue const& text, AqlValue const& search,
                           AqlValue const* start, AqlValue const* end);
template<>
inline constexpr std::string_view kStringFunctionName<findLast> = "FIND_LAST";

ResultT<AqlValue> concatSeparator(StringFunctionContext const& ctx,
                                  AqlValue const& separator,
                                  VPackFunctionParametersView values);
template<>
inline constexpr std::string_view kStringFunctionName<concatSeparator> =
    "CONCAT_SEPARATOR";

ResultT<AqlValue> charLength(StringFunctionContext const& ctx,
                             AqlValue const& value);
template<>
inline constexpr std::string_view kStringFunctionName<charLength> =
    "CHAR_LENGTH";

ResultT<AqlValue> lower(StringFunctionContext const& ctx,
                        AqlValue const& value);
template<>
inline constexpr std::string_view kStringFunctionName<lower> = "LOWER";

ResultT<AqlValue> upper(StringFunctionContext const& ctx,
                        AqlValue const& value);
template<>
inline constexpr std::string_view kStringFunctionName<upper> = "UPPER";

ResultT<AqlValue> substring(StringFunctionContext const& ctx,
                            AqlValue const& value, AqlValue const& offset,
                            AqlValue const* length);
template<>
inline constexpr std::string_view kStringFunctionName<substring> = "SUBSTRING";

ResultT<AqlValue> substringBytes(StringFunctionContext const& ctx,
                                 AqlValue const& value, AqlValue const& offset,
                                 AqlValue const* length, AqlValue const* left,
                                 AqlValue const* right);
template<>
inline constexpr std::string_view kStringFunctionName<substringBytes> =
    "SUBSTRING_BYTES";

ResultT<AqlValue> left(StringFunctionContext const& ctx, AqlValue const& value,
                       AqlValue const& length);
template<>
inline constexpr std::string_view kStringFunctionName<left> = "LEFT";

ResultT<AqlValue> right(StringFunctionContext const& ctx, AqlValue const& value,
                        AqlValue const& length);
template<>
inline constexpr std::string_view kStringFunctionName<right> = "RIGHT";

ResultT<AqlValue> trim(StringFunctionContext const& ctx, AqlValue const& value,
                       AqlValue const* typeOrChars);
template<>
inline constexpr std::string_view kStringFunctionName<trim> = "TRIM";

ResultT<AqlValue> ltrim(StringFunctionContext const& ctx, AqlValue const& value,
                        AqlValue const* chars);
template<>
inline constexpr std::string_view kStringFunctionName<ltrim> = "LTRIM";

ResultT<AqlValue> rtrim(StringFunctionContext const& ctx, AqlValue const& value,
                        AqlValue const* chars);
template<>
inline constexpr std::string_view kStringFunctionName<rtrim> = "RTRIM";

ResultT<AqlValue> concat(StringFunctionContext const& ctx,
                         VPackFunctionParametersView values);
template<>
inline constexpr std::string_view kStringFunctionName<concat> = "CONCAT";

ResultT<AqlValue> contains(StringFunctionContext const& ctx,
                           AqlValue const& text, AqlValue const& search,
                           AqlValue const* returnIndex);
template<>
inline constexpr std::string_view kStringFunctionName<contains> = "CONTAINS";

ResultT<AqlValue> like(StringFunctionContext const& ctx, AqlValue const& text,
                       AqlValue const& pattern,
                       AqlValue const* caseInsensitive);
template<>
inline constexpr std::string_view kStringFunctionName<like> = "LIKE";

ResultT<AqlValue> split(StringFunctionContext const& ctx, AqlValue const& value,
                        AqlValue const* separator, AqlValue const* limit);
template<>
inline constexpr std::string_view kStringFunctionName<split> = "SPLIT";

ResultT<AqlValue> substitute(StringFunctionContext const& ctx,
                             AqlValue const& value, AqlValue const& search,
                             AqlValue const* replace, AqlValue const* limit);
template<>
inline constexpr std::string_view kStringFunctionName<substitute> =
    "SUBSTITUTE";

StringFunctionContext makeStringFunctionContext(ExpressionContext* ctx);

// registers a failed result as a warning and returns null
AqlValue toAqlResult(ExpressionContext* ctx, std::string_view name,
                     ResultT<AqlValue>&& result);

namespace detail {

template<typename T>
T stringFunctionArgument(VPackFunctionParametersView parameters, size_t index) {
  if constexpr (std::same_as<T, AqlValue const&>) {
    return extractFunctionParameterValue(parameters, index);
  } else if constexpr (std::same_as<T, AqlValue const*>) {
    return index < parameters.size() ? &parameters[index] : nullptr;
  } else {
    static_assert(std::same_as<T, VPackFunctionParametersView>);
    return parameters.subspan(std::min(index, parameters.size()));
  }
}

template<typename... Args>
ResultT<AqlValue> invokeStringFunction(
    ResultT<AqlValue> (*fn)(StringFunctionContext const&, Args...),
    StringFunctionContext const& ctx, VPackFunctionParametersView parameters) {
  return [&]<size_t... I>(std::index_sequence<I...>) {
    return fn(ctx, stringFunctionArgument<Args>(parameters, I)...);
  }
  (std::index_sequence_for<Args...>{});
}

}  // namespace detail

// explicit name: under CALL/APPLY the node is the CALL/APPLY node
template<auto Fn>
AqlValue adapt(ExpressionContext* ctx, AstNode const&,
               VPackFunctionParametersView parameters) {
  static_assert(!kStringFunctionName<Fn>.empty(),
                "string function has no kStringFunctionName");
  return toAqlResult(ctx, kStringFunctionName<Fn>,
                     detail::invokeStringFunction(
                         Fn, makeStringFunctionContext(ctx), parameters));
}

}  // namespace functions
}  // namespace arangodb::aql
