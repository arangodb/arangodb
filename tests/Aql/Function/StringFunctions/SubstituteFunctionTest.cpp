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
#include "Basics/voc-errors.h"
#include "StringFunctionTestHelper.h"

using namespace arangodb;
using namespace arangodb::aql::functions;
using namespace arangodb::tests::aql;

namespace {

constexpr auto kSubstitute = StringFunctionsName::kSubstitute;

}  // namespace

TEST(SubstituteFunctionTest, replaces_search_string_with_replacement) {
  expectResult(kSubstitute, R"(["the quick brown fox", "fox", "dog"])",
               "the quick brown dog");
}

TEST(SubstituteFunctionTest, removes_matches_when_no_replacement_given) {
  expectResult(kSubstitute, R"(["the quick brown fox", "quick "])",
               "the brown fox");
}

TEST(SubstituteFunctionTest, replaces_each_search_with_matching_replacement) {
  expectResult(kSubstitute,
               R"(["the quick brown fox", ["quick", "fox"], ["slow", "cat"]])",
               "the slow brown cat");
}

TEST(SubstituteFunctionTest, plain_string_replacement_applies_to_all_searches) {
  expectResult(kSubstitute,
               R"(["the quick brown fox", ["quick", "brown"], "x"])",
               "the x x fox");
}

TEST(SubstituteFunctionTest, removes_matches_without_replacement_entry) {
  expectResult(
      kSubstitute,
      R"(["the quick brown fox", ["quick", "brown", "fox"], ["slow"]])",
      "the slow  ");
  expectResult(kSubstitute, R"(["the quick brown fox", ["brown"], []])",
               "the quick  fox");
}

TEST(SubstituteFunctionTest, null_or_empty_replacement_removes_match) {
  expectResult(kSubstitute,
               R"(["the quick brown fox", ["quick", "brown", "fox"],
                   ["", null, "ant"]])",
               "the   ant");
  expectResult(kSubstitute,
               R"(["the quick brown fox",
                   {"quick": "", "brown": null, "fox": "ant"}])",
               "the   ant");
}

TEST(SubstituteFunctionTest, mapping_object_replaces_keys_with_values) {
  expectResult(kSubstitute,
               R"(["the quick brown fox", {"quick": "slow", "fox": null}])",
               "the slow brown ");
}

TEST(SubstituteFunctionTest, applies_all_patterns_in_one_pass) {
  expectResult(kSubstitute, R"(["fox and dog", {"fox": "dog", "dog": "fox"}])",
               "dog and fox");
  expectResult(kSubstitute,
               R"(["fox and dog", ["fox", "dog"], ["dog", "fox"]])",
               "dog and fox");
}

TEST(SubstituteFunctionTest, limit_caps_number_of_replacements) {
  expectResult(kSubstitute, R"(["a a a", "a", "b", 2])", "b b a");
}

TEST(SubstituteFunctionTest, limit_counts_across_all_patterns) {
  expectResult(kSubstitute,
               R"(["aaaabbbbccc", ["a", "b", "c"], ["x", "y", "z"], 6])",
               "xxxxyybbccc");
  expectResult(kSubstitute,
               R"(["aBcDef", {"a": "A", "B": "b", "c": "C", "D": "d"}, 2])",
               "AbcDef");
}

TEST(SubstituteFunctionTest, zero_limit_returns_input_unchanged) {
  expectResult(kSubstitute, R"(["a a a", {"a": "b"}, 0])", "a a a");
  expectResult(kSubstitute, R"(["a a a", "a", "b", 0])", "a a a");
}

TEST(SubstituteFunctionTest, only_minus_one_disables_limit) {
  expectResult(kSubstitute, R"(["a a a", "a", "b", -1])", "b b b");
  expectResult(kSubstitute, R"(["a a a", "a", "b", -2])", "a a a");
}

TEST(SubstituteFunctionTest, returns_input_when_nothing_matches) {
  expectResult(kSubstitute, R"(["the fox", "cat", "dog"])", "the fox");
  expectResult(kSubstitute, R"(["the fox", {"cat": "dog"}])", "the fox");
  expectResult(kSubstitute, R"(["the fox", []])", "the fox");
}

TEST(SubstituteFunctionTest, empty_input_returns_empty_string) {
  expectResult(kSubstitute, R"(["", "foo", "bar"])", "");
  expectResult(kSubstitute, R"(["", ["foo", "baz"], ["bar", "qux"]])", "");
  expectResult(kSubstitute, R"(["", {"foo": "bar"}])", "");
}

TEST(SubstituteFunctionTest, empty_search_inserts_between_characters) {
  expectResult(kSubstitute, R"(["abc", "", "_"])", "_a_b_c_");
  expectResult(kSubstitute, R"(["abc", "", "_", 2])", "_a_bc");
  expectResult(kSubstitute, R"(["abc", {"": "_"}])", "_a_b_c_");
  expectResult(kSubstitute, R"(["öü", "", "-"])", "-ö-ü-");
  expectResult(kSubstitute, R"(["", "", "x"])", "x");
  expectResult(kSubstitute, R"(["abc", ["b", ""], ["-", "_"]])", "_a-_c_");
}

TEST(SubstituteFunctionTest,
     duplicate_empty_patterns_all_apply_at_each_position) {
  expectResult(kSubstitute, R"(["abc", ["", ""], ["_", "-"]])", "_-a_-b_-c_-");
}

TEST(SubstituteFunctionTest, earliest_match_wins) {
  expectResult(kSubstitute, R"(["baaaa", ["ba", "aaa"], ["<1>", "<2>"]])",
               "<1><2>");
  expectResult(kSubstitute, R"(["baaaa", ["aaa", "ba"], ["<2>", "<1>"]])",
               "<1><2>");
  expectResult(kSubstitute, R"(["bab", ["ba", "ab"], ["<1>", "<2>"]])", "<1>b");
}

TEST(SubstituteFunctionTest, earlier_pattern_wins_at_same_position) {
  expectResult(kSubstitute, R"(["ab", ["a", "ab"], ["1", "2"]])", "1b");
  expectResult(kSubstitute, R"(["ab", ["ab", "a"], ["2", "1"]])", "2");
}

TEST(SubstituteFunctionTest, matches_do_not_overlap) {
  expectResult(kSubstitute, R"(["aaaaaa", "aaa", "x"])", "xx");
  expectResult(kSubstitute, R"(["baaaaaaa", ["ba", "aaa"], ["<1>", "<2>"]])",
               "<1><2><2>");
}

TEST(SubstituteFunctionTest, is_case_sensitive) {
  expectResult(kSubstitute, R"(["abc", "A", "x"])", "abc");
}

TEST(SubstituteFunctionTest, treats_patterns_as_literal_text) {
  expectResult(kSubstitute, R"(["abc", ".", "x"])", "abc");
  expectResult(kSubstitute, R"(["a\r\nb\r\nc", "\r\n", "."])", "a.b.c");
}

TEST(SubstituteFunctionTest, handles_multibyte_characters) {
  expectResult(kSubstitute, R"(["the fox", "fox", "mötör"])", "the mötör");
  expectResult(kSubstitute, R"(["mötör", "ö", "o"])", "motor");
}

TEST(SubstituteFunctionTest, converts_non_string_input_to_string) {
  expectResult(kSubstitute, R"([123, "2", "x"])", "1x3");
  expectResult(kSubstitute, R"([123, [], "x"])", "123");
  expectResult(kSubstitute, R"([null, "n", "x"])", "");
}

TEST(SubstituteFunctionTest, non_string_search_is_type_mismatch) {
  expectError(kSubstitute, R"(["abc", 1, "x"])",
              TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  expectError(kSubstitute, R"(["abc", ["a", 1]])",
              TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
}

TEST(SubstituteFunctionTest, non_string_replacement_is_type_mismatch) {
  expectError(kSubstitute, R"(["abc", "a", 1])",
              TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  expectError(kSubstitute, R"(["abc", "a", null])",
              TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  expectError(kSubstitute, R"(["abc", "a", {}])",
              TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  expectError(kSubstitute, R"(["abc", ["a"], [1]])",
              TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
}

TEST(SubstituteFunctionTest, non_string_mapping_value_is_type_mismatch) {
  expectError(kSubstitute, R"(["abc", {"a": 1}])",
              TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
}

TEST(SubstituteFunctionTest, missing_search_is_number_mismatch) {
  expectError(kSubstitute, R"(["abc"])",
              TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH);
}

TEST(SubstituteFunctionTest, mapping_with_extra_arguments_is_number_mismatch) {
  expectError(kSubstitute, R"(["abc", {"a": "b"}, 1, 2])",
              TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH);
}
