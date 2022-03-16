////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022-2022 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>

#include "Rest/PathMatch.h"

#include <string>
#include <string_view>
#include <vector>

using namespace arangodb;
using namespace arangodb::rest;

using namespace std::string_literals;
using namespace std::string_view_literals;

TEST(PathMatch, test_suffixes_string_vector) {
  auto const suffixes = std::vector<std::string>{
      "foo", "foo_val", "bar", "bar_val", "baz", "baz_val"};

  EXPECT_TRUE(Match(suffixes).against("foo", "foo_val", "bar", "bar_val", "baz",
                                      "baz_val"));

  // assign string_views
  {
    std::string_view foo, bar, baz;
    // match against mixed components (string_view, string, const char*)
    EXPECT_TRUE(
        Match(suffixes).against("foo"sv, &foo, "bar"s, &bar, "baz", &baz));
    EXPECT_EQ(foo, "foo_val");
    EXPECT_EQ(bar, "bar_val");
    EXPECT_EQ(baz, "baz_val");
  }
  // assign strings
  {
    std::string foo, bar, baz;
    // match against mixed components (string_view, string, const char*)
    EXPECT_TRUE(
        Match(suffixes).against("foo"sv, &foo, "bar"s, &bar, "baz", &baz));
    EXPECT_EQ(foo, "foo_val");
    EXPECT_EQ(bar, "bar_val");
    EXPECT_EQ(baz, "baz_val");
  }

  EXPECT_FALSE(Match(suffixes).against("foo"sv));
  EXPECT_FALSE(Match(suffixes).against("foo", "quux", "bar", "bar_val", "baz",
                                       "baz_val"));
}

// TODO As soon as we have support for ranges, #include <ranges> and add this
// test. TEST(PathMatch, test_suffixes_string_view_range) {
//   auto const suffixes_vec = std::vector<std::string_view>{
//       "foo", "foo_val", "bar", "bar_val", "baz", "baz_val"};
//   {
//     auto const suffixes = std::views::all(suffixes_vec);
//     EXPECT_TRUE(Match(suffixes).against("foo", "foo_val", "bar", "bar_val",
//                                         "baz", "baz_val"));
//     // assign string_views
//     {
//       std::string_view foo, bar, baz;
//       // match against mixed components (string_view, string, const char*)
//       EXPECT_TRUE(
//           Match(suffixes).against("foo"sv, &foo, "bar"s, &bar, "baz", &baz));
//       EXPECT_EQ(foo, "foo_val");
//       EXPECT_EQ(bar, "bar_val");
//       EXPECT_EQ(baz, "baz_val");
//     }
//     // assign strings
//     {
//       std::string foo, bar, baz;
//       // match against mixed components (string_view, string, const char*)
//       EXPECT_TRUE(
//           Match(suffixes).against("foo"sv, &foo, "bar"s, &bar, "baz", &baz));
//       EXPECT_EQ(foo, "foo_val");
//       EXPECT_EQ(bar, "bar_val");
//       EXPECT_EQ(baz, "baz_val");
//     }
//   }
//   {
//     auto const suffixes = suffixes_vec | std::views::drop(2);
//     EXPECT_TRUE(Match(suffixes).against("bar", "bar_val", "baz", "baz_val"));
//   }
// }
