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

#include "sources.h"
#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#ifndef BUILD_PATH
#error "BUILD_PATH must be defined at compile time (arangodb build directory)"
#endif
#ifndef PROJECT_ROOT
#error "PROJECT_ROOT must be defined at compile time (absolute path to repo)"
#endif

namespace {

/**
 * Whether `files` contains `entry`.
 */
auto contains(std::vector<std::string> const& files, std::string const& entry)
    -> bool {
  return std::ranges::find(files, entry) != files.end();
}

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

TEST(ActivityDocumentationSourcesTest, selects_a_single_source_file) {
  auto const database = database_of(BUILD_PATH);
  ASSERT_NE(database, nullptr);
  auto const files = sources::get_sources(
      *database, {PROJECT_ROOT "/arangod/StorageEngine/TransactionState.cpp"});
  EXPECT_TRUE(contains(
      files, PROJECT_ROOT "/arangod/StorageEngine/TransactionState.cpp"))
      << "got " << files.size() << " files";
}

TEST(ActivityDocumentationSourcesTest, routes_a_header_to_its_sibling_source) {
  auto const database = database_of(BUILD_PATH);
  ASSERT_NE(database, nullptr);
  auto const files = sources::get_sources(
      *database, {PROJECT_ROOT "/arangod/Cluster/ActionBase.h"});
  EXPECT_TRUE(contains(files, PROJECT_ROOT "/arangod/Cluster/ActionBase.cpp"))
      << "got " << files.size() << " files";
}

TEST(ActivityDocumentationSourcesTest, yields_no_files_for_an_unknown_path) {
  auto const database = database_of(BUILD_PATH);
  ASSERT_NE(database, nullptr);
  EXPECT_TRUE(
      sources::get_sources(*database, {PROJECT_ROOT "/does/not/exist.cpp"})
          .empty());
}

TEST(ActivityDocumentationSourcesTest, yields_no_files_for_an_ignored_path) {
  auto const database = database_of(BUILD_PATH);
  ASSERT_NE(database, nullptr);
  EXPECT_TRUE(
      sources::get_sources(*database, {PROJECT_ROOT "/3rdParty/rocksdb"})
          .empty());
}

TEST(ActivityDocumentationSourcesTest, searches_a_directory_recursively) {
  auto const database = database_of(BUILD_PATH);
  ASSERT_NE(database, nullptr);
  auto const files =
      sources::get_sources(*database, {PROJECT_ROOT "/arangod/StorageEngine"});
  EXPECT_TRUE(contains(
      files, PROJECT_ROOT "/arangod/StorageEngine/TransactionState.cpp"))
      << "got " << files.size() << " files";
}

}  // namespace
