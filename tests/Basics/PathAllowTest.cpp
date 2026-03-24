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
#include "Basics/PathAllow.h"

TEST(PathAllowTest, default_does_not_allow_any) {
  auto testee = arangodb::PathAllow();

  ASSERT_FALSE(testee.allowed(""));
  ASSERT_FALSE(testee.allowed("foo"));
  ASSERT_FALSE(testee.allowed("///blubb"));
  ASSERT_FALSE(testee.allowed("irgend string"));
  ASSERT_FALSE(testee.allowed("14 20 🙄"));
}

TEST(PathAllowTest, allowing_allows) {
  auto testee = arangodb::PathAllow();

  testee.addPath("/foo/bar/baz");

  ASSERT_FALSE(testee.allowed("/"));
  ASSERT_FALSE(testee.allowed("/foo/bar"));
  ASSERT_FALSE(testee.allowed("banana"));
  ASSERT_FALSE(testee.allowed("/banana"));
  ASSERT_TRUE(testee.allowed("/foo/bar/baz"));
  ASSERT_TRUE(testee.allowed("/foo/bar/baz/"));
  ASSERT_TRUE(testee.allowed("/foo/bar/baz/123.txt"));
  ASSERT_TRUE(testee.allowed("/foo/bar/baz/a/b/c/123.txt"));
}

TEST(PathAllowTest, regression_test_with_longer_allow) {
  auto testee = arangodb::PathAllow();

  testee.addPath("/home/makx-arango/scratch/arangodb-play/js");

  // Crashes if std::mismatch is not called with std::end(snd)
  ASSERT_FALSE(testee.allowed("/home/makx-arango/scratch/arangodb-play"));
}
