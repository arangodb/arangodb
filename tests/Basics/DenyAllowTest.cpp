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
#include "Basics/DenyAllow.h"

TEST(DenyAllowTest, default_denies_all) {
  auto testee = arangodb::DenyAllow();

  ASSERT_EQ(testee.check(""), arangodb::DenyAllowResult::DENIED);
  ASSERT_EQ(testee.check("foo"), arangodb::DenyAllowResult::DENIED);
  ASSERT_EQ(testee.check("///blubb"), arangodb::DenyAllowResult::DENIED);
  ASSERT_EQ(testee.check("irgend string"), arangodb::DenyAllowResult::DENIED);
  ASSERT_EQ(testee.check("14 20 🙄"), arangodb::DenyAllowResult::DENIED);
}

TEST(DenyAllowTest, allowing_allows) {
  auto testee = arangodb::DenyAllow("", "/foo");

  ASSERT_EQ(testee.check(""), arangodb::DenyAllowResult::DENIED);
  ASSERT_EQ(testee.check("/foo"), arangodb::DenyAllowResult::ALLOWED);
}

TEST(DenyAllowTest, denying_denies) {
  auto testee = arangodb::DenyAllow("/foo", ".+");

  ASSERT_EQ(testee.check(""), arangodb::DenyAllowResult::DENIED);
  ASSERT_EQ(testee.check("bar"), arangodb::DenyAllowResult::ALLOWED);
  ASSERT_EQ(testee.check("/foo"), arangodb::DenyAllowResult::DENIED);
}

TEST(DenyAllowTest, deny_overrides_allow) {
  auto testee = arangodb::DenyAllow("/foo", "/foo");

  ASSERT_EQ(testee.check("/foo"), arangodb::DenyAllowResult::DENIED);
}
