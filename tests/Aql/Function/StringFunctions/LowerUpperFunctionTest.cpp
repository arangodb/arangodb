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

using LowerFunctionTest = StringFunctionTest;
using UpperFunctionTest = StringFunctionTest;
using arangodb::aql::functions::lower;
using arangodb::aql::functions::upper;

TEST_F(LowerFunctionTest, ascii_letters_are_lowercased) {
  expectResult(lower(ctx, str("")), "");
  expectResult(lower(ctx, str("THE quick Brown foX JuMpED")),
               "the quick brown fox jumped");
}

TEST_F(LowerFunctionTest, non_letters_are_unchanged) {
  expectResult(lower(ctx, str(R"(0123456789<>|,;.:-_#'+*@!"$&/(){[]}?\)")),
               R"(0123456789<>|,;.:-_#'+*@!"$&/(){[]}?\)");
}

TEST_F(LowerFunctionTest, non_ascii_letters_are_lowercased) {
  expectResult(lower(ctx, str("ÄÖÜß")), "äöüß");
  expectResult(lower(ctx, str("ΑΒΓ")), "αβγ");
  expectResult(lower(ctx, str("АБВ")), "абв");
  expectResult(lower(ctx, str("アボカド名称について")), "アボカド名称について");
}

TEST_F(LowerFunctionTest, final_sigma_is_lowercased_by_position) {
  expectResult(lower(ctx, str("ΟΔΟΣ ΣΟΣ")), "οδος σος");
}

TEST_F(LowerFunctionTest, lowercasing_can_change_the_byte_length) {
  // U+0130 -> U+0069 U+0307, Ohm sign -> omega, Kelvin sign -> k
  expectResult(lower(ctx, str("\xc4\xb0")), "i\xcc\x87");
  expectResult(lower(ctx, str("\xe2\x84\xa6")), "\xcf\x89");
  expectResult(lower(ctx, str("\xe2\x84\xaa")), "k");
}

TEST_F(LowerFunctionTest, null_becomes_empty_string) {
  expectResult(lower(ctx, json("null")), "");
}

TEST_F(LowerFunctionTest, booleans_and_numbers_are_stringified) {
  expectResult(lower(ctx, json("true")), "true");
  expectResult(lower(ctx, json("3")), "3");
  expectResult(lower(ctx, json("1E20")), "1e+20");
}

TEST_F(LowerFunctionTest, arrays_and_objects_are_lowercased_as_json) {
  expectResult(lower(ctx, json("[]")), "[]");
  expectResult(lower(ctx, json(R"([1, "A", true])")), R"([1,"a",true])");
  expectResult(lower(ctx, json("{}")), "{}");
  expectResult(lower(ctx, json(R"({"A": 1, "a": 2, "b": "X"})")),
               R"({"a":1,"a":2,"b":"x"})");
}

TEST_F(UpperFunctionTest, ascii_letters_are_uppercased) {
  expectResult(upper(ctx, str("")), "");
  expectResult(upper(ctx, str("THE quick Brown foX JuMpED")),
               "THE QUICK BROWN FOX JUMPED");
}

TEST_F(UpperFunctionTest, non_letters_are_unchanged) {
  expectResult(upper(ctx, str(R"(0123456789<>|,;.:-_#'+*@!"$&/(){[]}?\)")),
               R"(0123456789<>|,;.:-_#'+*@!"$&/(){[]}?\)");
}

TEST_F(UpperFunctionTest, non_ascii_letters_are_uppercased) {
  expectResult(upper(ctx, str("äöü")), "ÄÖÜ");
  expectResult(upper(ctx, str("αβγς")), "ΑΒΓΣ");
  expectResult(upper(ctx, str("абв")), "АБВ");
  expectResult(upper(ctx, str("アボカド名称について")), "アボカド名称について");
}

TEST_F(UpperFunctionTest, uppercasing_can_change_the_byte_length) {
  // sharp s -> SS, fi ligature -> FI, dotless i -> I
  expectResult(upper(ctx, str("\xc3\x9f")), "SS");
  expectResult(upper(ctx, str("\xef\xac\x81")), "FI");
  expectResult(upper(ctx, str("\xc4\xb1")), "I");
}

TEST_F(UpperFunctionTest, null_becomes_empty_string) {
  expectResult(upper(ctx, json("null")), "");
}

TEST_F(UpperFunctionTest, booleans_and_numbers_are_stringified) {
  expectResult(upper(ctx, json("true")), "TRUE");
  expectResult(upper(ctx, json("3")), "3");
  expectResult(upper(ctx, json("1e20")), "1E+20");
}

TEST_F(UpperFunctionTest, arrays_and_objects_are_uppercased_as_json) {
  expectResult(upper(ctx, json("[]")), "[]");
  expectResult(upper(ctx, json(R"([1, "a", true, null])")),
               R"([1,"A",TRUE,NULL])");
  expectResult(upper(ctx, json("{}")), "{}");
  expectResult(upper(ctx, json(R"({"A": 1, "a": 2, "b": "x"})")),
               R"({"A":1,"A":2,"B":"X"})");
}

}  // namespace arangodb::tests::aql
