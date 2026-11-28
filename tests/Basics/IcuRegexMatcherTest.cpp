////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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

#include "Basics/IcuRegexMatcher.h"

#include "Basics/Exceptions.h"
#include "Basics/ICancellationToken.h"
#include "Basics/ResultAssertions.h"
#include "Basics/voc-errors.h"

#include <gtest/gtest.h>

#include <string>

namespace arangodb {
using tests::IsError;
using tests::IsOk;
namespace basics {

namespace {
std::unique_ptr<IcuRegexMatcher> compile(std::string const& pattern) {
  auto m = IcuRegexMatcher::create(pattern);
  EXPECT_NE(m, nullptr) << pattern;
  return m;
}

struct AlwaysKilled final : ICancellationToken {
  bool killed() const override { return true; }
};
}  // namespace

TEST(IcuRegexMatcherTest, invalid_pattern_cannot_be_created) {
  EXPECT_EQ(IcuRegexMatcher::create("["), nullptr);
  EXPECT_EQ(IcuRegexMatcher::create("(a"), nullptr);
}

TEST(IcuRegexMatcherTest, matches_requires_the_whole_value_to_match) {
  auto m = compile("a.c");
  EXPECT_TRUE(m->matches("abc").get());
  EXPECT_FALSE(m->matches("xabc").get());
}

TEST(IcuRegexMatcherTest, values_are_read_as_utf8_with_their_full_length) {
  EXPECT_TRUE(compile("^.$")->matches("ä").get());
  EXPECT_TRUE(compile("b")->find(std::string_view("a\0b", 3)).get());
}

TEST(IcuRegexMatcherTest, groups_of_the_last_find_are_returned) {
  auto m = compile("(a+)(b+)");
  ASSERT_TRUE(m->find("xaabbbx").get());
  ASSERT_EQ(m->groupCount(), 2);
  EXPECT_EQ(m->group(0).get(), "aabbb");
  EXPECT_EQ(m->group(1).get(), "aa");
  EXPECT_EQ(m->group(2).get(), "bbb");
}

TEST(IcuRegexMatcherTest, icu_failures_are_returned_as_icu_error) {
  auto m = compile("(a)");
  EXPECT_TRUE(IsError(m->group(0), TRI_ERROR_ARANGO_ICU_ERROR));  // no find yet

  ASSERT_TRUE(m->find("a").get());
  EXPECT_TRUE(
      IsError(m->group(2), TRI_ERROR_ARANGO_ICU_ERROR));  // no such group
}

TEST(IcuRegexMatcherTest, find_matches_anywhere_in_the_value) {
  auto m = compile("abc");
  EXPECT_TRUE(m->find("xxabcxx").get());

  auto noMatch = m->find("xyz");
  EXPECT_TRUE(IsOk(noMatch));  // no match is a result, not an error
  EXPECT_FALSE(noMatch.get());
}

TEST(IcuRegexMatcherTest, replace_first_or_all_occurrences) {
  auto m = compile("o");
  EXPECT_EQ(m->replace("foo", "0", false).get(), "f0o");
  EXPECT_EQ(m->replace("foo", "0", true).get(), "f00");
}

TEST(IcuRegexMatcherTest, split_returns_the_pieces_between_matches) {
  auto m = compile(",");
  icu_64_64::UnicodeString out[16];
  auto count = m->split(icu_64_64::UnicodeString::fromUTF8("a,b,c"), out, 16);
  ASSERT_EQ(count.get(), 3);

  std::string piece;
  EXPECT_EQ(out[0].toUTF8String(piece), "a");
  piece.clear();
  EXPECT_EQ(out[2].toUTF8String(piece), "c");
}

TEST(IcuRegexMatcherTest, killed_query_stops_a_long_match) {
  AlwaysKilled token;
  auto m = compile("(a+)+b");  // exponential backtracking on the input below
  m->abortWhenKilled(&token);

  try {
    std::ignore = m->find(std::string(30, 'a'));
    FAIL() << "expected the match to be aborted";
  } catch (basics::Exception const& ex) {
    EXPECT_EQ(ex.code(), TRI_ERROR_QUERY_KILLED);
  }
}

TEST(IcuRegexMatcherTest, null_token_means_no_kill_check) {
  auto m = compile("a");
  m->abortWhenKilled(nullptr);
  EXPECT_TRUE(m->find("a").get());
}

TEST(IcuRegexMatcherTest, clone_matches_independently) {
  auto original = compile("(a)|(b)");
  auto copy = original->clone();

  ASSERT_TRUE(original->find("a").get());
  ASSERT_TRUE(copy->find("b").get());
  EXPECT_EQ(original->group(0).get(), "a");  // not changed by the copy
}

}  // namespace basics
}  // namespace arangodb