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

using namespace std::string_view_literals;

namespace arangodb::tests::aql {

using arangodb::aql::functions::toChar;

TEST_F(StringFunctionTest, to_char_ascii_code_points_become_one_byte) {
  expectResult(toChar(ctx, json("0")), "\0"sv);
  expectResult(toChar(ctx, json("65")), "A");
  expectResult(toChar(ctx, json("127")), "\x7f");
}

TEST_F(StringFunctionTest, to_char_two_byte_code_points_are_utf8_encoded) {
  expectResult(toChar(ctx, json("128")), "\xc2\x80");
  expectResult(toChar(ctx, json("246")), "ö");
  expectResult(toChar(ctx, json("2047")), "\xdf\xbf");
}

TEST_F(StringFunctionTest, to_char_three_byte_code_points_are_utf8_encoded) {
  expectResult(toChar(ctx, json("2048")), "\xe0\xa0\x80");
  expectResult(toChar(ctx, json("8364")), "€");
  expectResult(toChar(ctx, json("65535")), "\xef\xbf\xbf");
}

TEST_F(StringFunctionTest, to_char_four_byte_code_points_are_utf8_encoded) {
  expectResult(toChar(ctx, json("65536")), "\xf0\x90\x80\x80");
  expectResult(toChar(ctx, json("128512")), "😀");
  expectResult(toChar(ctx, json("1114111")), "\xf4\x8f\xbf\xbf");
}

TEST_F(StringFunctionTest, to_char_doubles_are_truncated_to_integers) {
  expectResult(toChar(ctx, json("65.0")), "A");
  expectResult(toChar(ctx, json("65.9")), "A");
}

TEST_F(StringFunctionTest, to_char_negative_numbers_are_rejected) {
  expectError(toChar(ctx, json("-1")),
              TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  expectError(toChar(ctx, json("-1000")),
              TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
}

TEST_F(StringFunctionTest, to_char_non_numbers_are_rejected) {
  for (auto arg :
       {R"(null)", R"(true)", R"("65")", R"("piff")", R"([65])", R"({})"}) {
    expectError(toChar(ctx, json(arg)),
                TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  }
}

}  // namespace arangodb::tests::aql
