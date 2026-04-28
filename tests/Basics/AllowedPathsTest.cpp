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
#include "gtest/gtest.h"
#include "Basics/AllowedPaths.h"

TEST(AllowedPathsTest, default_does_not_allow_any) {
  auto testee = arangodb::AllowedPaths();

  ASSERT_FALSE(testee.isAllowed(""));
  ASSERT_FALSE(testee.isAllowed("foo"));
  ASSERT_FALSE(testee.isAllowed("///blubb"));
  ASSERT_FALSE(testee.isAllowed("irgend string"));
  ASSERT_FALSE(testee.isAllowed("14 20 🙄"));
}

TEST(AllowedPathsTest, allowing_allows) {
  auto testee = arangodb::AllowedPaths();

  testee.addPath("/foo/bar/baz");

  ASSERT_FALSE(testee.isAllowed("/"));
  ASSERT_FALSE(testee.isAllowed("/foo/bar"));
  ASSERT_FALSE(testee.isAllowed("banana"));
  ASSERT_FALSE(testee.isAllowed("/banana"));
  ASSERT_TRUE(testee.isAllowed("/foo/bar/baz"));
  ASSERT_TRUE(testee.isAllowed("/foo/bar/baz/"));
  ASSERT_TRUE(testee.isAllowed("/foo/bar/baz/123.txt"));
  ASSERT_TRUE(testee.isAllowed("/foo/bar/baz/a/b/c/123.txt"));
}

TEST(AllowedPathsTest, regression_test_with_longer_allow) {
  auto testee = arangodb::AllowedPaths();

  testee.addPath("/home/makx-arango/scratch/arangodb-play/js");

  // Crashes if std::mismatch is not called with std::end(snd)
  ASSERT_FALSE(testee.isAllowed("/home/makx-arango/scratch/arangodb-play"));
}
