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

constexpr auto kConcatSeparator = StringFunctionsName::kConcatSeparator;

TEST(ConcatSeparatorFunctionTest, values_are_joined_with_separator) {
  expectResult(kConcatSeparator, R"(["-", "a"])", "a");
  expectResult(kConcatSeparator, R"([", ", "a", "b", "c"])", "a, b, c");
}

TEST(ConcatSeparatorFunctionTest, null_values_are_skipped) {
  expectResult(kConcatSeparator, R"(["-", null, "a", null, "b", null])", "a-b");
  expectResult(kConcatSeparator, R"(["-", null])", "");
  expectResult(kConcatSeparator, R"(["-", null, null])", "");
}

TEST(ConcatSeparatorFunctionTest, empty_strings_are_kept) {
  expectResult(kConcatSeparator, R"(["-", "", "a", ""])", "-a-");
  expectResult(kConcatSeparator, R"(["-", "", ""])", "-");
}

TEST(ConcatSeparatorFunctionTest, single_array_argument_is_joined) {
  expectResult(kConcatSeparator, R"(["-", ["a", "b", "c"]])", "a-b-c");
  expectResult(kConcatSeparator, R"(["-", ["a", null, "b", null]])", "a-b");
  expectResult(kConcatSeparator, R"(["-", ["", ""]])", "-");
  expectResult(kConcatSeparator, R"(["-", []])", "");
  expectResult(kConcatSeparator, R"(["-", [null]])", "");
}

TEST(ConcatSeparatorFunctionTest, nested_values_in_array_are_stringified) {
  expectResult(kConcatSeparator, R"(["-", [1, true, [1, 2], {"a": 1}]])",
               R"(1-true-[1,2]-{"a":1})");
}

TEST(ConcatSeparatorFunctionTest,
     arrays_among_several_arguments_are_stringified) {
  expectResult(kConcatSeparator, R"(["-", ["a", "b"], "c"])", R"(["a","b"]-c)");
  expectResult(kConcatSeparator, R"(["-", "c", [null]])", "c-[null]");
}

TEST(ConcatSeparatorFunctionTest, non_string_values_are_stringified) {
  expectResult(kConcatSeparator, R"(["-", 1, -2, 1.5, true, false])",
               "1--2-1.5-true-false");
  expectResult(kConcatSeparator, R"(["-", {}, {"a": "b"}])", R"({}-{"a":"b"})");
}

TEST(ConcatSeparatorFunctionTest, non_string_separators_are_stringified) {
  expectResult(kConcatSeparator, R"([1, "a", "b"])", "a1b");
  expectResult(kConcatSeparator, R"([true, "a", "b"])", "atrueb");
  expectResult(kConcatSeparator, R"([null, "a", "b"])", "ab");
  expectResult(kConcatSeparator, R"([[","], ["a", "b"]])", R"(a[","]b)");
}

TEST(ConcatSeparatorFunctionTest, multibyte_strings_are_joined_unchanged) {
  expectResult(kConcatSeparator, R"(["€", "ö", "😀"])", "ö€😀");
}

}  // namespace arangodb::tests::aql
