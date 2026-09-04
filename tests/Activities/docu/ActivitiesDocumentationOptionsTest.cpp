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
////////////////////////////////////////////////////////////////////////////////

#include "options.h"
#include <gtest/gtest.h>

#include <string_view>
#include <variant>
#include <vector>

namespace {

using namespace std::string_view_literals;

TEST(ActivityDocumentationOptionsTest, parses_long_build_path_and_sources) {
  EXPECT_EQ(options::parse({"--build-path"sv, "build"sv, "arangod"sv, "lib"sv}),
            options::ParseResult(options::Options{
                .build_path = "build", .source_paths = {"arangod", "lib"}}));
}

TEST(ActivityDocumentationOptionsTest, parses_attached_long_build_path) {
  EXPECT_EQ(
      options::parse({"--build-path=build"sv, "arangod"sv}),
      options::ParseResult(options::Options{.build_path = "build",
                                            .source_paths = {"arangod"}}));
}

TEST(ActivityDocumentationOptionsTest, parses_short_build_path) {
  EXPECT_EQ(
      options::parse({"-p"sv, "build"sv, "arangod"sv}),
      options::ParseResult(options::Options{.build_path = "build",
                                            .source_paths = {"arangod"}}));
}

TEST(ActivityDocumentationOptionsTest, accepts_build_path_after_sources) {
  EXPECT_EQ(
      options::parse({"arangod"sv, "lib"sv, "-p"sv, "build"sv}),
      options::ParseResult(options::Options{
          .build_path = "build", .source_paths = {"arangod", "lib"}}));
}

TEST(ActivityDocumentationOptionsTest, help_short_flag_requests_help) {
  EXPECT_EQ(options::parse({"-h"sv}),
            options::ParseResult(options::HelpRequested{}));
}

TEST(ActivityDocumentationOptionsTest, help_long_flag_requests_help) {
  EXPECT_EQ(options::parse({"--help"sv, "-p"sv, "build"sv}),
            options::ParseResult(options::HelpRequested{}));
}

TEST(ActivityDocumentationOptionsTest, errors_on_missing_build_path) {
  EXPECT_TRUE(
      std::holds_alternative<options::Error>(options::parse({"arangod"sv})));
}

TEST(ActivityDocumentationOptionsTest, errors_on_valueless_build_path) {
  EXPECT_TRUE(std::holds_alternative<options::Error>(
      options::parse({"arangod"sv, "--build-path"sv})));
}

TEST(ActivityDocumentationOptionsTest, errors_on_repeated_build_path) {
  EXPECT_TRUE(std::holds_alternative<options::Error>(
      options::parse({"-p"sv, "build"sv, "-p"sv, "other"sv, "arangod"sv})));
}

TEST(ActivityDocumentationOptionsTest, errors_on_missing_source_path) {
  EXPECT_TRUE(std::holds_alternative<options::Error>(
      options::parse({"-p"sv, "build"sv})));
}

TEST(ActivityDocumentationOptionsTest, errors_on_unknown_option) {
  EXPECT_TRUE(std::holds_alternative<options::Error>(
      options::parse({"-p"sv, "build"sv, "--nonsense"sv, "arangod"sv})));
}

}  // namespace
