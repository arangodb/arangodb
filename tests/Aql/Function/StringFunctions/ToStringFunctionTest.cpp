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

using arangodb::aql::functions::toString;

TEST_F(StringFunctionTest, to_string_null_becomes_empty_string) {
  expectResult(toString(ctx, json("null")), "");
}

TEST_F(StringFunctionTest, to_string_booleans_become_their_names) {
  expectResult(toString(ctx, json("true")), "true");
  expectResult(toString(ctx, json("false")), "false");
}

TEST_F(StringFunctionTest, to_string_integers_are_printed_exactly) {
  expectResult(toString(ctx, json("0")), "0");
  expectResult(toString(ctx, json("-1")), "-1");
  expectResult(toString(ctx, json("1152921504606846977")),
               "1152921504606846977");
  expectResult(toString(ctx, json("-1152921504606846977")),
               "-1152921504606846977");
}

TEST_F(StringFunctionTest, to_string_doubles_are_printed_in_shortest_form) {
  expectResult(toString(ctx, json("1.5")), "1.5");
  expectResult(toString(ctx, json("-0.25")), "-0.25");
  // 2^52 has no ".0", 2^53 keeps it, 2^64 cannot be printed exactly
  expectResult(toString(ctx, json("4503599627370496.0")), "4503599627370496");
  expectResult(toString(ctx, json("9007199254740992.0")), "9007199254740992.0");
  expectResult(toString(ctx, json("18446744073709551616.0")),
               "18446744073709552000");
}

TEST_F(StringFunctionTest, to_string_strings_are_returned_unchanged) {
  expectResult(toString(ctx, str("")), "");
  expectResult(toString(ctx, str(" ")), " ");
  expectResult(toString(ctx, str("1 ")), "1 ");
  expectResult(toString(ctx, str("öü/\n")), "öü/\n");
}

TEST_F(StringFunctionTest, to_string_arrays_become_compact_json) {
  expectResult(toString(ctx, json("[]")), "[]");
  expectResult(toString(ctx, json("[0, 1, [1, 2], [[9, 4]]]")),
               "[0,1,[1,2],[[9,4]]]");
  expectResult(toString(ctx, json(R"(["", false, null, {}])")),
               R"(["",false,null,{}])");
}

TEST_F(StringFunctionTest, to_string_objects_become_compact_json) {
  expectResult(toString(ctx, json("{}")), "{}");
  expectResult(toString(ctx, json(R"({"a": true, "b": 0})")),
               R"({"a":true,"b":0})");
  expectResult(toString(ctx, json(R"({"a": {}, "b": []})")),
               R"({"a":{},"b":[]})");
}

TEST_F(StringFunctionTest,
       to_string_nested_strings_keep_unicode_and_slashes_unescaped) {
  expectResult(toString(ctx, json(R"(["ö/ü"])")), R"(["ö/ü"])");
  expectResult(toString(ctx, json(R"({"ö": "a/b"})")), R"({"ö":"a/b"})");
}

TEST_F(StringFunctionTest, to_string_nested_strings_escape_control_characters) {
  expectResult(toString(ctx, json(R"(["a\nb\"c"])")), R"(["a\nb\"c"])");
}

}  // namespace arangodb::tests::aql
