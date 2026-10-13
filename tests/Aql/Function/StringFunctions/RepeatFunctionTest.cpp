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

constexpr auto kRepeat = StringFunctionsName::kRepeat;
constexpr int64_t kMaxOutputLength = 16 * 1024 * 1024;

std::string repeatArgs(std::string_view value, int64_t repetitions) {
  return "[\"" + std::string(value) + "\", " + std::to_string(repetitions) +
         "]";
}

TEST(RepeatFunctionTest, value_is_repeated_the_given_number_of_times) {
  expectResult(kRepeat, R"(["foo", 1])", "foo");
  expectResult(kRepeat, R"(["foo", 3])", "foofoofoo");
}

TEST(RepeatFunctionTest, zero_repetitions_give_empty_string) {
  expectResult(kRepeat, R"(["foo", 0])", "");
  expectResult(kRepeat, R"(["foo", 0, "-"])", "");
}

TEST(RepeatFunctionTest, separator_is_placed_between_repetitions) {
  expectResult(kRepeat, R"(["foo", 1, "-"])", "foo");
  expectResult(kRepeat, R"(["foo", 3, ", "])", "foo, foo, foo");
}

TEST(RepeatFunctionTest, empty_values_still_get_separators) {
  expectResult(kRepeat, R"(["", 3, "-"])", "--");
  expectResult(kRepeat, R"([null, 3, "-"])", "--");
}

TEST(RepeatFunctionTest, empty_values_without_separator_return_immediately) {
  expectResult(kRepeat, R"(["", 9223372036854775807])", "");
  expectResult(kRepeat, R"([null, 9223372036854775807, ""])", "");
}

TEST(RepeatFunctionTest, empty_or_null_separator_is_no_separator) {
  expectResult(kRepeat, R"(["foo", 3, ""])", "foofoofoo");
  expectResult(kRepeat, R"(["foo", 3, null])", "foofoofoo");
}

TEST(RepeatFunctionTest, multibyte_strings_are_repeated_unchanged) {
  expectResult(kRepeat, R"(["ö😀", 2, "€"])", "ö😀€ö😀");
}

TEST(RepeatFunctionTest, non_string_values_are_stringified) {
  expectResult(kRepeat, R"([null, 3])", "");
  expectResult(kRepeat, R"([true, 2])", "truetrue");
  expectResult(kRepeat, R"([1.5, 2])", "1.51.5");
  expectResult(kRepeat, R"([[1, "a"], 2, "|"])", R"([1,"a"]|[1,"a"])");
  expectResult(kRepeat, R"([{"a": 1}, 2])", R"({"a":1}{"a":1})");
}

TEST(RepeatFunctionTest, non_string_separators_are_stringified) {
  expectResult(kRepeat, R"(["a", 3, 1])", "a1a1a");
  expectResult(kRepeat, R"(["a", 2, false])", "afalsea");
  expectResult(kRepeat, R"(["a", 2, [1]])", "a[1]a");
}

TEST(RepeatFunctionTest, repetitions_are_converted_to_integers) {
  expectResult(kRepeat, R"(["a", 2.9])", "aa");
  expectResult(kRepeat, R"(["a", "3"])", "aaa");
  expectResult(kRepeat, R"(["a", true])", "a");
  expectResult(kRepeat, R"(["a", [2]])", "aa");
}

TEST(RepeatFunctionTest, non_numeric_repetitions_count_as_zero) {
  for (auto args : {R"(["a", null])", R"(["a", false])", R"(["a", "x"])",
                    R"(["a", []])", R"(["a", [1, 2]])", R"(["a", {}])"}) {
    expectResult(kRepeat, args, "");
  }
}

TEST(RepeatFunctionTest, negative_repetitions_are_rejected) {
  expectError(kRepeat, R"(["a", -1])",
              TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  expectError(kRepeat, R"(["a", "-3"])",
              TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
}

TEST(RepeatFunctionTest, output_up_to_16mb_is_allowed) {
  auto result = call(kRepeat, repeatArgs("abcd", kMaxOutputLength / 4));
  ASSERT_TRUE(result.ok()) << result.errorMessage();
  EXPECT_EQ(result->size(), kMaxOutputLength);
}

TEST(RepeatFunctionTest, output_over_16mb_is_rejected) {
  expectError(kRepeat, repeatArgs("abcd", kMaxOutputLength / 4 + 1),
              TRI_ERROR_RESOURCE_LIMIT);
}

TEST(RepeatFunctionTest, separators_count_towards_16mb_limit) {
  std::string const separator(1024, 'x');
  std::string const args = "[null, " +
                           std::to_string(kMaxOutputLength / 1024 + 2) +
                           ", \"" + separator + "\"]";
  expectError(kRepeat, args, TRI_ERROR_RESOURCE_LIMIT);
}

}  // namespace arangodb::tests::aql
