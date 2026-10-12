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

#include "gtest/gtest.h"

#include "Aql/AqlFunctionsInternalCache.h"
#include "Aql/AqlValue.h"
#include "Aql/Function/StringFunctions.h"
#include "Basics/ResultT.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>

#include <unicode/locid.h>

#include <string>
#include <string_view>
#include <vector>

namespace arangodb::tests::aql {

using arangodb::aql::functions::StringFunctionsName;

inline ResultT<std::string> call(StringFunctionsName name,
                                 velocypack::Slice argsSlice) {
  std::vector<arangodb::aql::AqlValue> args;
  for (auto slice : velocypack::ArrayIterator(argsSlice)) {
    args.emplace_back(arangodb::aql::AqlValueHintSliceNoCopy{slice});
  }
  arangodb::aql::AqlFunctionsInternalCache cache;
  arangodb::aql::functions::StringFunctionContext env{
      velocypack::Options::Defaults, icu_64_64::Locale::getRoot(), cache};

  auto result = arangodb::aql::functions::runStringFunction(name, args, env);
  if (result.fail()) {
    return result.result();
  }
  arangodb::aql::AqlValueGuard guard(*result, true);
  return result->slice().copyString();
}

inline ResultT<std::string> call(StringFunctionsName name,
                                 std::string_view argsJson) {
  auto builder = velocypack::Parser::fromJson(argsJson);
  return call(name, builder->slice());
}

inline void expectResult(StringFunctionsName name, std::string_view argsJson,
                         std::string_view expected) {
  SCOPED_TRACE(argsJson);
  auto result = call(name, argsJson);
  ASSERT_TRUE(result.ok()) << result.errorMessage();
  EXPECT_EQ(*result, expected);
}

inline void expectError(StringFunctionsName name, std::string_view argsJson,
                        ErrorCode expected) {
  SCOPED_TRACE(argsJson);
  EXPECT_EQ(call(name, argsJson).errorNumber(), expected);
}

}  // namespace arangodb::tests::aql
