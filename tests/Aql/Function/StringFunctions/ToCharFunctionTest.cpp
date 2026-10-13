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

constexpr auto kToChar = StringFunctionsName::kToChar;

TEST(ToCharFunctionTest, ascii_code_points_become_one_byte) {
  expectResult(kToChar, R"([0])", "\0"sv);
  expectResult(kToChar, R"([65])", "A");
  expectResult(kToChar, R"([127])", "\x7f");
}

TEST(ToCharFunctionTest, two_byte_code_points_are_utf8_encoded) {
  expectResult(kToChar, R"([128])", "\xc2\x80");
  expectResult(kToChar, R"([246])", "ö");
  expectResult(kToChar, R"([2047])", "\xdf\xbf");
}

TEST(ToCharFunctionTest, three_byte_code_points_are_utf8_encoded) {
  expectResult(kToChar, R"([2048])", "\xe0\xa0\x80");
  expectResult(kToChar, R"([8364])", "€");
  expectResult(kToChar, R"([65535])", "\xef\xbf\xbf");
}

TEST(ToCharFunctionTest, four_byte_code_points_are_utf8_encoded) {
  expectResult(kToChar, R"([65536])", "\xf0\x90\x80\x80");
  expectResult(kToChar, R"([128512])", "😀");
  expectResult(kToChar, R"([1114111])", "\xf4\x8f\xbf\xbf");
}

TEST(ToCharFunctionTest, doubles_are_truncated_to_integers) {
  expectResult(kToChar, R"([65.0])", "A");
  expectResult(kToChar, R"([65.9])", "A");
}

TEST(ToCharFunctionTest, negative_numbers_are_rejected) {
  expectError(kToChar, R"([-1])",
              TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  expectError(kToChar, R"([-1000])",
              TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
}

TEST(ToCharFunctionTest, non_numbers_are_rejected) {
  for (auto args : {R"([null])", R"([true])", R"(["65"])", R"(["piff"])",
                    R"([[65]])", R"([{}])"}) {
    expectError(kToChar, args, TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  }
}

TEST(ToCharFunctionTest, extra_arguments_are_ignored) {
  expectResult(kToChar, R"([65, 66])", "A");
}

}  // namespace arangodb::tests::aql
