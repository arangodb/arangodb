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

constexpr auto kToString = StringFunctionsName::kToString;

TEST(ToStringFunctionTest, null_becomes_empty_string) {
  expectResult(kToString, R"([null])", "");
}

TEST(ToStringFunctionTest, booleans_become_their_names) {
  expectResult(kToString, R"([true])", "true");
  expectResult(kToString, R"([false])", "false");
}

TEST(ToStringFunctionTest, integers_are_printed_exactly) {
  expectResult(kToString, R"([0])", "0");
  expectResult(kToString, R"([-1])", "-1");
  expectResult(kToString, R"([1152921504606846977])", "1152921504606846977");
  expectResult(kToString, R"([-1152921504606846977])", "-1152921504606846977");
}

TEST(ToStringFunctionTest, doubles_are_printed_in_shortest_form) {
  expectResult(kToString, R"([1.5])", "1.5");
  expectResult(kToString, R"([-0.25])", "-0.25");
  // 2^52 has no ".0", 2^53 keeps it, 2^64 cannot be printed exactly
  expectResult(kToString, R"([4503599627370496.0])", "4503599627370496");
  expectResult(kToString, R"([9007199254740992.0])", "9007199254740992.0");
  expectResult(kToString, R"([18446744073709551616.0])",
               "18446744073709552000");
}

TEST(ToStringFunctionTest, strings_are_returned_unchanged) {
  expectResult(kToString, R"([""])", "");
  expectResult(kToString, R"([" "])", " ");
  expectResult(kToString, R"(["1 "])", "1 ");
  expectResult(kToString, R"(["öü/\n"])", "öü/\n");
}

TEST(ToStringFunctionTest, arrays_become_compact_json) {
  expectResult(kToString, R"([[]])", "[]");
  expectResult(kToString, R"([[0, 1, [1, 2], [[9, 4]]]])",
               "[0,1,[1,2],[[9,4]]]");
  expectResult(kToString, R"([["", false, null, {}]])",
               R"(["",false,null,{}])");
}

TEST(ToStringFunctionTest, objects_become_compact_json) {
  expectResult(kToString, R"([{}])", "{}");
  expectResult(kToString, R"([{"a": true, "b": 0}])", R"({"a":true,"b":0})");
  expectResult(kToString, R"([{"a": {}, "b": []}])", R"({"a":{},"b":[]})");
}

TEST(ToStringFunctionTest, nested_strings_keep_unicode_and_slashes_unescaped) {
  expectResult(kToString, R"([["ö/ü"]])", R"(["ö/ü"])");
  expectResult(kToString, R"([{"ö": "a/b"}])", R"({"ö":"a/b"})");
}

TEST(ToStringFunctionTest, nested_strings_escape_control_characters) {
  expectResult(kToString, R"([["a\nb\"c"]])", R"(["a\nb\"c"])");
}

}  // namespace arangodb::tests::aql
