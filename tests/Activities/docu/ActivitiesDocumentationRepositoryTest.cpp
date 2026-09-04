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
#include "git.h"
#include "sources.h"
#include <gtest/gtest.h>

#include <filesystem>
#include <regex>
#include <string>
#include <utility>
#include <variant>

#ifndef PROJECT_ROOT
#error "PROJECT_ROOT must be defined at compile time (absolute path to repo)"
#endif
#ifndef BUILD_PATH
#error "BUILD_PATH must be defined at compile time (arangodb build directory)"
#endif

namespace {

/**
 * Load the compilation database of `build_path`, failing the test on error.
 */
auto database_of(std::string const& build_path) -> sources::Database {
  auto result = sources::get_database(build_path);
  auto* database = std::get_if<sources::Database>(&result);
  EXPECT_NE(database, nullptr);
  if (database == nullptr) {
    return nullptr;
  }
  return std::move(*database);
}

}  // namespace

TEST(ActivityDocumentationRepositoryTest, finds_short_commit_id_of_repository) {
  auto const commit_id = git::current_commit_id(PROJECT_ROOT);

  ASSERT_TRUE(commit_id.has_value());
  EXPECT_TRUE(std::regex_match(*commit_id, std::regex{"[0-9a-f]{7,40}"}))
      << "not a short commit id: " << *commit_id;
}

TEST(ActivityDocumentationRepositoryTest, resolves_a_file_to_its_repository) {
  EXPECT_EQ(git::current_commit_id(PROJECT_ROOT
                                   "/lib/Activities/docu/src/markdown.cpp"),
            git::current_commit_id(PROJECT_ROOT));
}

TEST(ActivityDocumentationRepositoryTest, resolves_a_bare_file_name) {
  auto const previous = std::filesystem::current_path();
  std::filesystem::current_path(PROJECT_ROOT "/lib/Activities/docu/src");
  auto const commit_id = git::current_commit_id("markdown.cpp");
  std::filesystem::current_path(previous);

  EXPECT_EQ(commit_id, git::current_commit_id(PROJECT_ROOT));
}

TEST(ActivityDocumentationRepositoryTest,
     has_no_commit_id_outside_a_repository) {
  EXPECT_EQ(git::current_commit_id("/nonexistent-path-for-test"), std::nullopt);
}

TEST(ActivityDocumentationRepositoryTest, finds_repository_root) {
  EXPECT_EQ(git::repository_root(PROJECT_ROOT), std::string{PROJECT_ROOT});
}

TEST(ActivityDocumentationRepositoryTest,
     resolves_a_file_to_its_repository_root) {
  EXPECT_EQ(git::repository_root(PROJECT_ROOT
                                 "/lib/Activities/docu/src/markdown.cpp"),
            std::string{PROJECT_ROOT});
}

TEST(ActivityDocumentationRepositoryTest,
     has_no_repository_root_outside_a_repository) {
  EXPECT_EQ(git::repository_root("/nonexistent-path-for-test"), std::nullopt);
}

TEST(ActivityDocumentationRepositoryTest,
     records_only_arangodb_commit_for_a_normal_path) {
  auto const database = database_of(BUILD_PATH);
  ASSERT_NE(database, nullptr);
  auto const commits =
      repository::commit_ids(*database, {PROJECT_ROOT "/arangod"});

  ASSERT_EQ(commits.size(), 1u);
  EXPECT_EQ(commits[0].repository, "arangodb");
  EXPECT_EQ(commits[0].id,
            git::current_commit_id(PROJECT_ROOT).value_or("unknown"));
}

TEST(ActivityDocumentationRepositoryTest,
     records_enterprise_commit_for_an_enterprise_path) {
  if (not std::filesystem::exists(PROJECT_ROOT "/enterprise")) {
    GTEST_SKIP() << "no enterprise directory in this checkout";
  }
  auto const database = database_of(BUILD_PATH);
  ASSERT_NE(database, nullptr);
  auto const commits = repository::commit_ids(
      *database,
      {PROJECT_ROOT "/enterprise/Enterprise", PROJECT_ROOT "/lib"});

  ASSERT_EQ(commits.size(), 2u);
  EXPECT_EQ(commits[0].repository, "arangodb");
  EXPECT_EQ(commits[1].repository, "enterprise");
  // the enterprise submodule is checked out at its own commit
  EXPECT_EQ(
      commits[1].id,
      git::current_commit_id(PROJECT_ROOT "/enterprise").value_or("unknown"));
}
