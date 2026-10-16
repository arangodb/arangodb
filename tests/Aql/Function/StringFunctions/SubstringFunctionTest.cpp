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

#include <velocypack/Exception.h>

namespace arangodb::tests::aql {

using arangodb::aql::functions::substring;

TEST_F(StringFunctionTest, substring_returns_rest_of_text_from_offset) {
  expectResult(substring(ctx, str("abc"), json("0"), nullptr), "abc");
  expectResult(substring(ctx, str("abc"), json("1"), nullptr), "bc");
  expectResult(substring(ctx, str("abc"), json("3"), nullptr), "");
  expectResult(substring(ctx, str("abc"), json("5"), nullptr), "");
}

TEST_F(StringFunctionTest, substring_length_limits_the_result) {
  expectResult(
      substring(ctx, str("the quick brown fox"), json("4"), &json("5")),
      "quick");
  expectResult(substring(ctx, str("abc"), json("1"), &json("1")), "b");
  expectResult(substring(ctx, str("abc"), json("1"), &json("10")), "bc");
  expectResult(substring(ctx, str("abc"), json("0"), &json("0")), "");
}

TEST_F(StringFunctionTest, substring_negative_offset_counts_from_the_end) {
  expectResult(substring(ctx, str("abc"), json("-1"), nullptr), "c");
  expectResult(substring(ctx, str("abc"), json("-3"), nullptr), "abc");
  expectResult(substring(ctx, str("abc"), json("-5"), nullptr), "abc");
  expectResult(substring(ctx, str("abc"), json("-5"), &json("2")), "ab");
}

TEST_F(StringFunctionTest, substring_negative_length_gives_empty_string) {
  expectResult(substring(ctx, str("abc"), json("0"), &json("-1")), "");
  expectResult(substring(ctx, str("abc"), json("2"), &json("-1")), "");
  expectResult(substring(ctx, str("abc"), json("-1"), &json("-1")), "");
}

TEST_F(StringFunctionTest, substring_counts_code_points) {
  expectResult(substring(ctx, str("😀x"), json("1"), nullptr), "x");
  expectResult(substring(ctx, str("😀x"), json("0"), &json("1")), "😀");
  expectResult(substring(ctx, str("x😀y"), json("-2"), &json("1")), "😀");
  expectResult(substring(ctx, str("🤡cpp quick"), json("0"), &json("3")),
               "🤡cp");
}

TEST_F(StringFunctionTest, substring_can_split_a_character_sequence) {
  expectResult(substring(ctx, str("e\xcc\x81x"), json("0"), &json("1")), "e");
  expectResult(substring(ctx, str("🇭🇷!"), json("0"), &json("1")), "🇭");
}

TEST_F(StringFunctionTest, substring_non_string_values_are_stringified) {
  expectResult(substring(ctx, json("null"), json("0"), nullptr), "");
  expectResult(substring(ctx, json("true"), json("0"), nullptr), "true");
  expectResult(substring(ctx, json("1.5"), json("1"), nullptr), ".5");
  expectResult(substring(ctx, json("[1,2,3]"), json("3"), nullptr), "2,3]");
  expectResult(substring(ctx, json("{}"), json("0"), nullptr), "{}");
}

TEST_F(StringFunctionTest, substring_offset_and_length_are_converted_to_ints) {
  expectResult(substring(ctx, str("abc"), json("1.9"), nullptr), "bc");
  expectResult(substring(ctx, str("abc"), str("1"), nullptr), "bc");
  expectResult(substring(ctx, str("abc"), json("0"), &str("2")), "ab");
  expectResult(substring(ctx, str("yes"), str("yes"), &json("true")), "y");
  expectResult(substring(ctx, str("yes"), json("null"), &json("0")), "");
}

TEST_F(StringFunctionTest, substring_null_length_means_zero) {
  expectResult(substring(ctx, str("abc"), json("0"), &json("null")), "");
}

TEST_F(StringFunctionTest, substring_offset_and_length_wrap_at_32_bits) {
  expectResult(substring(ctx, str("abc"), json("4294967297"), nullptr), "bc");
  expectResult(substring(ctx, str("abc"), json("-2147483649"), nullptr), "");
  expectResult(substring(ctx, str("abc"), json("0"), &json("4294967296")), "");
  expectResult(substring(ctx, str("abc"), json("0"), &json("4294967298")),
               "ab");
}

TEST_F(StringFunctionTest, substring_int32_min_offset_or_length_gives_empty) {
  expectResult(substring(ctx, str("abc"), json("2147483648"), nullptr), "");
  expectResult(substring(ctx, str("abc"), json("0"), &json("2147483648")), "");
}

TEST_F(StringFunctionTest, substring_out_of_int64_range_numbers_throw) {
  EXPECT_THROW(substring(ctx, str("abc"), json("1e30"), nullptr),
               velocypack::Exception);
  EXPECT_THROW(substring(ctx, str("abc"), json("0"), &json("1e30")),
               velocypack::Exception);
}

TEST_F(StringFunctionTest, substring_of_empty_text_is_empty) {
  expectResult(substring(ctx, str(""), json("0"), nullptr), "");
  expectResult(substring(ctx, str(""), json("-1"), nullptr), "");
  expectResult(substring(ctx, str(""), json("0"), &json("5")), "");
}

TEST_F(StringFunctionTest, substring_invalid_utf8_becomes_replacement_char) {
  expectResult(substring(ctx,
                         str("\xff"
                             "a"),
                         json("0"), &json("1")),
               "\xef\xbf\xbd");
}

}  // namespace arangodb::tests::aql
