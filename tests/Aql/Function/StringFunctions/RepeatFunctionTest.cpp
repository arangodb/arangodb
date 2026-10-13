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

#include "gtest/gtest.h"

#include "Aql/Function/StringFunctions.h"
#include "StringFunctionTestHelper.h"

#include <string>

namespace arangodb::tests::aql {

using RepeatFunctionTest = StringFunctionTest;
using arangodb::aql::functions::repeat;
constexpr int64_t kMaxOutputLength = 16 * 1024 * 1024;

TEST_F(RepeatFunctionTest, value_is_repeated_the_given_number_of_times) {
  expectResult(repeat(ctx, str("foo"), json("1"), nullptr), "foo");
  expectResult(repeat(ctx, str("foo"), json("3"), nullptr), "foofoofoo");
}

TEST_F(RepeatFunctionTest, zero_repetitions_give_empty_string) {
  expectResult(repeat(ctx, str("foo"), json("0"), nullptr), "");
  expectResult(repeat(ctx, str("foo"), json("0"), &str("-")), "");
}

TEST_F(RepeatFunctionTest, separator_is_placed_between_repetitions) {
  expectResult(repeat(ctx, str("foo"), json("1"), &str("-")), "foo");
  expectResult(repeat(ctx, str("foo"), json("3"), &str(", ")), "foo, foo, foo");
}

TEST_F(RepeatFunctionTest, empty_values_still_get_separators) {
  expectResult(repeat(ctx, str(""), json("3"), &str("-")), "--");
  expectResult(repeat(ctx, json("null"), json("3"), &str("-")), "--");
}

TEST_F(RepeatFunctionTest, empty_values_without_separator_return_immediately) {
  expectResult(repeat(ctx, str(""), json("9223372036854775807"), nullptr), "");
  expectResult(repeat(ctx, json("null"), json("9223372036854775807"), &str("")),
               "");
}

TEST_F(RepeatFunctionTest, empty_or_null_separator_is_no_separator) {
  expectResult(repeat(ctx, str("foo"), json("3"), &str("")), "foofoofoo");
  expectResult(repeat(ctx, str("foo"), json("3"), &json("null")), "foofoofoo");
}

TEST_F(RepeatFunctionTest, multibyte_strings_are_repeated_unchanged) {
  expectResult(repeat(ctx, str("ö😀"), json("2"), &str("€")), "ö😀€ö😀");
}

TEST_F(RepeatFunctionTest, non_string_values_are_stringified) {
  expectResult(repeat(ctx, json("null"), json("3"), nullptr), "");
  expectResult(repeat(ctx, json("true"), json("2"), nullptr), "truetrue");
  expectResult(repeat(ctx, json("1.5"), json("2"), nullptr), "1.51.5");
  expectResult(repeat(ctx, json(R"([1, "a"])"), json("2"), &str("|")),
               R"([1,"a"]|[1,"a"])");
  expectResult(repeat(ctx, json(R"({"a": 1})"), json("2"), nullptr),
               R"({"a":1}{"a":1})");
}

TEST_F(RepeatFunctionTest, non_string_separators_are_stringified) {
  expectResult(repeat(ctx, str("a"), json("3"), &json("1")), "a1a1a");
  expectResult(repeat(ctx, str("a"), json("2"), &json("false")), "afalsea");
  expectResult(repeat(ctx, str("a"), json("2"), &json("[1]")), "a[1]a");
}

TEST_F(RepeatFunctionTest, repetitions_are_converted_to_integers) {
  expectResult(repeat(ctx, str("a"), json("2.9"), nullptr), "aa");
  expectResult(repeat(ctx, str("a"), str("3"), nullptr), "aaa");
  expectResult(repeat(ctx, str("a"), json("true"), nullptr), "a");
  expectResult(repeat(ctx, str("a"), json("[2]"), nullptr), "aa");
}

TEST_F(RepeatFunctionTest, non_numeric_repetitions_count_as_zero) {
  for (auto repetitions :
       {R"(null)", R"(false)", R"("x")", R"([])", R"([1, 2])", R"({})"}) {
    expectResult(repeat(ctx, str("a"), json(repetitions), nullptr), "");
  }
}

TEST_F(RepeatFunctionTest, negative_repetitions_are_rejected) {
  expectError(repeat(ctx, str("a"), json("-1"), nullptr),
              TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  expectError(repeat(ctx, str("a"), str("-3"), nullptr),
              TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
}

TEST_F(RepeatFunctionTest, output_up_to_16mb_is_allowed) {
  auto result = repeat(ctx, str("abcd"),
                       json(std::to_string(kMaxOutputLength / 4)), nullptr);
  ASSERT_TRUE(result.ok()) << result.errorMessage();
  arangodb::aql::AqlValueGuard guard(*result, true);
  EXPECT_EQ(result->slice().getStringLength(), kMaxOutputLength);
}

TEST_F(RepeatFunctionTest, output_over_16mb_is_rejected) {
  expectError(repeat(ctx, str("abcd"),
                     json(std::to_string(kMaxOutputLength / 4 + 1)), nullptr),
              TRI_ERROR_RESOURCE_LIMIT);
}

TEST_F(RepeatFunctionTest, separators_count_towards_16mb_limit) {
  expectError(repeat(ctx, json("null"),
                     json(std::to_string(kMaxOutputLength / 1024 + 2)),
                     &str(std::string(1024, 'x'))),
              TRI_ERROR_RESOURCE_LIMIT);
}

}  // namespace arangodb::tests::aql
