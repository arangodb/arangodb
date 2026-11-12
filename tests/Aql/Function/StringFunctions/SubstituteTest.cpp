////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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

#include "gtest/gtest.h"

#include "fakeit.hpp"

#include "Aql/AqlValue.h"
#include "Aql/ExpressionContext.h"
#include "Aql/Function/AqlFunction.h"
#include "Aql/Function/FunctionContext.h"
#include "Aql/Function/StringFunctions.h"

#include <velocypack/Builder.h>
#include <velocypack/Options.h>
#include <velocypack/Parser.h>

#include <unicode/locid.h>

#include <memory>
#include <string_view>
#include <vector>

using namespace arangodb;
using namespace arangodb::aql;

namespace {

class SubstituteTest : public ::testing::Test {
 protected:
  auto substitute(std::vector<std::string_view> const& jsonArgs)
      -> ResultT<AqlValue> {
    return functions::invoke<functions::Substitute>(context(),
                                                    toArgs(jsonArgs));
  }

  auto parse(std::vector<std::string_view> const& jsonArgs)
      -> ResultT<functions::Substitute::Arguments> {
    return functions::Substitute::parseArguments(context(), toArgs(jsonArgs));
  }

  void expectResult(std::vector<std::string_view> const& jsonArgs,
                    std::string_view expected) {
    auto result = substitute(jsonArgs);
    ASSERT_TRUE(result.ok()) << result.errorMessage();
    AqlValueGuard guard{*result, true};
    ASSERT_TRUE(result->isString());
    EXPECT_EQ(result->slice().stringView(), expected);
  }

  void expectError(std::vector<std::string_view> const& jsonArgs,
                   ErrorCode expected) {
    auto result = substitute(jsonArgs);
    EXPECT_EQ(result.errorNumber(), expected);
  }

 private:
  auto context() -> functions::FunctionContext {
    return functions::FunctionContext{
        .vpackOptions = velocypack::Options::Defaults,
        .locale = _locale,
        .expressionContext = _expressionContextMock.get()};
  }

  auto toArgs(std::vector<std::string_view> const& jsonArgs)
      -> std::span<AqlValue const> {
    _builders.clear();
    _args.clear();
    for (auto json : jsonArgs) {
      _builders.push_back(velocypack::Parser::fromJson(json));
      _args.emplace_back(AqlValueHintSliceNoCopy{_builders.back()->slice()});
    }
    return _args;
  }

  icu_64_64::Locale _locale{"en"};
  fakeit::Mock<ExpressionContext> _expressionContextMock;
  std::vector<std::shared_ptr<velocypack::Builder>> _builders;
  std::vector<AqlValue> _args;
};

TEST_F(SubstituteTest, replaces_a_search_string_with_a_replacement) {
  expectResult({R"("the quick brown fox")", R"("quick")", R"("slow")"},
               "the slow brown fox");
}

TEST_F(SubstituteTest, removes_matches_when_no_replacement_is_given) {
  expectResult({R"("the quick brown fox")", R"("quick ")"}, "the brown fox");
}

TEST_F(SubstituteTest, replaces_a_list_of_search_strings_pairwise) {
  expectResult({R"("the quick brown fox")", R"(["quick", "brown"])",
                R"(["slow", "red"])"},
               "the slow red fox");
}

TEST_F(SubstituteTest, removes_search_strings_without_a_paired_replacement) {
  expectResult(
      {R"("the quick brown fox")", R"(["quick", "brown"])", R"(["slow"])"},
      "the slow  fox");
}

TEST_F(SubstituteTest, uses_a_plain_string_replacement_for_every_search) {
  expectResult({R"("the quick brown fox")", R"(["quick", "brown"])", R"("x")"},
               "the x x fox");
}

TEST_F(SubstituteTest, replaces_using_an_object_mapping) {
  expectResult(
      {R"("the quick brown fox")", R"({"quick": "slow", "brown": null})"},
      "the slow  fox");
}

TEST_F(SubstituteTest, stops_after_limit_replacements) {
  expectResult({R"("aaa")", R"("a")", R"("b")", "2"}, "bba");
}

TEST_F(SubstituteTest, stops_after_limit_replacements_with_an_object_mapping) {
  expectResult({R"("aaa")", R"({"a": "b"})", "1"}, "baa");
}

TEST_F(SubstituteTest, returns_the_input_unchanged_for_a_zero_limit) {
  expectResult({R"("aaa")", R"("a")", R"("b")", "0"}, "aaa");
}

TEST_F(SubstituteTest, converts_a_non_string_value_to_a_string) {
  expectResult({"123", R"("2")", R"("x")"}, "1x3");
}

TEST_F(SubstituteTest, parses_a_plain_string_replacement_as_replace_all) {
  auto parsed = parse({R"("abc")", R"(["a", "b"])", R"("x")"});
  ASSERT_TRUE(parsed.ok());
  EXPECT_EQ(parsed->index(), 1U);
}

TEST_F(SubstituteTest, rejects_a_non_string_search) {
  expectError({R"("abc")", "1", R"("x")"},
              TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
}

TEST_F(SubstituteTest, rejects_a_non_string_in_the_search_list) {
  expectError({R"("abc")", R"(["a", 1])"},
              TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
}

TEST_F(SubstituteTest, rejects_a_non_string_in_the_replacement_list) {
  expectError({R"("abc")", R"(["a"])", "[1]"},
              TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
}

TEST_F(SubstituteTest, rejects_a_replacement_that_is_not_a_string_or_list) {
  expectError({R"("abc")", R"("a")", "1"},
              TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
}

TEST_F(SubstituteTest, rejects_a_non_string_value_in_an_object_mapping) {
  expectError({R"("abc")", R"({"a": 1})"},
              TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
}

TEST_F(SubstituteTest, rejects_an_object_mapping_with_four_arguments) {
  expectError({R"("abc")", R"({"a": "b"})", "1", "2"},
              TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH);
}

}  // namespace
