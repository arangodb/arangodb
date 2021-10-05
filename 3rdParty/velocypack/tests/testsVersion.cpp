////////////////////////////////////////////////////////////////////////////////
/// @brief Library to build up VPack documents.
///
/// DISCLAIMER
///
/// Copyright 2015 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
/// @author Jan Steemann
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "tests-common.h"

TEST(VersionTest, TestCompare) {
  ASSERT_EQ(0, Version::compare(Version(1, 0, 0), Version(1, 0, 0)));
  ASSERT_EQ(0, Version::compare(Version(1, 2, 3), Version(1, 2, 3)));
  ASSERT_EQ(0, Version::compare(Version(0, 0, 1), Version(0, 0, 1)));

  ASSERT_EQ(1, Version::compare(Version(2, 0, 0), Version(1, 0, 0)));
  ASSERT_EQ(1, Version::compare(Version(1, 1, 0), Version(1, 0, 0)));
  ASSERT_EQ(1, Version::compare(Version(1, 1, 0), Version(1, 0, 1)));
  ASSERT_EQ(1, Version::compare(Version(1, 0, 1), Version(1, 0, 0)));
  ASSERT_EQ(1, Version::compare(Version(1, 1, 1), Version(1, 0, 0)));
  ASSERT_EQ(1, Version::compare(Version(1, 1, 1), Version(1, 0, 1)));
  ASSERT_EQ(1, Version::compare(Version(1, 1, 1), Version(1, 1, 0)));

  ASSERT_EQ(-1, Version::compare(Version(1, 0, 0), Version(2, 0, 0)));
  ASSERT_EQ(-1, Version::compare(Version(1, 0, 0), Version(1, 1, 0)));
  ASSERT_EQ(-1, Version::compare(Version(1, 0, 1), Version(1, 1, 0)));
  ASSERT_EQ(-1, Version::compare(Version(1, 0, 0), Version(1, 0, 1)));
  ASSERT_EQ(-1, Version::compare(Version(1, 0, 0), Version(1, 1, 1)));
  ASSERT_EQ(-1, Version::compare(Version(1, 0, 1), Version(1, 1, 1)));
  ASSERT_EQ(-1, Version::compare(Version(1, 1, 0), Version(1, 1, 1)));
}

TEST(VersionTest, TestDigits) {
  int major = Version::BuildVersion.majorValue;
  ASSERT_TRUE(major >= 0 && major <= 10);
  int minor = Version::BuildVersion.minorValue;
  ASSERT_TRUE(minor >= 0 && minor <= 10);
  int patch = Version::BuildVersion.patchValue;
  ASSERT_TRUE(patch >= 0 && patch <= 999);
}

TEST(VersionTest, TestFormat) {
  std::string version = Version::BuildVersion.toString();

  int majors = 0;
  int minors = 0;
  int patch = 0;

  char const* p = version.c_str();
  while (*p && *p >= '0' && *p <= '9') {
    majors++;
    ++p;
  }
  ASSERT_TRUE(majors > 0);
  ASSERT_EQ('.', *p);
  ++p;

  while (*p && *p >= '0' && *p <= '9') {
    minors++;
    ++p;
  }
  ASSERT_TRUE(minors > 0);
  ASSERT_EQ('.', *p);
  ++p;

  while (*p && *p >= '0' && *p <= '9') {
    patch++;
    ++p;
  }
  ASSERT_TRUE(patch > 0);
  ASSERT_EQ('\0', *p);
}

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
