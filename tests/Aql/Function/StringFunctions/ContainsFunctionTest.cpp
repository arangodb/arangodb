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

using arangodb::aql::functions::contains;

TEST_F(StringFunctionTest, contains_returns_whether_search_is_found) {
  expectBool(contains(ctx, str("test123"), str("123"), nullptr), true);
  expectBool(contains(ctx, str("test123"), str("t1"), nullptr), true);
  expectBool(contains(ctx, str("test123"), str("x"), nullptr), false);
  expectBool(contains(ctx, str("test123"), str("123"), &json("false")), true);
}

TEST_F(StringFunctionTest, contains_returns_position_when_asked_for_index) {
  expectResult(contains(ctx, str("test123"), str("t"), &json("true")), 0);
  expectResult(contains(ctx, str("test123"), str("123"), &json("true")), 4);
  expectResult(contains(ctx, str("test123"), str("x"), &json("true")), -1);
}

TEST_F(StringFunctionTest, contains_empty_search_matches_at_start) {
  expectBool(contains(ctx, str("test123"), str(""), nullptr), true);
  expectResult(contains(ctx, str("test123"), str(""), &json("true")), 0);
  expectBool(contains(ctx, str(""), str(""), nullptr), true);
  expectResult(contains(ctx, str(""), str(""), &json("true")), 0);
  expectBool(contains(ctx, str(""), str("a"), nullptr), false);
}

TEST_F(StringFunctionTest, contains_non_string_arguments_are_stringified) {
  expectBool(contains(ctx, json("null"), json("null"), nullptr), true);
  expectBool(contains(ctx, json("3"), json("null"), nullptr), true);
  expectResult(contains(ctx, json("12345"), json("34"), &json("true")), 2);
  expectResult(contains(ctx, json("[1,2]"), str("2"), &json("true")), 3);
  expectBool(contains(ctx, json("true"), str("ru"), nullptr), true);
}

TEST_F(StringFunctionTest, contains_index_counts_utf16_code_units) {
  expectResult(contains(ctx, str("äb"), str("b"), &json("true")), 1);
  expectResult(contains(ctx, str("äöü"), str("ü"), &json("true")), 2);
  expectResult(contains(ctx, str("€x"), str("x"), &json("true")), 1);
  expectResult(contains(ctx, str("a€b€"), str("€"), &json("true")), 1);
  expectResult(contains(ctx, str("😀x"), str("x"), &json("true")), 2);
  expectResult(contains(ctx, str("😀😀x"), str("x"), &json("true")), 4);
  expectResult(contains(ctx, str("😀😀x"), str("😀"), &json("true")), 0);
}

TEST_F(StringFunctionTest, contains_matches_canonically_equivalent_characters) {
  expectResult(contains(ctx, str("caf\u00e9"), str("e\u0301"), &json("true")),
               3);
  expectResult(contains(ctx, str("cafe\u0301"), str("\u00e9"), &json("true")),
               3);
  expectResult(contains(ctx, str("cafe\u0301"), str("e"), &json("true")), -1);
}

}  // namespace arangodb::tests::aql
