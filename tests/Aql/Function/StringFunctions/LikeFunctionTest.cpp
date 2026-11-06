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

#include <string>

namespace arangodb::tests::aql {

using arangodb::aql::functions::like;

TEST_F(StringFunctionTest, like_matches_the_whole_text) {
  expectBool(like(ctx, str("abc"), str("abc"), nullptr), true);
  expectBool(like(ctx, str("abc"), str("ab"), nullptr), false);
  expectBool(like(ctx, str("xabc"), str("abc"), nullptr), false);
  expectBool(like(ctx, str(""), str(""), nullptr), true);
}

TEST_F(StringFunctionTest, like_percent_matches_any_sequence) {
  expectBool(like(ctx, str("abc"), str("a%"), nullptr), true);
  expectBool(like(ctx, str("abc"), str("%c"), nullptr), true);
  expectBool(like(ctx, str("abc"), str("%b%"), nullptr), true);
  expectBool(like(ctx, str("abc"), str("a%b%c"), nullptr), true);
  expectBool(like(ctx, str(""), str("%"), nullptr), true);
  expectBool(like(ctx, str("abc"), str("%d%"), nullptr), false);
}

TEST_F(StringFunctionTest, like_underscore_matches_exactly_one_character) {
  expectBool(like(ctx, str("abc"), str("a_c"), nullptr), true);
  expectBool(like(ctx, str("ac"), str("a_c"), nullptr), false);
  expectBool(like(ctx, str("abbc"), str("a_c"), nullptr), false);
  expectBool(like(ctx, str(""), str("_"), nullptr), false);
  expectBool(like(ctx, str("😀"), str("_"), nullptr), true);
}

TEST_F(StringFunctionTest, like_underscore_matches_code_points_not_graphemes) {
  expectBool(like(ctx, str("e\u0301"), str("_"), nullptr), false);
  expectBool(like(ctx, str("e\u0301"), str("__"), nullptr), true);
  expectBool(like(ctx, str("🇭🇷"), str("_"), nullptr), false);
  expectBool(like(ctx, str("\u00e9"), str("e\u0301"), nullptr), false);
}

TEST_F(StringFunctionTest, like_wildcards_match_carriage_return_and_newline) {
  expectBool(like(ctx, str("a\nb"), str("a%b"), nullptr), true);
  expectBool(like(ctx, str("a\r\nb"), str("a__b"), nullptr), true);
  expectBool(like(ctx, str("a\n"), str("a"), nullptr), false);
}

TEST_F(StringFunctionTest, like_wildcards_do_not_match_other_line_terminators) {
  expectBool(like(ctx, str("a\u2028b"), str("a%b"), nullptr), false);
  expectBool(like(ctx, str("\u2029"), str("_"), nullptr), false);
  expectBool(like(ctx, str("\u0085"), str("_"), nullptr), false);
  expectBool(like(ctx, str("\v"), str("_"), nullptr), false);
  expectBool(like(ctx, str("\f"), str("_"), nullptr), false);
}

TEST_F(StringFunctionTest, like_escaped_wildcards_are_literal) {
  expectBool(like(ctx, str("100%"), str("100\\%"), nullptr), true);
  expectBool(like(ctx, str("1000"), str("100\\%"), nullptr), false);
  expectBool(like(ctx, str("a_"), str("a\\_"), nullptr), true);
  expectBool(like(ctx, str("ab"), str("a\\_"), nullptr), false);
}

TEST_F(StringFunctionTest, like_double_backslash_matches_a_backslash) {
  expectBool(like(ctx, str("a\\b"), str("a\\\\b"), nullptr), true);
  expectBool(like(ctx, str("\\x"), str("\\\\%"), nullptr), true);
}

TEST_F(StringFunctionTest, like_backslash_before_other_character_is_literal) {
  expectBool(like(ctx, str("\\a"), str("\\a"), nullptr), true);
  expectBool(like(ctx, str("a"), str("\\a"), nullptr), false);
}

TEST_F(StringFunctionTest, like_trailing_backslash_is_dropped) {
  expectBool(like(ctx, str("a"), str("a\\"), nullptr), true);
  expectBool(like(ctx, str("a\\"), str("a\\"), nullptr), false);
}

TEST_F(StringFunctionTest, like_regex_special_characters_are_literal) {
  expectBool(like(ctx, str("a.b"), str("a.b"), nullptr), true);
  expectBool(like(ctx, str("axb"), str("a.b"), nullptr), false);
  expectBool(like(ctx, str("a.b"), str("a\\.b"), nullptr), true);
  expectBool(like(ctx, str("a*b+c?"), str("a*b+c?"), nullptr), true);
  expectBool(like(ctx, str("aab"), str("a*b"), nullptr), false);
  expectBool(like(ctx, str("(a)[b]{c}^$|"), str("(a)[b]{c}^$|"), nullptr),
             true);
  expectBool(like(ctx, str("a]b#c d"), str("a]b#c d"), nullptr), true);
}

TEST_F(StringFunctionTest, like_is_case_sensitive_by_default) {
  expectBool(like(ctx, str("ABC"), str("abc"), nullptr), false);
  expectBool(like(ctx, str("\u00c4"), str("\u00e4"), nullptr), false);
  expectBool(like(ctx, str("ABC"), str("abc"), &json("false")), false);
}

TEST_F(StringFunctionTest, like_case_insensitive_flag_ignores_case) {
  expectBool(like(ctx, str("ABC"), str("abc"), &json("true")), true);
  expectBool(like(ctx, str("ABC"), str("a%"), &json("true")), true);
  expectBool(like(ctx, str("\u00c4"), str("\u00e4"), &json("true")), true);
  expectBool(like(ctx, str("STRASSE"), str("stra\u00dfe"), &json("true")),
             true);
  expectBool(like(ctx, str("\u212a"), str("k"), &json("true")), true);
}

TEST_F(StringFunctionTest, like_case_insensitive_flag_is_converted_to_bool) {
  expectBool(like(ctx, str("A"), str("a"), &str("x")), true);
  expectBool(like(ctx, str("A"), str("a"), &json("1")), true);
  expectBool(like(ctx, str("A"), str("a"), &json("0")), false);
  expectBool(like(ctx, str("A"), str("a"), &json("null")), false);
}

TEST_F(StringFunctionTest, like_cached_matchers_keep_the_case_flag_apart) {
  expectBool(like(ctx, str("A"), str("a"), nullptr), false);
  expectBool(like(ctx, str("A"), str("a"), &json("true")), true);
  expectBool(like(ctx, str("A"), str("a"), nullptr), false);
  expectBool(like(ctx, str("a"), str("a"), nullptr), true);
}

TEST_F(StringFunctionTest, like_non_string_arguments_are_stringified) {
  expectBool(like(ctx, json("null"), str(""), nullptr), true);
  expectBool(like(ctx, json("1"), str("1"), nullptr), true);
  expectBool(like(ctx, json("[1]"), str("[1]"), nullptr), true);
  expectBool(like(ctx, json("true"), str("t%"), nullptr), true);
  expectBool(like(ctx, str(""), json("null"), nullptr), true);
  expectBool(like(ctx, str("null"), json("null"), nullptr), false);
}

TEST_F(StringFunctionTest, like_invalid_utf8_bytes_count_as_one_character) {
  expectBool(like(ctx,
                  str("\xff"
                      "a"),
                  str("_a"), nullptr),
             true);
}

TEST_F(StringFunctionTest,
       like_wildcard_over_long_text_fails_as_invalid_regex) {
  auto const& text = str(std::string(200'000, 'a'));
  expectError(like(ctx, text, str("a%"), nullptr),
              TRI_ERROR_QUERY_INVALID_REGEX);
  expectBool(like(ctx, str(std::string(100'000, 'a')), str("a%"), nullptr),
             true);
}

}  // namespace arangodb::tests::aql
