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
#include <cstddef>
#include <string_view>

namespace arangodb::aql {
class AqlFunctionsInternalCache;

namespace functions {

struct StringFunctionEnv {
  // the transaction's options resolve custom types (e.g. _id) and translated
  // attribute names in place, without copying the document first
  velocypack::Options const& vopts;
  icu_64_64::Locale const& locale;
  AqlFunctionsInternalCache& cache;
};

using PureStringFunction = ResultT<AqlValue> (*)(
    VPackFunctionParametersView parameters, StringFunctionEnv const& env);

// registers a failed result as a warning and returns null
AqlValue callPure(ExpressionContext* ctx, std::string_view functionName,
                  PureStringFunction fn,
                  VPackFunctionParametersView parameters);

// lets a string literal be passed as a template argument
template<std::size_t N>
struct FunctionName {
  constexpr FunctionName(char const (&name)[N]) { std::copy_n(name, N, value); }
  char value[N];
};

// explicit name: under CALL/APPLY the node is the CALL/APPLY node
template<PureStringFunction F, FunctionName Name>
AqlValue adapt(ExpressionContext* ctx, AstNode const&,
               VPackFunctionParametersView parameters) {
  return callPure(ctx, {Name.value, sizeof(Name.value) - 1}, F, parameters);
}

ResultT<AqlValue> toString(VPackFunctionParametersView parameters,
                           StringFunctionEnv const& env);
ResultT<AqlValue> toChar(VPackFunctionParametersView parameters,
                         StringFunctionEnv const& env);
ResultT<AqlValue> repeat(VPackFunctionParametersView parameters,
                         StringFunctionEnv const& env);
ResultT<AqlValue> findFirst(VPackFunctionParametersView parameters,
                            StringFunctionEnv const& env);
ResultT<AqlValue> findLast(VPackFunctionParametersView parameters,
                           StringFunctionEnv const& env);
ResultT<AqlValue> concatSeparator(VPackFunctionParametersView parameters,
                                  StringFunctionEnv const& env);
ResultT<AqlValue> charLength(VPackFunctionParametersView parameters,
                             StringFunctionEnv const& env);
ResultT<AqlValue> lower(VPackFunctionParametersView parameters,
                        StringFunctionEnv const& env);
ResultT<AqlValue> upper(VPackFunctionParametersView parameters,
                        StringFunctionEnv const& env);
ResultT<AqlValue> substring(VPackFunctionParametersView parameters,
                            StringFunctionEnv const& env);
ResultT<AqlValue> substringBytes(VPackFunctionParametersView parameters,
                                 StringFunctionEnv const& env);
ResultT<AqlValue> substitute(VPackFunctionParametersView parameters,
                             StringFunctionEnv const& env);
ResultT<AqlValue> left(VPackFunctionParametersView parameters,
                       StringFunctionEnv const& env);
ResultT<AqlValue> right(VPackFunctionParametersView parameters,
                        StringFunctionEnv const& env);
ResultT<AqlValue> trim(VPackFunctionParametersView parameters,
                       StringFunctionEnv const& env);
ResultT<AqlValue> ltrim(VPackFunctionParametersView parameters,
                        StringFunctionEnv const& env);
ResultT<AqlValue> rtrim(VPackFunctionParametersView parameters,
                        StringFunctionEnv const& env);
ResultT<AqlValue> contains(VPackFunctionParametersView parameters,
                           StringFunctionEnv const& env);
ResultT<AqlValue> concat(VPackFunctionParametersView parameters,
                         StringFunctionEnv const& env);
ResultT<AqlValue> like(VPackFunctionParametersView parameters,
                       StringFunctionEnv const& env);
ResultT<AqlValue> split(VPackFunctionParametersView parameters,
                        StringFunctionEnv const& env);

}  // namespace functions
}  // namespace arangodb::aql
