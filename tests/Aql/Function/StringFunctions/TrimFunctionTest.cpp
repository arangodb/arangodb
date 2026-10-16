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

using arangodb::aql::functions::ltrim;
using arangodb::aql::functions::rtrim;
using arangodb::aql::functions::trim;

TEST_F(StringFunctionTest, trim_removes_default_whitespace_from_both_ends) {
  expectResult(trim(ctx, str("  foo  "), nullptr), "foo");
  expectResult(trim(ctx, str("\r\n\t foo \t\n\r"), nullptr), "foo");
  expectResult(trim(ctx, str(" a b "), nullptr), "a b");
}

TEST_F(StringFunctionTest, trim_default_whitespace_is_space_tab_and_newlines) {
  expectResult(trim(ctx, str("\v\f\xc2\xa0x\xc2\xa0\f\v"), nullptr),
               "\v\f\xc2\xa0x\xc2\xa0\f\v");
}

TEST_F(StringFunctionTest, trim_whitespace_only_gives_empty_string) {
  expectResult(trim(ctx, str(""), nullptr), "");
  expectResult(trim(ctx, str(" \t\n\r "), nullptr), "");
}

TEST_F(StringFunctionTest, trim_type_selects_the_side) {
  expectResult(trim(ctx, str("  foo  "), &json("0")), "foo");
  expectResult(trim(ctx, str("  foo  "), &json("1")), "foo  ");
  expectResult(trim(ctx, str("  foo  "), &json("2")), "  foo");
}

TEST_F(StringFunctionTest, trim_type_is_truncated_to_an_integer) {
  expectResult(trim(ctx, str("  foo  "), &json("1.9")), "foo  ");
}

TEST_F(StringFunctionTest, trim_out_of_range_type_trims_both_sides) {
  expectResult(trim(ctx, str("  foo  "), &json("3")), "foo");
  expectResult(trim(ctx, str("  foo  "), &json("-1")), "foo");
}

TEST_F(StringFunctionTest, trim_string_argument_is_the_set_of_characters) {
  expectResult(trim(ctx, str("11x11"), &str("1")), "x");
  expectResult(trim(ctx, str(",;x;,"), &str(";,")), "x");
  expectResult(trim(ctx, str("  ,x,  "), &str(",")), "  ,x,  ");
  expectResult(trim(ctx, str("öäxäö"), &str("äö")), "x");
}

TEST_F(StringFunctionTest, trim_empty_characters_trim_nothing) {
  expectResult(trim(ctx, str(" x "), &str("")), " x ");
}

TEST_F(StringFunctionTest, trim_other_arguments_use_default_whitespace) {
  for (auto arg : {R"(null)", R"(true)", R"([])", R"({})"}) {
    expectResult(trim(ctx, str(" x "), &json(arg)), "x");
  }
}

TEST_F(StringFunctionTest, trim_non_string_values_are_stringified) {
  expectResult(trim(ctx, json("null"), nullptr), "");
  expectResult(trim(ctx, json("1"), nullptr), "1");
  expectResult(trim(ctx, json("[1]"), &str("[]")), "1");
}

TEST_F(StringFunctionTest, ltrim_removes_default_whitespace_from_the_start) {
  expectResult(ltrim(ctx, str("\r\n\t foo \t\n\r"), nullptr), "foo \t\n\r");
  expectResult(ltrim(ctx, str(" \t\n\r "), nullptr), "");
  expectResult(ltrim(ctx, str(""), nullptr), "");
}

TEST_F(StringFunctionTest, ltrim_characters_are_removed_from_the_start) {
  expectResult(ltrim(ctx, str(",\n,x,\n"), &str(",\n")), "x,\n");
  expectResult(ltrim(ctx, str("öäxäö"), &str("äö")), "xäö");
  expectResult(ltrim(ctx, str("😀😀a😀"), &str("😀")), "a😀");
}

TEST_F(StringFunctionTest, ltrim_empty_characters_trim_nothing) {
  expectResult(ltrim(ctx, str(" x"), &str("")), " x");
}

TEST_F(StringFunctionTest, ltrim_non_string_characters_are_stringified) {
  expectResult(ltrim(ctx, str("11x"), &json("1")), "x");
  expectResult(ltrim(ctx, str("truex"), &json("true")), "x");
  expectResult(ltrim(ctx, str(" x"), &json("null")), " x");
}

TEST_F(StringFunctionTest, ltrim_non_string_values_are_stringified) {
  expectResult(ltrim(ctx, json("null"), nullptr), "");
  expectResult(ltrim(ctx, json("[1]"), &str("[")), "1]");
}

TEST_F(StringFunctionTest, rtrim_removes_default_whitespace_from_the_end) {
  expectResult(rtrim(ctx, str("\r\n\t foo \t\n\r"), nullptr), "\r\n\t foo");
  expectResult(rtrim(ctx, str(" \t\n\r "), nullptr), "");
  expectResult(rtrim(ctx, str(""), nullptr), "");
}

TEST_F(StringFunctionTest, rtrim_characters_are_removed_from_the_end) {
  expectResult(rtrim(ctx, str(",\n,x,\n"), &str(",\n")), ",\n,x");
  expectResult(rtrim(ctx, str("öäxäö"), &str("äö")), "öäx");
  expectResult(rtrim(ctx, str("x"), &str("x")), "");
}

TEST_F(StringFunctionTest, rtrim_empty_characters_trim_nothing) {
  expectResult(rtrim(ctx, str("x "), &str("")), "x ");
}

TEST_F(StringFunctionTest, rtrim_non_string_characters_are_stringified) {
  expectResult(rtrim(ctx, str("x11"), &json("1")), "x");
  expectResult(rtrim(ctx, str("xtrue"), &json("true")), "x");
  expectResult(rtrim(ctx, str("x "), &json("null")), "x ");
}

TEST_F(StringFunctionTest, rtrim_non_string_values_are_stringified) {
  expectResult(rtrim(ctx, json("null"), nullptr), "");
  expectResult(rtrim(ctx, json("[1]"), &str("]")), "[1");
}

}  // namespace arangodb::tests::aql
