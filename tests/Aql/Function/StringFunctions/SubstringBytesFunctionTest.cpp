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

namespace arangodb::tests::aql {

using arangodb::aql::functions::substringBytes;

TEST_F(StringFunctionTest, substring_bytes_offset_and_length_are_in_bytes) {
  expectResult(substringBytes(ctx, str("hello"), json("1"), &json("3"), nullptr,
                              nullptr),
               "ell");
  expectResult(substringBytes(ctx, str("🤡foo"), json("0"), &json("4"),
                              nullptr, nullptr),
               "🤡");
  expectResult(substringBytes(ctx, str("🤡foo"), json("4"), &json("2"),
                              nullptr, nullptr),
               "fo");
}

TEST_F(StringFunctionTest,
       substring_bytes_without_length_the_rest_is_returned) {
  expectResult(
      substringBytes(ctx, str("🤡foo"), json("0"), nullptr, nullptr, nullptr),
      "🤡foo");
  expectResult(
      substringBytes(ctx, str("🤡foo"), json("4"), nullptr, nullptr, nullptr),
      "foo");
}

TEST_F(StringFunctionTest,
       substring_bytes_negative_offset_counts_from_the_end) {
  expectResult(substringBytes(ctx, str("🤡foo"), json("-1"), &json("1"),
                              nullptr, nullptr),
               "o");
  expectResult(substringBytes(ctx, str("🤡foo"), json("-3"), &json("2"),
                              nullptr, nullptr),
               "fo");
}

TEST_F(StringFunctionTest,
       substring_bytes_out_of_range_offset_and_length_are_clamped) {
  expectResult(substringBytes(ctx, str("🤡foo"), json("-42"), &json("4"),
                              nullptr, nullptr),
               "🤡");
  expectResult(substringBytes(ctx, str("🤡foo"), json("42"), &json("2"),
                              nullptr, nullptr),
               "");
  expectResult(substringBytes(ctx, str("🤡foo"), json("4"), &json("42"),
                              nullptr, nullptr),
               "foo");
}

TEST_F(StringFunctionTest, substring_bytes_negative_length_gives_empty_string) {
  expectResult(substringBytes(ctx, str("🤡foo"), json("-2"), &json("-2"),
                              nullptr, nullptr),
               "");
}

TEST_F(StringFunctionTest, substring_bytes_empty_string_gives_empty_string) {
  expectResult(
      substringBytes(ctx, str(""), json("0"), &json("1"), nullptr, nullptr),
      "");
  expectResult(
      substringBytes(ctx, str(""), json("5"), &json("1"), &json("3"), nullptr),
      "");
}

TEST_F(StringFunctionTest,
       substring_bytes_offset_and_length_are_converted_to_integers) {
  expectResult(substringBytes(ctx, str("abc"), json("1.9"), &json("1"), nullptr,
                              nullptr),
               "b");
  expectResult(
      substringBytes(ctx, str("abc"), str("1"), &json("1"), nullptr, nullptr),
      "b");
  expectResult(substringBytes(ctx, str("abc"), json("[1]"), &json("[1]"),
                              nullptr, nullptr),
               "b");
}

TEST_F(StringFunctionTest,
       substring_bytes_non_numeric_offset_and_length_count_as_zero) {
  expectResult(substringBytes(ctx, str("🤡foo"), json("null"), &json("4"),
                              nullptr, nullptr),
               "🤡");
  expectResult(substringBytes(ctx, str("🤡foo"), str("foobar"), &json("4"),
                              nullptr, nullptr),
               "🤡");
  expectResult(substringBytes(ctx, str("🤡foo"), json("4"), &json("null"),
                              nullptr, nullptr),
               "");
  expectResult(substringBytes(ctx, str("🤡foo"), json("-2"), &str("foobar"),
                              nullptr, nullptr),
               "");
}

TEST_F(StringFunctionTest,
       substring_bytes_cutting_inside_a_character_is_rejected) {
  expectError(
      substringBytes(ctx, str("🤡"), json("1"), &json("2"), nullptr, nullptr),
      TRI_ERROR_BAD_PARAMETER);
  expectError(
      substringBytes(ctx, str("🤡"), json("0"), &json("2"), nullptr, nullptr),
      TRI_ERROR_BAD_PARAMETER);
  expectError(substringBytes(ctx, str("🤡foo"), json("-4"), &json("2"),
                             nullptr, nullptr),
              TRI_ERROR_BAD_PARAMETER);
  expectError(substringBytes(ctx, str("🤡foo"), json("-42"), &json("2"),
                             nullptr, nullptr),
              TRI_ERROR_BAD_PARAMETER);
}

TEST_F(StringFunctionTest, substring_bytes_non_string_values_are_rejected) {
  for (auto value : {R"(null)", R"(true)", R"(1)", R"(["🤡"])", R"({})"}) {
    expectError(substringBytes(ctx, json(value), json("0"), &json("1"), nullptr,
                               nullptr),
                TRI_ERROR_BAD_PARAMETER);
  }
}

TEST_F(StringFunctionTest,
       substring_bytes_left_and_right_extend_by_characters) {
  expectResult(substringBytes(ctx, str("aa🤡aa"), json("6"), &json("1"),
                              &json("1"), &json("0")),
               "🤡a");
  expectResult(substringBytes(ctx, str("aa🤡aa"), json("1"), &json("1"),
                              &json("0"), &json("1")),
               "a🤡");
  expectResult(substringBytes(ctx, str("🤡💩🤡ユ🤡"), json("8"),
                              &json("4"), &json("1"), &json("1")),
               "💩🤡ユ");
  expectResult(substringBytes(ctx, str("abcx🤡ユf🤡"), json("4"),
                              &json("4"), &json("3"), &json("2")),
               "bcx🤡ユf");
}

TEST_F(StringFunctionTest,
       substring_bytes_left_without_right_extends_both_sides) {
  expectResult(substringBytes(ctx, str("abcde"), json("2"), &json("1"),
                              &json("1"), nullptr),
               "bcd");
}

TEST_F(StringFunctionTest,
       substring_bytes_extension_stops_at_the_string_bounds) {
  expectResult(substringBytes(ctx, str("abcde"), json("2"), &json("1"),
                              &json("10"), nullptr),
               "abcde");
}

TEST_F(StringFunctionTest,
       substring_bytes_negative_or_null_extension_counts_as_zero) {
  expectResult(substringBytes(ctx, str("abc"), json("1"), &json("1"),
                              &json("-1"), nullptr),
               "b");
  expectResult(substringBytes(ctx, str("abc"), json("1"), &json("1"),
                              &json("null"), &json("1")),
               "bc");
}

}  // namespace arangodb::tests::aql
