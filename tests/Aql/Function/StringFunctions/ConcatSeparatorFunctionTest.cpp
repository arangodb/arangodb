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

using ConcatSeparatorFunctionTest = StringFunctionTest;
using arangodb::aql::functions::concatSeparator;

TEST_F(ConcatSeparatorFunctionTest, values_are_joined_with_separator) {
  expectResult(concatSeparator(ctx, str("-"), values(R"(["a"])")), "a");
  expectResult(concatSeparator(ctx, str(", "), values(R"(["a", "b", "c"])")),
               "a, b, c");
}

TEST_F(ConcatSeparatorFunctionTest, null_values_are_skipped) {
  expectResult(
      concatSeparator(ctx, str("-"), values(R"([null, "a", null, "b", null])")),
      "a-b");
  expectResult(concatSeparator(ctx, str("-"), values(R"([null])")), "");
  expectResult(concatSeparator(ctx, str("-"), values(R"([null, null])")), "");
}

TEST_F(ConcatSeparatorFunctionTest, empty_strings_are_kept) {
  expectResult(concatSeparator(ctx, str("-"), values(R"(["", "a", ""])")),
               "-a-");
  expectResult(concatSeparator(ctx, str("-"), values(R"(["", ""])")), "-");
}

TEST_F(ConcatSeparatorFunctionTest, single_array_argument_is_joined) {
  expectResult(concatSeparator(ctx, str("-"), values(R"([["a", "b", "c"]])")),
               "a-b-c");
  expectResult(
      concatSeparator(ctx, str("-"), values(R"([["a", null, "b", null]])")),
      "a-b");
  expectResult(concatSeparator(ctx, str("-"), values(R"([["", ""]])")), "-");
  expectResult(concatSeparator(ctx, str("-"), values(R"([[]])")), "");
  expectResult(concatSeparator(ctx, str("-"), values(R"([[null]])")), "");
}

TEST_F(ConcatSeparatorFunctionTest, nested_values_in_array_are_stringified) {
  expectResult(concatSeparator(ctx, str("-"),
                               values(R"([[1, true, [1, 2], {"a": 1}]])")),
               R"(1-true-[1,2]-{"a":1})");
}

TEST_F(ConcatSeparatorFunctionTest,
       arrays_among_several_arguments_are_stringified) {
  expectResult(concatSeparator(ctx, str("-"), values(R"([["a", "b"], "c"])")),
               R"(["a","b"]-c)");
  expectResult(concatSeparator(ctx, str("-"), values(R"(["c", [null]])")),
               "c-[null]");
}

TEST_F(ConcatSeparatorFunctionTest, non_string_values_are_stringified) {
  expectResult(
      concatSeparator(ctx, str("-"), values(R"([1, -2, 1.5, true, false])")),
      "1--2-1.5-true-false");
  expectResult(concatSeparator(ctx, str("-"), values(R"([{}, {"a": "b"}])")),
               R"({}-{"a":"b"})");
}

TEST_F(ConcatSeparatorFunctionTest, non_string_separators_are_stringified) {
  expectResult(concatSeparator(ctx, json("1"), values(R"(["a", "b"])")), "a1b");
  expectResult(concatSeparator(ctx, json("true"), values(R"(["a", "b"])")),
               "atrueb");
  expectResult(concatSeparator(ctx, json("null"), values(R"(["a", "b"])")),
               "ab");
  expectResult(
      concatSeparator(ctx, json(R"([","])"), values(R"([["a", "b"]])")),
      R"(a[","]b)");
}

TEST_F(ConcatSeparatorFunctionTest, multibyte_strings_are_joined_unchanged) {
  expectResult(concatSeparator(ctx, str("€"), values(R"(["ö", "😀"])")), "ö€😀");
}

}  // namespace arangodb::tests::aql
