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

namespace arangodb::aql {
class AqlFunctionsInternalCache;

namespace functions {

struct StringFunctionEnv {
  icu_64_64::Locale const& locale;
  AqlFunctionsInternalCache& cache;
};

// parameters have custom types resolved to strings
using PureStringFunction = ResultT<AqlValue> (*)(
    VPackFunctionParametersView parameters, StringFunctionEnv& env);

// registers a failed result as a warning and returns null
AqlValue callPure(ExpressionContext* ctx, AstNode const& node,
                  PureStringFunction fn,
                  VPackFunctionParametersView parameters);

template<PureStringFunction F>
AqlValue adapt(ExpressionContext* ctx, AstNode const& node,
               VPackFunctionParametersView parameters) {
  return callPure(ctx, node, F, parameters);
}

ResultT<AqlValue> toString(VPackFunctionParametersView parameters,
                           StringFunctionEnv& env);
ResultT<AqlValue> toChar(VPackFunctionParametersView parameters,
                         StringFunctionEnv& env);
ResultT<AqlValue> repeat(VPackFunctionParametersView parameters,
                         StringFunctionEnv& env);
ResultT<AqlValue> findFirst(VPackFunctionParametersView parameters,
                            StringFunctionEnv& env);
ResultT<AqlValue> findLast(VPackFunctionParametersView parameters,
                           StringFunctionEnv& env);
ResultT<AqlValue> concatSeparator(VPackFunctionParametersView parameters,
                                  StringFunctionEnv& env);
ResultT<AqlValue> charLength(VPackFunctionParametersView parameters,
                             StringFunctionEnv& env);
ResultT<AqlValue> lower(VPackFunctionParametersView parameters,
                        StringFunctionEnv& env);
ResultT<AqlValue> upper(VPackFunctionParametersView parameters,
                        StringFunctionEnv& env);
ResultT<AqlValue> substring(VPackFunctionParametersView parameters,
                            StringFunctionEnv& env);
ResultT<AqlValue> substringBytes(VPackFunctionParametersView parameters,
                                 StringFunctionEnv& env);
ResultT<AqlValue> substitute(VPackFunctionParametersView parameters,
                             StringFunctionEnv& env);
ResultT<AqlValue> left(VPackFunctionParametersView parameters,
                       StringFunctionEnv& env);
ResultT<AqlValue> right(VPackFunctionParametersView parameters,
                        StringFunctionEnv& env);
ResultT<AqlValue> trim(VPackFunctionParametersView parameters,
                       StringFunctionEnv& env);
ResultT<AqlValue> ltrim(VPackFunctionParametersView parameters,
                        StringFunctionEnv& env);
ResultT<AqlValue> rtrim(VPackFunctionParametersView parameters,
                        StringFunctionEnv& env);
ResultT<AqlValue> contains(VPackFunctionParametersView parameters,
                           StringFunctionEnv& env);
ResultT<AqlValue> concat(VPackFunctionParametersView parameters,
                         StringFunctionEnv& env);
ResultT<AqlValue> like(VPackFunctionParametersView parameters,
                       StringFunctionEnv& env);
ResultT<AqlValue> split(VPackFunctionParametersView parameters,
                        StringFunctionEnv& env);

}  // namespace functions
}  // namespace arangodb::aql
