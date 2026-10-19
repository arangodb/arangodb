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

using arangodb::aql::functions::charLength;

TEST_F(StringFunctionTest, char_length_ascii_strings_count_one_per_byte) {
  expectResult(charLength(ctx, str("")), 0);
  expectResult(charLength(ctx, str("a")), 1);
  expectResult(charLength(ctx, str("the quick fox")), 13);
}

TEST_F(StringFunctionTest, char_length_multibyte_characters_count_once) {
  expectResult(charLength(ctx, str("äöüÄÖÜß")), 7);
  expectResult(charLength(ctx, str("€")), 1);
  expectResult(charLength(ctx, str("アボカド名称について")), 10);
  expectResult(charLength(ctx, str("🥑😄👻👽")), 4);
}

TEST_F(StringFunctionTest, char_length_null_has_length_zero) {
  expectResult(charLength(ctx, json("null")), 0);
}

TEST_F(StringFunctionTest, char_length_booleans_count_their_names) {
  expectResult(charLength(ctx, json("true")), 4);
  expectResult(charLength(ctx, json("false")), 5);
}

TEST_F(StringFunctionTest, char_length_numbers_count_their_string_form) {
  expectResult(charLength(ctx, json("0")), 1);
  expectResult(charLength(ctx, json("3")), 1);
  expectResult(charLength(ctx, json("-42")), 3);
  expectResult(charLength(ctx, json("3.3")), 3);
  expectResult(charLength(ctx, json("1e20")), 5);
  expectResult(charLength(ctx, json("1.5e-7")), 6);
}

TEST_F(StringFunctionTest,
       char_length_arrays_and_objects_count_their_json_form) {
  expectResult(charLength(ctx, json("[]")), 2);
  expectResult(charLength(ctx, json("[1, 2, 3]")), 7);
  expectResult(charLength(ctx, json("{}")), 2);
  expectResult(charLength(ctx, json(R"({"a": 1})")), 7);
  expectResult(charLength(ctx, json(R"({"a": "foo"})")), 11);
  expectResult(charLength(ctx, json(R"({"a": 1, "b": [true, null]})")), 23);
}

TEST_F(StringFunctionTest,
       char_length_arrays_and_objects_count_bytes_not_characters) {
  expectResult(charLength(ctx, json(R"(["ö"])")), 6);
  expectResult(charLength(ctx, json(R"({"€": 1})")), 9);
}

}  // namespace arangodb::tests::aql
