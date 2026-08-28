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

#include "repository.h"
#include <gtest/gtest.h>

#include <filesystem>
#include <regex>
#include <string>

#ifndef PROJECT_ROOT
#error "PROJECT_ROOT must be defined at compile time (absolute path to repo)"
#endif

TEST(ActivityDocumentationRepositoryTest, finds_short_commit_id_of_repository) {
  auto const commit_id = current_commit_id(PROJECT_ROOT);

  ASSERT_TRUE(commit_id.has_value());
  EXPECT_TRUE(std::regex_match(*commit_id, std::regex{"[0-9a-f]{7,40}"}))
      << "not a short commit id: " << *commit_id;
}

TEST(ActivityDocumentationRepositoryTest, resolves_a_file_to_its_repository) {
  EXPECT_EQ(
      current_commit_id(PROJECT_ROOT "/lib/Activities/docu/src/markdown.cpp"),
      current_commit_id(PROJECT_ROOT));
}

TEST(ActivityDocumentationRepositoryTest, resolves_a_bare_file_name) {
  auto const previous = std::filesystem::current_path();
  std::filesystem::current_path(PROJECT_ROOT "/lib/Activities/docu/src");
  auto const commit_id = current_commit_id("markdown.cpp");
  std::filesystem::current_path(previous);

  EXPECT_EQ(commit_id, current_commit_id(PROJECT_ROOT));
}

TEST(ActivityDocumentationRepositoryTest,
     has_no_commit_id_outside_a_repository) {
  EXPECT_EQ(current_commit_id("/nonexistent-path-for-test"), std::nullopt);
}
