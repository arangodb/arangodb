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

using arangodb::aql::functions::concat;

TEST_F(StringFunctionTest, concat_joins_all_arguments) {
  expectResult(concat(ctx, values(R"(["the", "Quick", "", "Brown"])")),
               "theQuickBrown");
  expectResult(concat(ctx, values(R"(["a"])")), "a");
  expectResult(concat(ctx, values(R"(["アボカド", "名称"])")), "アボカド名称");
}

TEST_F(StringFunctionTest, concat_skips_null_arguments) {
  expectResult(concat(ctx, values(R"([null, "a", null, "b"])")), "ab");
  expectResult(concat(ctx, values("[null]")), "");
}

TEST_F(StringFunctionTest, concat_stringifies_non_string_arguments) {
  expectResult(concat(ctx, values(R"(["yes", true, 4, false])")),
               "yestrue4false");
  expectResult(concat(ctx, values(R"(["yes", [], {}])")), "yes[]{}");
  expectResult(concat(ctx, values(R"([[1, "Quick"], "", null, ["Brown"]])")),
               R"([1,"Quick"]["Brown"])");
  expectResult(concat(ctx, values(R"([{"a": "foo", "b": 2}, "yes"])")),
               R"({"a":"foo","b":2}yes)");
}

TEST_F(StringFunctionTest, concat_single_array_argument_joins_its_members) {
  expectResult(concat(ctx, values(R"([["the", "Quick", "", null, "Fox"]])")),
               "theQuickFox");
  expectResult(concat(ctx, values("[[]]")), "");
  expectResult(concat(ctx, values("[[null]]")), "");
  expectResult(concat(ctx, values(R"([[{}, true, 1.5]])")), "{}true1.5");
}

TEST_F(StringFunctionTest, concat_flattens_only_one_array_level) {
  expectResult(concat(ctx, values(R"([[[1, 2], null, [null]]])")),
               "[1,2][null]");
  expectResult(concat(ctx, values(R"([[["a"]]])")), R"(["a"])");
}

TEST_F(StringFunctionTest, concat_array_among_other_arguments_is_stringified) {
  expectResult(concat(ctx, values(R"([["a"], "b"])")), R"(["a"]b)");
  expectResult(concat(ctx, values(R"(["a", ["b", null]])")), R"(a["b",null])");
}

TEST_F(StringFunctionTest, concat_single_range_joins_its_members) {
  AqlValue range(1, 3);
  std::vector<AqlValue> single{range};
  expectResult(concat(ctx, single), "123");
  std::vector<AqlValue> withText{range, str("a")};
  expectResult(concat(ctx, withText), "[1,2,3]a");
}

TEST_F(StringFunctionTest, concat_numbers_use_velocypack_formatting) {
  expectResult(concat(ctx, values(R"([1.0, 0.1, 1e-7, 9007199254740992])")),
               "10.11e-79007199254740992");
  expectResult(concat(ctx, values("[1e16]")), "10000000000000000.0");
}

TEST_F(StringFunctionTest, concat_keeps_invalid_utf8_bytes) {
  std::vector<AqlValue> arguments{str("\xff"), str("a")};
  expectResult(concat(ctx, arguments),
               "\xff"
               "a");
}

}  // namespace arangodb::tests::aql
