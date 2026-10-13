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

namespace arangodb::tests::aql {

using SubstituteFunctionTest = StringFunctionTest;
using arangodb::aql::functions::substitute;

TEST_F(SubstituteFunctionTest, replaces_search_string_with_replacement) {
  expectResult(substitute(ctx, str("the quick brown fox"), str("fox"),
                          &str("dog"), nullptr),
               "the quick brown dog");
}

TEST_F(SubstituteFunctionTest, removes_matches_when_no_replacement_given) {
  expectResult(substitute(ctx, str("the quick brown fox"), str("quick "),
                          nullptr, nullptr),
               "the brown fox");
}

TEST_F(SubstituteFunctionTest, replaces_each_search_with_matching_replacement) {
  expectResult(
      substitute(ctx, str("the quick brown fox"), json(R"(["quick", "fox"])"),
                 &json(R"(["slow", "cat"])"), nullptr),
      "the slow brown cat");
}

TEST_F(SubstituteFunctionTest,
       plain_string_replacement_applies_to_all_searches) {
  expectResult(substitute(ctx, str("the quick brown fox"),
                          json(R"(["quick", "brown"])"), &str("x"), nullptr),
               "the x x fox");
}

TEST_F(SubstituteFunctionTest, removes_matches_without_replacement_entry) {
  expectResult(substitute(ctx, str("the quick brown fox"),
                          json(R"(["quick", "brown", "fox"])"),
                          &json(R"(["slow"])"), nullptr),
               "the slow  ");
  expectResult(substitute(ctx, str("the quick brown fox"), json(R"(["brown"])"),
                          &json("[]"), nullptr),
               "the quick  fox");
}

TEST_F(SubstituteFunctionTest, null_or_empty_replacement_removes_match) {
  expectResult(substitute(ctx, str("the quick brown fox"),
                          json(R"(["quick", "brown", "fox"])"),
                          &json(R"(["", null, "ant"])"), nullptr),
               "the   ant");
  expectResult(substitute(ctx, str("the quick brown fox"),
                          json(R"({"quick": "", "brown": null, "fox": "ant"})"),
                          nullptr, nullptr),
               "the   ant");
}

TEST_F(SubstituteFunctionTest, mapping_object_replaces_keys_with_values) {
  expectResult(
      substitute(ctx, str("the quick brown fox"),
                 json(R"({"quick": "slow", "fox": null})"), nullptr, nullptr),
      "the slow brown ");
}

TEST_F(SubstituteFunctionTest, applies_all_patterns_in_one_pass) {
  expectResult(
      substitute(ctx, str("fox and dog"),
                 json(R"({"fox": "dog", "dog": "fox"})"), nullptr, nullptr),
      "dog and fox");
  expectResult(substitute(ctx, str("fox and dog"), json(R"(["fox", "dog"])"),
                          &json(R"(["dog", "fox"])"), nullptr),
               "dog and fox");
}

TEST_F(SubstituteFunctionTest, limit_caps_number_of_replacements) {
  expectResult(substitute(ctx, str("a a a"), str("a"), &str("b"), &json("2")),
               "b b a");
}

TEST_F(SubstituteFunctionTest, limit_counts_across_all_patterns) {
  expectResult(substitute(ctx, str("aaaabbbbccc"), json(R"(["a", "b", "c"])"),
                          &json(R"(["x", "y", "z"])"), &json("6")),
               "xxxxyybbccc");
  expectResult(substitute(ctx, str("aBcDef"),
                          json(R"({"a": "A", "B": "b", "c": "C", "D": "d"})"),
                          &json("2"), nullptr),
               "AbcDef");
}

TEST_F(SubstituteFunctionTest, zero_limit_returns_input_unchanged) {
  expectResult(
      substitute(ctx, str("a a a"), json(R"({"a": "b"})"), &json("0"), nullptr),
      "a a a");
  expectResult(substitute(ctx, str("a a a"), str("a"), &str("b"), &json("0")),
               "a a a");
}

TEST_F(SubstituteFunctionTest, only_minus_one_disables_limit) {
  expectResult(substitute(ctx, str("a a a"), str("a"), &str("b"), &json("-1")),
               "b b b");
  expectResult(substitute(ctx, str("a a a"), str("a"), &str("b"), &json("-2")),
               "a a a");
}

TEST_F(SubstituteFunctionTest, returns_input_when_nothing_matches) {
  expectResult(
      substitute(ctx, str("the fox"), str("cat"), &str("dog"), nullptr),
      "the fox");
  expectResult(substitute(ctx, str("the fox"), json(R"({"cat": "dog"})"),
                          nullptr, nullptr),
               "the fox");
  expectResult(substitute(ctx, str("the fox"), json("[]"), nullptr, nullptr),
               "the fox");
}

TEST_F(SubstituteFunctionTest, empty_input_returns_empty_string) {
  expectResult(substitute(ctx, str(""), str("foo"), &str("bar"), nullptr), "");
  expectResult(substitute(ctx, str(""), json(R"(["foo", "baz"])"),
                          &json(R"(["bar", "qux"])"), nullptr),
               "");
  expectResult(
      substitute(ctx, str(""), json(R"({"foo": "bar"})"), nullptr, nullptr),
      "");
}

TEST_F(SubstituteFunctionTest, empty_search_inserts_between_characters) {
  expectResult(substitute(ctx, str("abc"), str(""), &str("_"), nullptr),
               "_a_b_c_");
  expectResult(substitute(ctx, str("abc"), str(""), &str("_"), &json("2")),
               "_a_bc");
  expectResult(
      substitute(ctx, str("abc"), json(R"({"": "_"})"), nullptr, nullptr),
      "_a_b_c_");
  expectResult(substitute(ctx, str("öü"), str(""), &str("-"), nullptr),
               "-ö-ü-");
  expectResult(substitute(ctx, str(""), str(""), &str("x"), nullptr), "x");
  expectResult(substitute(ctx, str("abc"), json(R"(["b", ""])"),
                          &json(R"(["-", "_"])"), nullptr),
               "_a-_c_");
}

TEST_F(SubstituteFunctionTest,
       duplicate_empty_patterns_all_apply_at_each_position) {
  expectResult(substitute(ctx, str("abc"), json(R"(["", ""])"),
                          &json(R"(["_", "-"])"), nullptr),
               "_-a_-b_-c_-");
}

TEST_F(SubstituteFunctionTest, earliest_match_wins) {
  expectResult(substitute(ctx, str("baaaa"), json(R"(["ba", "aaa"])"),
                          &json(R"(["<1>", "<2>"])"), nullptr),
               "<1><2>");
  expectResult(substitute(ctx, str("baaaa"), json(R"(["aaa", "ba"])"),
                          &json(R"(["<2>", "<1>"])"), nullptr),
               "<1><2>");
  expectResult(substitute(ctx, str("bab"), json(R"(["ba", "ab"])"),
                          &json(R"(["<1>", "<2>"])"), nullptr),
               "<1>b");
}

TEST_F(SubstituteFunctionTest, earlier_pattern_wins_at_same_position) {
  expectResult(substitute(ctx, str("ab"), json(R"(["a", "ab"])"),
                          &json(R"(["1", "2"])"), nullptr),
               "1b");
  expectResult(substitute(ctx, str("ab"), json(R"(["ab", "a"])"),
                          &json(R"(["2", "1"])"), nullptr),
               "2");
}

TEST_F(SubstituteFunctionTest, matches_do_not_overlap) {
  expectResult(substitute(ctx, str("aaaaaa"), str("aaa"), &str("x"), nullptr),
               "xx");
  expectResult(substitute(ctx, str("baaaaaaa"), json(R"(["ba", "aaa"])"),
                          &json(R"(["<1>", "<2>"])"), nullptr),
               "<1><2><2>");
}

TEST_F(SubstituteFunctionTest, is_case_sensitive) {
  expectResult(substitute(ctx, str("abc"), str("A"), &str("x"), nullptr),
               "abc");
}

TEST_F(SubstituteFunctionTest, treats_patterns_as_literal_text) {
  expectResult(substitute(ctx, str("abc"), str("."), &str("x"), nullptr),
               "abc");
  expectResult(
      substitute(ctx, str("a\r\nb\r\nc"), str("\r\n"), &str("."), nullptr),
      "a.b.c");
}

TEST_F(SubstituteFunctionTest, handles_multibyte_characters) {
  expectResult(
      substitute(ctx, str("the fox"), str("fox"), &str("mötör"), nullptr),
      "the mötör");
  expectResult(substitute(ctx, str("mötör"), str("ö"), &str("o"), nullptr),
               "motor");
}

TEST_F(SubstituteFunctionTest, converts_non_string_input_to_string) {
  expectResult(substitute(ctx, json("123"), str("2"), &str("x"), nullptr),
               "1x3");
  expectResult(substitute(ctx, json("123"), json("[]"), &str("x"), nullptr),
               "123");
  expectResult(substitute(ctx, json("null"), str("n"), &str("x"), nullptr), "");
}

TEST_F(SubstituteFunctionTest, non_string_search_is_type_mismatch) {
  expectError(substitute(ctx, str("abc"), json("1"), &str("x"), nullptr),
              TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  expectError(
      substitute(ctx, str("abc"), json(R"(["a", 1])"), nullptr, nullptr),
      TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
}

TEST_F(SubstituteFunctionTest, non_string_replacement_is_type_mismatch) {
  expectError(substitute(ctx, str("abc"), str("a"), &json("1"), nullptr),
              TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  expectError(substitute(ctx, str("abc"), str("a"), &json("null"), nullptr),
              TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  expectError(substitute(ctx, str("abc"), str("a"), &json("{}"), nullptr),
              TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  expectError(
      substitute(ctx, str("abc"), json(R"(["a"])"), &json("[1]"), nullptr),
      TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
}

TEST_F(SubstituteFunctionTest, non_string_mapping_value_is_type_mismatch) {
  expectError(
      substitute(ctx, str("abc"), json(R"({"a": 1})"), nullptr, nullptr),
      TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
}

TEST_F(SubstituteFunctionTest,
       mapping_with_extra_arguments_is_number_mismatch) {
  expectError(substitute(ctx, str("abc"), json(R"({"a": "b"})"), &json("1"),
                         &json("2")),
              TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH);
}

}  // namespace arangodb::tests::aql
