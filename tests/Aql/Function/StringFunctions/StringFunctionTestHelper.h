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

#include <deque>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace arangodb::tests::aql {

using arangodb::aql::AqlValue;
using arangodb::aql::functions::VPackFunctionParametersView;

class StringFunctionTest : public ::testing::Test {
 protected:
  // argument values live as long as the test
  AqlValue const& json(std::string_view text) {
    return keep(velocypack::Parser::fromJson(text))[0];
  }

  AqlValue const& str(std::string_view text) {
    auto builder = std::make_shared<velocypack::Builder>();
    builder->add(velocypack::Value(std::string(text)));
    return keep(builder)[0];
  }

  VPackFunctionParametersView values(std::string_view jsonArray) {
    auto builder = velocypack::Parser::fromJson(jsonArray);
    _builders.push_back(builder);
    auto& values = _values.emplace_back();
    for (auto slice : velocypack::ArrayIterator(builder->slice())) {
      values.emplace_back(arangodb::aql::AqlValueHintSliceNoCopy{slice});
    }
    return values;
  }

  static void expectResult(ResultT<AqlValue> result,
                           std::string_view expected) {
    ASSERT_TRUE(result.ok()) << result.errorMessage();
    arangodb::aql::AqlValueGuard guard(*result, true);
    EXPECT_EQ(result->slice().stringView(), expected);
  }

  static void expectError(ResultT<AqlValue> result, ErrorCode expected) {
    if (result.ok()) {
      result->destroy();
    }
    EXPECT_EQ(result.errorNumber(), expected);
  }

  arangodb::aql::AqlFunctionsInternalCache cache;
  arangodb::aql::functions::StringFunctionContext ctx{
      velocypack::Options::Defaults,
      []() -> icu_64_64::Locale const& { return icu_64_64::Locale::getRoot(); },
      cache};

 private:
  std::vector<AqlValue> const& keep(
      std::shared_ptr<velocypack::Builder> builder) {
    _builders.push_back(builder);
    return _values.emplace_back(std::vector<AqlValue>{
        AqlValue(arangodb::aql::AqlValueHintSliceNoCopy{builder->slice()})});
  }

  std::deque<std::shared_ptr<velocypack::Builder>> _builders;
  std::deque<std::vector<AqlValue>> _values;
};

}  // namespace arangodb::tests::aql
