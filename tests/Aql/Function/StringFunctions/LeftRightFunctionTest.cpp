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

using arangodb::aql::functions::left;
using arangodb::aql::functions::right;

TEST_F(StringFunctionTest, returns_the_first_characters) {
  expectResult(left(ctx, str("cpp"), json("2")), "cp");
  expectResult(left(ctx, str("foo"), json("1")), "f");
  expectResult(left(ctx, str("foo"), json("0")), "");
}

TEST_F(StringFunctionTest, length_beyond_the_string_returns_all_of_it) {
  expectResult(left(ctx, str("foo"), json("4")), "foo");
  expectResult(left(ctx, str("foo"), json("999999999")), "foo");
  expectResult(left(ctx, str(""), json("2")), "");
}

TEST_F(StringFunctionTest, length_counts_characters_not_bytes) {
  expectResult(left(ctx, str("mötör"), json("2")), "mö");
  expectResult(left(ctx, str("mötör"), json("4")), "mötö");
  expectResult(left(ctx, str("😰2🤓4🤡6😎8"), json("3")), "😰2🤓");
}

TEST_F(StringFunctionTest, non_string_values_are_stringified) {
  expectResult(left(ctx, json("null"), json("2")), "");
  expectResult(left(ctx, json("true"), json("2")), "tr");
  expectResult(left(ctx, json("4"), json("2")), "4");
  expectResult(left(ctx, json("[]"), json("1")), "[");
  expectResult(left(ctx, json("{}"), json("1")), "{");
}

TEST_F(StringFunctionTest, length_is_converted_to_an_integer) {
  expectResult(left(ctx, str("foo"), json("true")), "f");
  expectResult(left(ctx, str("foo"), str("2")), "fo");
  expectResult(left(ctx, str("foo"), json("1.9")), "f");
  expectResult(left(ctx, str("foo"), json("[1]")), "f");
}

TEST_F(StringFunctionTest, non_numeric_length_counts_as_zero) {
  for (auto length : {R"(null)", R"("bar")", R"([])", R"({})"}) {
    expectResult(left(ctx, str("foo"), json(length)), "");
  }
}

TEST_F(StringFunctionTest, negative_length_gives_empty_string) {
  expectResult(left(ctx, str("foo"), json("-1")), "");
  expectResult(left(ctx, str("foo"), json("-1.5")), "");
}

TEST_F(RightFunctionTest, returns_the_last_characters) {
  expectResult(right(ctx, str("ppc"), json("2")), "pc");
  expectResult(right(ctx, str("foo"), json("1")), "o");
  expectResult(right(ctx, str("foo"), json("0")), "");
}

TEST_F(RightFunctionTest, length_beyond_the_string_returns_all_of_it) {
  expectResult(right(ctx, str("foo"), json("4")), "foo");
  expectResult(right(ctx, str("foo"), json("999999999")), "foo");
  expectResult(right(ctx, str(""), json("2")), "");
}

TEST_F(RightFunctionTest, length_counts_characters_not_bytes) {
  expectResult(right(ctx, str("mötör"), json("2")), "ör");
  expectResult(right(ctx, str("mötör"), json("4")), "ötör");
  expectResult(right(ctx, str("😰2🤓4🤡6😎8"), json("3")), "6😎8");
}

TEST_F(RightFunctionTest, non_string_values_are_stringified) {
  expectResult(right(ctx, json("null"), json("2")), "");
  expectResult(right(ctx, json("true"), json("2")), "ue");
  expectResult(right(ctx, json("4"), json("2")), "4");
  expectResult(right(ctx, json("[]"), json("1")), "]");
  expectResult(right(ctx, json("{}"), json("1")), "}");
}

TEST_F(RightFunctionTest, length_is_converted_to_an_integer) {
  expectResult(right(ctx, str("foo"), json("true")), "o");
  expectResult(right(ctx, str("foo"), str("2")), "oo");
  expectResult(right(ctx, str("foo"), json("1.9")), "o");
  expectResult(right(ctx, str("foo"), json("[1]")), "o");
}

TEST_F(RightFunctionTest, non_numeric_length_counts_as_zero) {
  for (auto length : {R"(null)", R"("bar")", R"([])", R"({})"}) {
    expectResult(right(ctx, str("foo"), json(length)), "");
  }
}

TEST_F(RightFunctionTest, negative_length_gives_empty_string) {
  expectResult(right(ctx, str("foo"), json("-1")), "");
  expectResult(right(ctx, str("foo"), json("-1.5")), "");
}

}  // namespace arangodb::tests::aql
