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

using arangodb::aql::functions::findFirst;
using arangodb::aql::functions::findLast;

namespace {
// precomposed U+D55C is 1 UTF-16 unit, its decomposed form 3
constexpr std::string_view kHangulDecomposed = "\u1112\u1161\u11AB";
constexpr std::string_view kMaxInt64 = "9223372036854775807";
}  // namespace

TEST_F(StringFunctionTest, find_first_returns_first_match_position) {
  expectResult(findFirst(ctx, str("foobarbaz"), str("ba"), nullptr, nullptr),
               3);
  expectResult(findFirst(ctx, str("foobarbaz"), str("x"), nullptr, nullptr),
               -1);
  expectResult(findFirst(ctx, str(""), str("a"), nullptr, nullptr), -1);
}

TEST_F(StringFunctionTest, find_first_searches_between_start_and_end) {
  expectResult(findFirst(ctx, str("foobarbaz"), str("ba"), &json("4"), nullptr),
               6);
  expectResult(
      findFirst(ctx, str("foobarbaz"), str("ba"), &json("4"), &json("7")), 6);
  expectResult(
      findFirst(ctx, str("foobarbaz"), str("ba"), &json("4"), &json("6")), -1);
  expectResult(
      findFirst(ctx, str("foobarbaz"), str("ba"), &json("4"), &json("null")),
      6);
  expectResult(findFirst(ctx, str("abc"), str("c"), &json("3"), nullptr), -1);
}

TEST_F(StringFunctionTest, find_first_invalid_range_returns_minus_one) {
  expectResult(findFirst(ctx, str("abc"), str("a"), &json("-1"), nullptr), -1);
  expectResult(findFirst(ctx, str("abc"), str("a"), &json("2"), &json("1")),
               -1);
  expectResult(findFirst(ctx, str("abc"), str("a"), &json("0"), &json("-1")),
               -1);
}

TEST_F(StringFunctionTest, find_first_accepts_largest_end_offset) {
  expectResult(
      findFirst(ctx, str("abc"), str("c"), &json("0"), &json(kMaxInt64)), 2);
  expectResult(
      findFirst(ctx, str("abc"), str("c"), &json("1"), &json(kMaxInt64)), 2);
}

TEST_F(StringFunctionTest, find_first_empty_search_returns_start) {
  expectResult(findFirst(ctx, str("abc"), str(""), nullptr, nullptr), 0);
  expectResult(findFirst(ctx, str("abc"), str(""), &json("2"), nullptr), 2);
  expectResult(findFirst(ctx, str(""), str(""), nullptr, nullptr), 0);
}

TEST_F(StringFunctionTest, find_first_finds_overlapping_matches) {
  expectResult(findFirst(ctx, str("aaa"), str("aa"), &json("1"), nullptr), 1);
  expectResult(findFirst(ctx, str("aaaa"), str("aa"), &json("1"), nullptr), 1);
  expectResult(findFirst(ctx, str("aaaa"), str("aa"), &json("1"), &json("2")),
               1);
  expectResult(findFirst(ctx, str("aaaa"), str("aa"), &json("1"), &json("1")),
               -1);
  expectResult(findFirst(ctx, str("aaaa"), str("aa"), &json("4"), nullptr), -1);
}

TEST_F(StringFunctionTest, find_first_ignores_surrogate_pair_match_past_end) {
  expectResult(findFirst(ctx, str("😀x"), str("😀"), &json("0"), &json("0")), -1);
  expectResult(findFirst(ctx, str("😀x"), str("😀"), &json("0"), &json("1")), 0);
}

TEST_F(StringFunctionTest,
       find_first_ignores_match_starting_before_start_in_surrogate_pair) {
  expectResult(findFirst(ctx, str("😀x"), str("😀"), &json("1"), nullptr), -1);
  expectResult(findFirst(ctx, str("😀😀"), str("😀"), &json("1"), nullptr), 2);
}

TEST_F(StringFunctionTest, find_first_match_can_be_shorter_than_search) {
  expectResult(
      findFirst(ctx, str("한"), str(kHangulDecomposed), nullptr, nullptr), 0);
  expectResult(findFirst(ctx, str("x한y"), str(kHangulDecomposed), &json("0"),
                         &json("1")),
               1);
  expectResult(
      findFirst(ctx, str("caf\u00e9"), str("e\u0301"), &json("0"), &json("3")),
      3);
}

TEST_F(StringFunctionTest, find_last_returns_last_match_position) {
  expectResult(findLast(ctx, str("foobarbaz"), str("ba"), nullptr, nullptr), 6);
  expectResult(findLast(ctx, str("foobarbaz"), str("x"), nullptr, nullptr), -1);
  expectResult(findLast(ctx, str(""), str("a"), nullptr, nullptr), -1);
}

TEST_F(StringFunctionTest, find_last_searches_between_start_and_end) {
  expectResult(
      findLast(ctx, str("foobarbaz"), str("ba"), &json("0"), &json("6")), 3);
  expectResult(findLast(ctx, str("foobarbaz"), str("ba"), &json("4"), nullptr),
               6);
  expectResult(
      findLast(ctx, str("foobarbaz"), str("ba"), &json("4"), &json("6")), -1);
  expectResult(
      findLast(ctx, str("foobarbaz"), str("ba"), &json("0"), &json("null")), 6);
}

TEST_F(StringFunctionTest, find_last_invalid_range_returns_minus_one) {
  expectResult(findLast(ctx, str("abc"), str("a"), &json("-1"), nullptr), -1);
  expectResult(findLast(ctx, str("abc"), str("a"), &json("2"), &json("1")), -1);
  expectResult(findLast(ctx, str("abc"), str("a"), &json("0"), &json("-1")),
               -1);
}

TEST_F(StringFunctionTest, find_last_accepts_largest_end_offset) {
  expectResult(
      findLast(ctx, str("abc"), str("c"), &json("0"), &json(kMaxInt64)), 2);
}

TEST_F(StringFunctionTest, find_last_empty_search_returns_end) {
  expectResult(findLast(ctx, str("abc"), str(""), nullptr, nullptr), 3);
  expectResult(findLast(ctx, str("abc"), str(""), &json("0"), &json("1")), 2);
  expectResult(findLast(ctx, str(""), str(""), nullptr, nullptr), 0);
}

TEST_F(StringFunctionTest, find_last_finds_overlapping_matches) {
  expectResult(findLast(ctx, str("aaa"), str("aa"), nullptr, nullptr), 1);
  expectResult(findLast(ctx, str("aaaa"), str("aa"), nullptr, nullptr), 2);
  expectResult(findLast(ctx, str("aaaa"), str("aa"), &json("0"), &json("2")),
               1);
  expectResult(findLast(ctx, str("aaaa"), str("aa"), &json("2"), nullptr), 2);
  expectResult(findLast(ctx, str("aaaa"), str("aa"), &json("3"), nullptr), -1);
}

TEST_F(StringFunctionTest, find_last_ignores_surrogate_pair_match_past_end) {
  expectResult(findLast(ctx, str("😀x"), str("😀"), &json("0"), &json("0")), -1);
  expectResult(findLast(ctx, str("😀😀"), str("😀"), &json("0"), &json("2")), 0);
}

TEST_F(StringFunctionTest,
       find_last_ignores_match_starting_before_start_in_surrogate_pair) {
  expectResult(findLast(ctx, str("😀x"), str("😀"), &json("1"), nullptr), -1);
  expectResult(findLast(ctx, str("😀😀"), str("😀"), &json("1"), nullptr), 2);
}

TEST_F(StringFunctionTest, find_last_ignores_match_that_only_fits_truncated) {
  // U+1D400 is a surrogate pair, U+FE0F a variation selector
  expectResult(findLast(ctx, str("𝐀𝐀"), str("𝐀"), &json("0"), &json("2")), 0);
  expectResult(findLast(ctx, str("𝐀𝐀"), str("𝐀"), nullptr, nullptr), 2);
  expectResult(
      findLast(ctx, str("a\uFE0Fa\uFE0F"), str("a"), &json("0"), &json("2")),
      0);
  expectResult(findLast(ctx, str("a\uFE0Fa\uFE0F"), str("a"), nullptr, nullptr),
               2);
}

TEST_F(StringFunctionTest, find_last_match_can_be_shorter_than_search) {
  expectResult(
      findLast(ctx, str("한x한"), str(kHangulDecomposed), nullptr, nullptr), 2);
  expectResult(findLast(ctx, str("x한y한"), str(kHangulDecomposed), &json("0"),
                        &json("1")),
               1);
  expectResult(
      findLast(ctx, str("caf\u00e9"), str("e\u0301"), &json("0"), &json("3")),
      3);
}

}  // namespace arangodb::tests::aql
