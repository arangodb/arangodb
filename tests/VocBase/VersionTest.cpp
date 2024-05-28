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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "VocBase/Methods/Version.h"

#include "gtest/gtest.h"

using arangodb::methods::Version;

TEST(VocbaseMethodsTest, version) {
  EXPECT_EQ(Version::parseVersion("0"), 0);
  EXPECT_EQ(Version::parseVersion("0.0"), 0);
  EXPECT_EQ(Version::parseVersion("0.0.0"), 0);
  EXPECT_EQ(Version::parseVersion("99.99.99"), 999999);

  EXPECT_EQ(Version::parseVersion("1.0.0"), 10000);
  EXPECT_EQ(Version::parseVersion("2.3.4"), 20304);
  EXPECT_EQ(Version::parseVersion("12.34.56"), 123456);
  EXPECT_EQ(Version::parseVersion("3.4.0"), 30400);
  EXPECT_EQ(Version::parseVersion("3.4.devel"), 30400);
  EXPECT_EQ(Version::parseVersion("4.0.0"), 40000);
  EXPECT_EQ(Version::parseVersion("4.0.devel"), 40000);

  EXPECT_EQ(Version::parseVersion("12.3.4"), 120304);
  EXPECT_EQ(Version::parseVersion("1.23.4"), 12304);
  EXPECT_EQ(Version::parseVersion("1.2.34"), 10234);
}
