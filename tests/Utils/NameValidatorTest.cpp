////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Julia Puget
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "Utilities/NameValidator.h"

#include <velocypack/StringRef.h>

TEST(NameValidatorTest, test_isSystemName) {
  EXPECT_FALSE(arangodb::NameValidator::isSystemName(""));
  EXPECT_TRUE(arangodb::NameValidator::isSystemName("_"));
  EXPECT_TRUE(arangodb::NameValidator::isSystemName("_abc"));
  EXPECT_FALSE(arangodb::NameValidator::isSystemName("abc"));
  EXPECT_FALSE(arangodb::NameValidator::isSystemName("abc_"));
}

TEST(DatabaseNameValidatorTest, test_isAllowedName_traditionalNames) {
  // direct (non-system)
  std::string const borderline(64, 'x');
  std::string const borderlineSystem(std::string("_") +
                                     std::string(borderline.size() - 1, 'x'));
  std::string const tooLong(borderline.size() + 1, 'x');
  {
    EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
        false, false, arangodb::velocypack::StringRef(nullptr, 0)));
    EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
        false, false, arangodb::velocypack::StringRef("")));
    EXPECT_TRUE(arangodb::DatabaseNameValidator::isAllowedName(
        false, false, arangodb::velocypack::StringRef("abc123")));
    EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
        false, false, arangodb::velocypack::StringRef("123abc")));
    EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
        false, false, arangodb::velocypack::StringRef("123")));
    EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
        false, false, arangodb::velocypack::StringRef("_123")));
    EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
        false, false, arangodb::velocypack::StringRef("_abc")));
    EXPECT_TRUE(arangodb::DatabaseNameValidator::isAllowedName(
        false, false, arangodb::velocypack::StringRef(borderline)));
    EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
        false, false, arangodb::velocypack::StringRef(borderlineSystem)));
    EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
        false, false, arangodb::velocypack::StringRef(tooLong)));
  }

  // direct (system)
  {
    EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
        true, false, arangodb::velocypack::StringRef(nullptr, 0)));
    EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
        true, false, arangodb::velocypack::StringRef("")));
    EXPECT_TRUE(arangodb::DatabaseNameValidator::isAllowedName(
        true, false, arangodb::velocypack::StringRef("abc123")));
    EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
        true, false, arangodb::velocypack::StringRef("123abc")));
    EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
        true, false, arangodb::velocypack::StringRef("123")));
    EXPECT_TRUE(arangodb::DatabaseNameValidator::isAllowedName(
        true, false, arangodb::velocypack::StringRef("_123")));
    EXPECT_TRUE(arangodb::DatabaseNameValidator::isAllowedName(
        true, false, arangodb::velocypack::StringRef("_abc")));
    EXPECT_TRUE(arangodb::DatabaseNameValidator::isAllowedName(
        true, false, arangodb::velocypack::StringRef(borderline)));
    EXPECT_TRUE(arangodb::DatabaseNameValidator::isAllowedName(
        true, false, arangodb::velocypack::StringRef(borderlineSystem)));
    EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
        true, false, arangodb::velocypack::StringRef(tooLong)));
  }

  // special characters
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
      true, false, arangodb::velocypack::StringRef(" a + & ? = abc ")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
      true, false, arangodb::velocypack::StringRef("<script>alert(1);")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
      true, false, arangodb::velocypack::StringRef("a b c")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
      true, false, arangodb::velocypack::StringRef("test123 & ' \" < > abc")));

  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
      true, false, arangodb::velocypack::StringRef("abc:cde")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
      true, false, arangodb::velocypack::StringRef(".abc")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
      true, false, arangodb::velocypack::StringRef("a\0b", 3)));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
      true, false, arangodb::velocypack::StringRef("/")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
      true, false, arangodb::velocypack::StringRef("a/")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
      true, false, arangodb::velocypack::StringRef("a/b")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
      true, false, arangodb::velocypack::StringRef("a\\b")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
      true, false, arangodb::velocypack::StringRef("a.b.c")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
      true, false, arangodb::velocypack::StringRef(" a")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
      true, false, arangodb::velocypack::StringRef("\na")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
      true, false, arangodb::velocypack::StringRef("\ta")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
      true, false, arangodb::velocypack::StringRef("\ra")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
      true, false, arangodb::velocypack::StringRef("\ba")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
      true, false, arangodb::velocypack::StringRef("\fa")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
      true, false, arangodb::velocypack::StringRef("a\n")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
      true, false, arangodb::velocypack::StringRef("a\t")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
      true, false, arangodb::velocypack::StringRef("a\r")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
      true, false, arangodb::velocypack::StringRef("a\b")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
      true, false, arangodb::velocypack::StringRef("a\f")));

  // unicode
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
      true, false, arangodb::velocypack::StringRef("mötör")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
      true, false, arangodb::velocypack::StringRef("😀")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
      true, false, arangodb::velocypack::StringRef("😀 🍺")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
      true, false, arangodb::velocypack::StringRef("maçã")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
      true, false, arangodb::velocypack::StringRef("ﻚﻠﺑ ﻞﻄﻴﻓ")));
}

TEST(DatabaseNameValidatorTest, test_isAllowedName_extendedNames) {
  // direct (non-system)
  std::string const borderline(128, 'x');
  std::string const tooLong(borderline.size() + 1, 'x');
  {
    EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
        false, true, arangodb::velocypack::StringRef(nullptr, 0)));
    EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
        false, true, arangodb::velocypack::StringRef("")));
    EXPECT_TRUE(arangodb::DatabaseNameValidator::isAllowedName(
        false, true, arangodb::velocypack::StringRef("abc123")));
    EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
        false, true, arangodb::velocypack::StringRef("123abc")));
    EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
        false, true, arangodb::velocypack::StringRef("123")));
    EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
        false, true, arangodb::velocypack::StringRef("_123")));
    EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
        false, true, arangodb::velocypack::StringRef("_abc")));
    EXPECT_TRUE(arangodb::DatabaseNameValidator::isAllowedName(
        false, true, arangodb::velocypack::StringRef(borderline)));
    EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
        false, true, arangodb::velocypack::StringRef(tooLong)));
  }

  // direct (system)
  {
    EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
        true, true, arangodb::velocypack::StringRef(nullptr, 0)));
    EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
        true, true, arangodb::velocypack::StringRef("")));
    EXPECT_TRUE(arangodb::DatabaseNameValidator::isAllowedName(
        true, true, arangodb::velocypack::StringRef("abc123")));
    EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
        true, true, arangodb::velocypack::StringRef("123abc")));
    EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
        true, true, arangodb::velocypack::StringRef("123")));
    EXPECT_TRUE(arangodb::DatabaseNameValidator::isAllowedName(
        true, true, arangodb::velocypack::StringRef("_123")));
    EXPECT_TRUE(arangodb::DatabaseNameValidator::isAllowedName(
        true, true, arangodb::velocypack::StringRef("_abc")));
    EXPECT_TRUE(arangodb::DatabaseNameValidator::isAllowedName(
        true, true, arangodb::velocypack::StringRef(borderline)));
    EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
        true, true, arangodb::velocypack::StringRef(tooLong)));
  }

  // special characters
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
      true, true, arangodb::velocypack::StringRef(" a + & ? = abc ")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
      true, true, arangodb::velocypack::StringRef(" a + & ? = abc")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
      true, true, arangodb::velocypack::StringRef("a + & ? = abc ")));
  EXPECT_TRUE(arangodb::DatabaseNameValidator::isAllowedName(
      true, true, arangodb::velocypack::StringRef("a + & ? = abc")));
  EXPECT_TRUE(arangodb::DatabaseNameValidator::isAllowedName(
      true, true, arangodb::velocypack::StringRef("<script>alert(1);")));
  EXPECT_TRUE(arangodb::DatabaseNameValidator::isAllowedName(
      true, true, arangodb::velocypack::StringRef("a b c")));
  EXPECT_TRUE(arangodb::DatabaseNameValidator::isAllowedName(
      true, true, arangodb::velocypack::StringRef("test123 & ' \" < > abc")));

  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
      true, true, arangodb::velocypack::StringRef("abc:cde")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
      true, true, arangodb::velocypack::StringRef(".abc")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
      true, true, arangodb::velocypack::StringRef("a\0b", 3)));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
      true, true, arangodb::velocypack::StringRef("/")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
      true, true, arangodb::velocypack::StringRef("a/")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
      true, true, arangodb::velocypack::StringRef("a/b")));
  EXPECT_TRUE(arangodb::DatabaseNameValidator::isAllowedName(
      true, true, arangodb::velocypack::StringRef("a\\b")));
  EXPECT_TRUE(arangodb::DatabaseNameValidator::isAllowedName(
      true, true, arangodb::velocypack::StringRef("a.b.c")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
      true, true, arangodb::velocypack::StringRef("\na")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
      true, true, arangodb::velocypack::StringRef("\ta")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
      true, true, arangodb::velocypack::StringRef("\ra")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
      true, true, arangodb::velocypack::StringRef("\ba")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
      true, true, arangodb::velocypack::StringRef("\fa")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
      true, true, arangodb::velocypack::StringRef("a\n")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
      true, true, arangodb::velocypack::StringRef("a\t")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
      true, true, arangodb::velocypack::StringRef("a\r")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
      true, true, arangodb::velocypack::StringRef("a\b")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(
      true, true, arangodb::velocypack::StringRef("a\f")));

  // unicode
  EXPECT_TRUE(arangodb::DatabaseNameValidator::isAllowedName(
      true, true, arangodb::velocypack::StringRef("mötör")));
  EXPECT_TRUE(arangodb::DatabaseNameValidator::isAllowedName(
      true, true, arangodb::velocypack::StringRef("😀")));
  EXPECT_TRUE(arangodb::DatabaseNameValidator::isAllowedName(
      true, true, arangodb::velocypack::StringRef("😀 🍺")));
  EXPECT_TRUE(arangodb::DatabaseNameValidator::isAllowedName(
      true, true, arangodb::velocypack::StringRef("maçã")));
  EXPECT_TRUE(arangodb::DatabaseNameValidator::isAllowedName(
      true, true, arangodb::velocypack::StringRef("ﻚﻠﺑ ﻞﻄﻴﻓ")));
}

TEST(CollectionNameValidatorTest, test_isAllowedName_traditionalNames) {
  // direct (non-system)
  std::string const borderline(256, 'x');
  std::string const borderlineSystem(std::string("_") +
                                     std::string(borderline.size() - 1, 'x'));
  std::string const tooLong(borderline.size() + 1, 'x');
  {
    EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(
        false, false, arangodb::velocypack::StringRef(nullptr, 0)));
    EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(
        false, false, arangodb::velocypack::StringRef("")));
    EXPECT_TRUE(arangodb::CollectionNameValidator::isAllowedName(
        false, false, arangodb::velocypack::StringRef("abc123")));
    EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(
        false, false, arangodb::velocypack::StringRef("123abc")));
    EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(
        false, false, arangodb::velocypack::StringRef("123")));
    EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(
        false, false, arangodb::velocypack::StringRef("_123")));
    EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(
        false, false, arangodb::velocypack::StringRef("_abc")));
    EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(
        false, false, arangodb::velocypack::StringRef("_")));
    EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(
        false, false, arangodb::velocypack::StringRef(":")));
    EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(
        false, false, arangodb::velocypack::StringRef("abc:d")));
    EXPECT_TRUE(arangodb::CollectionNameValidator::isAllowedName(
        false, false, arangodb::velocypack::StringRef(borderline)));
    EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(
        false, false, arangodb::velocypack::StringRef(borderlineSystem)));
    EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(
        false, false, arangodb::velocypack::StringRef(tooLong)));
  }

  // direct (system)
  {
    EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(
        true, false, arangodb::velocypack::StringRef(nullptr, 0)));
    EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(
        true, false, arangodb::velocypack::StringRef("")));
    EXPECT_TRUE(arangodb::CollectionNameValidator::isAllowedName(
        true, false, arangodb::velocypack::StringRef("abc123")));
    EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(
        true, false, arangodb::velocypack::StringRef("123abc")));
    EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(
        true, false, arangodb::velocypack::StringRef("123")));
    EXPECT_TRUE(arangodb::CollectionNameValidator::isAllowedName(
        true, false, arangodb::velocypack::StringRef("_123")));
    EXPECT_TRUE(arangodb::CollectionNameValidator::isAllowedName(
        true, false, arangodb::velocypack::StringRef("_abc")));
    EXPECT_TRUE(arangodb::CollectionNameValidator::isAllowedName(
        true, false, arangodb::velocypack::StringRef(borderline)));
    EXPECT_TRUE(arangodb::CollectionNameValidator::isAllowedName(
        true, false, arangodb::velocypack::StringRef(borderlineSystem)));
    EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(
        true, false, arangodb::velocypack::StringRef(tooLong)));
  }

  // special characters
  EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(
      true, false, arangodb::velocypack::StringRef(" a + & ? = abc ")));
  EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(
      true, false, arangodb::velocypack::StringRef("<script>alert(1);")));
  EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(
      true, false, arangodb::velocypack::StringRef("a b c")));
  EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(
      true, false, arangodb::velocypack::StringRef("test123 & ' \" < > abc")));

  EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(
      true, false, arangodb::velocypack::StringRef("abc:cde")));
  EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(
      true, false, arangodb::velocypack::StringRef(".abc")));
  EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(
      true, false, arangodb::velocypack::StringRef("a\0b", 3)));

  // unicode
  EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(
      true, false, arangodb::velocypack::StringRef("mötör")));
  EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(
      true, false, arangodb::velocypack::StringRef("😀")));
  EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(
      true, false, arangodb::velocypack::StringRef("😀 🍺")));
  EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(
      true, false, arangodb::velocypack::StringRef("maçã")));
  EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(
      true, false, arangodb::velocypack::StringRef("ﻚﻠﺑ ﻞﻄﻴﻓ")));
}

TEST(CollectionNameValidatorTest, test_isAllowedName_extendedNames) {
  // direct (non-system)
  std::string const borderline(256, 'x');
  std::string const borderlineSystem(std::string("_") +
                                     std::string(borderline.size() - 1, 'x'));
  std::string const tooLong(borderline.size() + 1, 'x');
  {
    EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(
        false, true, arangodb::velocypack::StringRef(nullptr, 0)));
    EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(
        false, true, arangodb::velocypack::StringRef("")));
    EXPECT_TRUE(arangodb::CollectionNameValidator::isAllowedName(
        false, true, arangodb::velocypack::StringRef("abc123")));
    EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(
        false, true, arangodb::velocypack::StringRef("123abc")));
    EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(
        false, true, arangodb::velocypack::StringRef("123")));
    EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(
        false, true, arangodb::velocypack::StringRef("_123")));
    EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(
        false, true, arangodb::velocypack::StringRef("_abc")));
    EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(
        false, true, arangodb::velocypack::StringRef("_")));
    EXPECT_TRUE(arangodb::CollectionNameValidator::isAllowedName(
        false, true, arangodb::velocypack::StringRef(":")));
    EXPECT_TRUE(arangodb::CollectionNameValidator::isAllowedName(
        false, true, arangodb::velocypack::StringRef("abc:d")));
    EXPECT_TRUE(arangodb::CollectionNameValidator::isAllowedName(
        false, true, arangodb::velocypack::StringRef(borderline)));
    EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(
        false, true, arangodb::velocypack::StringRef(tooLong)));
  }

  // direct (system)
  {
    EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(
        true, true, arangodb::velocypack::StringRef(nullptr, 0)));
    EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(
        true, true, arangodb::velocypack::StringRef("")));
    EXPECT_TRUE(arangodb::CollectionNameValidator::isAllowedName(
        true, true, arangodb::velocypack::StringRef("abc123")));
    EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(
        true, true, arangodb::velocypack::StringRef("123abc")));
    EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(
        true, true, arangodb::velocypack::StringRef("123")));
    EXPECT_TRUE(arangodb::CollectionNameValidator::isAllowedName(
        true, true, arangodb::velocypack::StringRef("_123")));
    EXPECT_TRUE(arangodb::CollectionNameValidator::isAllowedName(
        true, true, arangodb::velocypack::StringRef("_abc")));
    EXPECT_TRUE(arangodb::CollectionNameValidator::isAllowedName(
        true, true, arangodb::velocypack::StringRef(borderline)));
    EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(
        true, true, arangodb::velocypack::StringRef(tooLong)));
  }

  // special characters
  EXPECT_TRUE(arangodb::CollectionNameValidator::isAllowedName(
      true, true, arangodb::velocypack::StringRef(" a + & ? = abc ")));
  EXPECT_TRUE(arangodb::CollectionNameValidator::isAllowedName(
      true, true, arangodb::velocypack::StringRef("<script>alert(1);")));
  EXPECT_TRUE(arangodb::CollectionNameValidator::isAllowedName(
      true, true, arangodb::velocypack::StringRef("a b c")));
  EXPECT_TRUE(arangodb::CollectionNameValidator::isAllowedName(
      true, true, arangodb::velocypack::StringRef("test123 & ' \" < > abc")));

  EXPECT_TRUE(arangodb::CollectionNameValidator::isAllowedName(
      true, true, arangodb::velocypack::StringRef("abc:cde")));
  EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(
      true, true, arangodb::velocypack::StringRef(".abc")));
  EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(
      true, true, arangodb::velocypack::StringRef("a\0b", 3)));
  EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(
      true, true, arangodb::velocypack::StringRef("/")));
  EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(
      true, true, arangodb::velocypack::StringRef("a/")));
  EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(
      true, true, arangodb::velocypack::StringRef("a/b")));
  EXPECT_TRUE(arangodb::CollectionNameValidator::isAllowedName(
      true, true, arangodb::velocypack::StringRef("a\\b")));
  EXPECT_TRUE(arangodb::CollectionNameValidator::isAllowedName(
      true, true, arangodb::velocypack::StringRef("a.b.c")));

  // unicode
  EXPECT_TRUE(arangodb::CollectionNameValidator::isAllowedName(
      true, true, arangodb::velocypack::StringRef("mötör")));
  EXPECT_TRUE(arangodb::CollectionNameValidator::isAllowedName(
      true, true, arangodb::velocypack::StringRef("😀")));
  EXPECT_TRUE(arangodb::CollectionNameValidator::isAllowedName(
      true, true, arangodb::velocypack::StringRef("😀 🍺")));
  EXPECT_TRUE(arangodb::CollectionNameValidator::isAllowedName(
      true, true, arangodb::velocypack::StringRef("maçã")));
  EXPECT_TRUE(arangodb::CollectionNameValidator::isAllowedName(
      true, true, arangodb::velocypack::StringRef("ﻚﻠﺑ ﻞﻄﻴﻓ")));
}

TEST(IndexNameValidatorTest, test_isAllowedName_traditionalNames) {
  // direct (non-system)
  std::string const borderline(256, 'x');
  std::string const borderlineSystem(std::string("_") +
                                     std::string(borderline.size() - 1, 'x'));
  std::string const tooLong(borderline.size() + 1, 'x');
  EXPECT_FALSE(arangodb::IndexNameValidator::isAllowedName(
      false, arangodb::velocypack::StringRef(nullptr, 0)));
  EXPECT_FALSE(
      arangodb::IndexNameValidator::isAllowedName(false, arangodb::velocypack::StringRef("")));
  EXPECT_TRUE(arangodb::IndexNameValidator::isAllowedName(
      false, arangodb::velocypack::StringRef("abc123")));
  EXPECT_TRUE(arangodb::IndexNameValidator::isAllowedName(
      false, arangodb::velocypack::StringRef("Abc123")));
  EXPECT_FALSE(arangodb::IndexNameValidator::isAllowedName(
      false, arangodb::velocypack::StringRef("123abc")));
  EXPECT_FALSE(arangodb::IndexNameValidator::isAllowedName(false, arangodb::velocypack::StringRef(
                                                                      "123")));
  EXPECT_FALSE(arangodb::IndexNameValidator::isAllowedName(false, arangodb::velocypack::StringRef(
                                                                      "_123")));
  EXPECT_FALSE(arangodb::IndexNameValidator::isAllowedName(false, arangodb::velocypack::StringRef(
                                                                      "_abc")));
  EXPECT_TRUE(arangodb::IndexNameValidator::isAllowedName(false, arangodb::velocypack::StringRef(
                                                                     "abc_")));
  EXPECT_TRUE(arangodb::IndexNameValidator::isAllowedName(false, arangodb::velocypack::StringRef(
                                                                     "abc-")));
  EXPECT_FALSE(
      arangodb::IndexNameValidator::isAllowedName(false, arangodb::velocypack::StringRef("_")));
  EXPECT_FALSE(
      arangodb::IndexNameValidator::isAllowedName(false, arangodb::velocypack::StringRef(":")));
  EXPECT_FALSE(arangodb::IndexNameValidator::isAllowedName(
      false, arangodb::velocypack::StringRef("abc:d")));
  EXPECT_TRUE(arangodb::IndexNameValidator::isAllowedName(false, arangodb::velocypack::StringRef(
                                                                     borderline)));
  EXPECT_FALSE(arangodb::IndexNameValidator::isAllowedName(
      false, arangodb::velocypack::StringRef(borderlineSystem)));
  EXPECT_FALSE(arangodb::IndexNameValidator::isAllowedName(false, arangodb::velocypack::StringRef(
                                                                      tooLong)));

  // special characters
  EXPECT_FALSE(arangodb::IndexNameValidator::isAllowedName(
      false, arangodb::velocypack::StringRef(" a + & ? = abc ")));
  EXPECT_FALSE(arangodb::IndexNameValidator::isAllowedName(
      false, arangodb::velocypack::StringRef("<script>alert(1);")));
  EXPECT_FALSE(arangodb::IndexNameValidator::isAllowedName(
      false, arangodb::velocypack::StringRef("a b c")));
  EXPECT_FALSE(arangodb::IndexNameValidator::isAllowedName(
      false, arangodb::velocypack::StringRef("test123 & ' \" < > abc")));

  EXPECT_FALSE(arangodb::IndexNameValidator::isAllowedName(
      false, arangodb::velocypack::StringRef("abc:cde")));
  EXPECT_FALSE(arangodb::IndexNameValidator::isAllowedName(false, arangodb::velocypack::StringRef(
                                                                      ".abc")));
  EXPECT_FALSE(arangodb::IndexNameValidator::isAllowedName(
      false, arangodb::velocypack::StringRef("a\0b", 3)));
  EXPECT_FALSE(arangodb::IndexNameValidator::isAllowedName(false, arangodb::velocypack::StringRef(
                                                                      "a/b")));
  EXPECT_FALSE(arangodb::IndexNameValidator::isAllowedName(false, arangodb::velocypack::StringRef(
                                                                      "//")));
  EXPECT_FALSE(arangodb::IndexNameValidator::isAllowedName(false, arangodb::velocypack::StringRef(
                                                                      "/\\")));

  // unicode
  EXPECT_FALSE(arangodb::IndexNameValidator::isAllowedName(
      false, arangodb::velocypack::StringRef("mötör")));
  EXPECT_FALSE(
      arangodb::IndexNameValidator::isAllowedName(false, arangodb::velocypack::StringRef("😀")));
  EXPECT_FALSE(arangodb::IndexNameValidator::isAllowedName(false, arangodb::velocypack::StringRef(
                                                                      "😀 🍺")));
  EXPECT_FALSE(arangodb::IndexNameValidator::isAllowedName(false, arangodb::velocypack::StringRef(
                                                                      "maçã")));
  EXPECT_FALSE(arangodb::IndexNameValidator::isAllowedName(
      false, arangodb::velocypack::StringRef("ﻚﻠﺑ ﻞﻄﻴﻓ")));
}

TEST(IndexNameValidatorTest, test_isAllowedName_extendedNames) {
  // direct (non-system)
  std::string const borderline(256, 'x');
  std::string const borderlineSystem(std::string("_") +
                                     std::string(borderline.size() - 1, 'x'));
  std::string const tooLong(borderline.size() + 1, 'x');
  EXPECT_FALSE(arangodb::IndexNameValidator::isAllowedName(
      true, arangodb::velocypack::StringRef(nullptr, 0)));
  EXPECT_FALSE(
      arangodb::IndexNameValidator::isAllowedName(true, arangodb::velocypack::StringRef("")));
  EXPECT_TRUE(arangodb::IndexNameValidator::isAllowedName(true, arangodb::velocypack::StringRef(
                                                                    "abc123")));
  EXPECT_TRUE(arangodb::IndexNameValidator::isAllowedName(true, arangodb::velocypack::StringRef(
                                                                    "Abc123")));
  EXPECT_FALSE(arangodb::IndexNameValidator::isAllowedName(
      true, arangodb::velocypack::StringRef("123abc")));
  EXPECT_FALSE(arangodb::IndexNameValidator::isAllowedName(true, arangodb::velocypack::StringRef(
                                                                     "123")));
  EXPECT_TRUE(arangodb::IndexNameValidator::isAllowedName(true, arangodb::velocypack::StringRef(
                                                                    "_123")));
  EXPECT_TRUE(arangodb::IndexNameValidator::isAllowedName(true, arangodb::velocypack::StringRef(
                                                                    "_abc")));
  EXPECT_TRUE(arangodb::IndexNameValidator::isAllowedName(true, arangodb::velocypack::StringRef(
                                                                    "abc_")));
  EXPECT_TRUE(arangodb::IndexNameValidator::isAllowedName(true, arangodb::velocypack::StringRef(
                                                                    "abc-")));
  EXPECT_TRUE(arangodb::IndexNameValidator::isAllowedName(true, arangodb::velocypack::StringRef(
                                                                    borderline)));
  EXPECT_TRUE(arangodb::IndexNameValidator::isAllowedName(
      true, arangodb::velocypack::StringRef(borderlineSystem)));
  EXPECT_FALSE(arangodb::IndexNameValidator::isAllowedName(true, arangodb::velocypack::StringRef(
                                                                     tooLong)));

  // special characters
  EXPECT_TRUE(arangodb::IndexNameValidator::isAllowedName(
      true, arangodb::velocypack::StringRef(" a + & ? = abc ")));
  EXPECT_TRUE(arangodb::IndexNameValidator::isAllowedName(
      true, arangodb::velocypack::StringRef("<script>alert(1);")));
  EXPECT_TRUE(arangodb::IndexNameValidator::isAllowedName(true, arangodb::velocypack::StringRef(
                                                                    "a b c")));
  EXPECT_TRUE(arangodb::IndexNameValidator::isAllowedName(
      true, arangodb::velocypack::StringRef("test123 & ' \" < > abc")));
  EXPECT_TRUE(arangodb::IndexNameValidator::isAllowedName(
      true, arangodb::velocypack::StringRef("abc:cde")));
  EXPECT_TRUE(arangodb::IndexNameValidator::isAllowedName(true, arangodb::velocypack::StringRef(
                                                                    ".abc")));
  EXPECT_FALSE(arangodb::IndexNameValidator::isAllowedName(
      true, arangodb::velocypack::StringRef("a\0b", 3)));
  EXPECT_FALSE(
      arangodb::IndexNameValidator::isAllowedName(true, arangodb::velocypack::StringRef("/")));
  EXPECT_FALSE(arangodb::IndexNameValidator::isAllowedName(true, arangodb::velocypack::StringRef(
                                                                     "/\\")));
  EXPECT_FALSE(
      arangodb::IndexNameValidator::isAllowedName(true, arangodb::velocypack::StringRef("a/")));
  EXPECT_FALSE(arangodb::IndexNameValidator::isAllowedName(true, arangodb::velocypack::StringRef(
                                                                     "a/b")));
  EXPECT_TRUE(arangodb::IndexNameValidator::isAllowedName(true, arangodb::velocypack::StringRef(
                                                                    "a\\b")));
  EXPECT_TRUE(arangodb::IndexNameValidator::isAllowedName(true, arangodb::velocypack::StringRef(
                                                                    "a.b.c")));

  // unicode
  EXPECT_TRUE(arangodb::IndexNameValidator::isAllowedName(true, arangodb::velocypack::StringRef(
                                                                    "mötör")));
  EXPECT_TRUE(arangodb::IndexNameValidator::isAllowedName(true, arangodb::velocypack::StringRef("😀")));
  EXPECT_TRUE(arangodb::IndexNameValidator::isAllowedName(true, arangodb::velocypack::StringRef(
                                                                    "😀 🍺")));
  EXPECT_TRUE(arangodb::IndexNameValidator::isAllowedName(true, arangodb::velocypack::StringRef(
                                                                    "maçã")));
  EXPECT_TRUE(arangodb::IndexNameValidator::isAllowedName(
      true, arangodb::velocypack::StringRef("ﻚﻠﺑ ﻞﻄﻴﻓ")));
}

TEST(AnalyzerNameValidatorTest, test_isAllowedName_traditionalNames) {
  // direct (non-system)
  std::string const borderline(64, 'x');
  std::string const borderlineSystem(std::string("_") +
                                     std::string(borderline.size() - 1, 'x'));
  std::string const tooLong(borderline.size() + 1, 'x');
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(
      false, arangodb::velocypack::StringRef(nullptr, 0)));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(false, arangodb::velocypack::StringRef(
                                                                         "")));
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::isAllowedName(
      false, arangodb::velocypack::StringRef("abc123")));
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::isAllowedName(
      false, arangodb::velocypack::StringRef("Abc123")));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(
      false, arangodb::velocypack::StringRef("123abc")));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(
      false, arangodb::velocypack::StringRef("123")));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(
      false, arangodb::velocypack::StringRef("_123")));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(
      false, arangodb::velocypack::StringRef("_abc")));
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::isAllowedName(
      false, arangodb::velocypack::StringRef("abc_")));
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::isAllowedName(
      false, arangodb::velocypack::StringRef("abc-")));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(false, arangodb::velocypack::StringRef(
                                                                         "_")));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(false, arangodb::velocypack::StringRef(
                                                                         ":")));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(
      false, arangodb::velocypack::StringRef("abc:d")));
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::isAllowedName(
      false, arangodb::velocypack::StringRef(borderline)));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(
      false, arangodb::velocypack::StringRef(borderlineSystem)));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(
      false, arangodb::velocypack::StringRef(tooLong)));

  // special characters
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(
      false, arangodb::velocypack::StringRef(" a + & ? = abc ")));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(
      false, arangodb::velocypack::StringRef("<script>alert(1);")));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(
      false, arangodb::velocypack::StringRef("a b c")));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(
      false, arangodb::velocypack::StringRef("test123 & ' \" < > abc")));

  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(
      false, arangodb::velocypack::StringRef("abc:cde")));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(
      false, arangodb::velocypack::StringRef(".abc")));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(
      false, arangodb::velocypack::StringRef("a\0b", 3)));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(
      false, arangodb::velocypack::StringRef("a/b")));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(
      false, arangodb::velocypack::StringRef("//")));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(
      false, arangodb::velocypack::StringRef("/\\")));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(
      false, arangodb::velocypack::StringRef("a:b")));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(false, arangodb::velocypack::StringRef(
                                                                         ":")));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(
      false, arangodb::velocypack::StringRef("aaaa::")));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(
      false, arangodb::velocypack::StringRef(":aaaa")));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(
      false, arangodb::velocypack::StringRef("abcdef::gghh")));

  // unicode
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(
      false, arangodb::velocypack::StringRef("mötör")));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(false, arangodb::velocypack::StringRef(
                                                                         "😀")));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(
      false, arangodb::velocypack::StringRef("😀 🍺")));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(
      false, arangodb::velocypack::StringRef("maçã")));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(
      false, arangodb::velocypack::StringRef("ﻚﻠﺑ ﻞﻄﻴﻓ")));
}

TEST(AnalyzerNameValidatorTest, test_isAllowedName_extendedNames) {
  // direct (non-system)
  std::string const borderline(64, 'x');
  std::string const borderlineSystem(std::string("_") +
                                     std::string(borderline.size() - 1, 'x'));
  std::string const tooLong(borderline.size() + 1, 'x');
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(
      true, arangodb::velocypack::StringRef(nullptr, 0)));
  EXPECT_FALSE(
      arangodb::AnalyzerNameValidator::isAllowedName(true, arangodb::velocypack::StringRef("")));
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::isAllowedName(
      true, arangodb::velocypack::StringRef("abc123")));
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::isAllowedName(
      true, arangodb::velocypack::StringRef("Abc123")));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(
      true, arangodb::velocypack::StringRef("123abc")));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(
      true, arangodb::velocypack::StringRef("123")));
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::isAllowedName(
      true, arangodb::velocypack::StringRef("_123")));
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::isAllowedName(
      true, arangodb::velocypack::StringRef("_abc")));
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::isAllowedName(
      true, arangodb::velocypack::StringRef("abc_")));
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::isAllowedName(
      true, arangodb::velocypack::StringRef("abc-")));
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::isAllowedName(
      true, arangodb::velocypack::StringRef(borderline)));
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::isAllowedName(
      true, arangodb::velocypack::StringRef(borderlineSystem)));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(
      true, arangodb::velocypack::StringRef(tooLong)));

  // special characters
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::isAllowedName(
      true, arangodb::velocypack::StringRef(" a + & ? = abc ")));
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::isAllowedName(
      true, arangodb::velocypack::StringRef("<script>alert(1);")));
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::isAllowedName(
      true, arangodb::velocypack::StringRef("a b c")));
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::isAllowedName(
      true, arangodb::velocypack::StringRef("test123 & ' \" < > abc")));
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::isAllowedName(
      true, arangodb::velocypack::StringRef(".abc")));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(
      true, arangodb::velocypack::StringRef("a\0b", 3)));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(true, arangodb::velocypack::StringRef(
                                                                        "/")));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(
      true, arangodb::velocypack::StringRef("/\\")));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(true, arangodb::velocypack::StringRef(
                                                                        "a/")));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(
      true, arangodb::velocypack::StringRef("a/b")));
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::isAllowedName(
      true, arangodb::velocypack::StringRef("a\\b")));
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::isAllowedName(
      true, arangodb::velocypack::StringRef("a.b.c")));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(
      true, arangodb::velocypack::StringRef("a:b")));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(true, arangodb::velocypack::StringRef(
                                                                        ":")));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(
      true, arangodb::velocypack::StringRef("abc:cde")));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(
      true, arangodb::velocypack::StringRef("aaaa::")));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(
      true, arangodb::velocypack::StringRef(":aaaa")));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(
      true, arangodb::velocypack::StringRef("abcdef::gghh")));

  // unicode
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::isAllowedName(
      true, arangodb::velocypack::StringRef("mötör")));
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::isAllowedName(true, arangodb::velocypack::StringRef(
                                                                       "😀")));
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::isAllowedName(true, arangodb::velocypack::StringRef(
                                                                       "😀 🍺")));
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::isAllowedName(
      true, arangodb::velocypack::StringRef("maçã")));
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::isAllowedName(
      true, arangodb::velocypack::StringRef("ﻚﻠﺑ ﻞﻄﻴﻓ")));
}

TEST(ViewNameValidatorTest, test_isAllowedName_traditionalNames) {
  // direct (non-system)
  std::string const borderline(64, 'x');
  std::string const borderlineSystem(std::string("_") +
                                     std::string(borderline.size() - 1, 'x'));
  std::string const tooLong(borderline.size() + 1, 'x');
  {
    EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(
        false, false, arangodb::velocypack::StringRef(nullptr, 0)));
    EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(
        false, false, arangodb::velocypack::StringRef("")));
    EXPECT_TRUE(arangodb::ViewNameValidator::isAllowedName(
        false, false, arangodb::velocypack::StringRef("abc123")));
    EXPECT_TRUE(arangodb::ViewNameValidator::isAllowedName(
        false, false, arangodb::velocypack::StringRef("Abc123")));
    EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(
        false, false, arangodb::velocypack::StringRef("123abc")));
    EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(
        false, false, arangodb::velocypack::StringRef("123")));
    EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(
        false, false, arangodb::velocypack::StringRef("_123")));
    EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(
        false, false, arangodb::velocypack::StringRef("_abc")));
    EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(
        false, false, arangodb::velocypack::StringRef("_")));
    EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(
        false, false, arangodb::velocypack::StringRef(":")));
    EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(
        false, false, arangodb::velocypack::StringRef("abc:d")));
    EXPECT_TRUE(arangodb::ViewNameValidator::isAllowedName(
        false, false, arangodb::velocypack::StringRef(borderline)));
    EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(
        false, false, arangodb::velocypack::StringRef(borderlineSystem)));
    EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(
        false, false, arangodb::velocypack::StringRef(tooLong)));
  }

  // direct (system)
  {
    EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(
        true, false, arangodb::velocypack::StringRef(nullptr, 0)));
    EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(
        true, false, arangodb::velocypack::StringRef("")));
    EXPECT_TRUE(arangodb::ViewNameValidator::isAllowedName(
        true, false, arangodb::velocypack::StringRef("abc123")));
    EXPECT_TRUE(arangodb::ViewNameValidator::isAllowedName(
        true, false, arangodb::velocypack::StringRef("Abc123")));
    EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(
        true, false, arangodb::velocypack::StringRef("123abc")));
    EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(
        true, false, arangodb::velocypack::StringRef("123")));
    EXPECT_TRUE(arangodb::ViewNameValidator::isAllowedName(
        true, false, arangodb::velocypack::StringRef("_123")));
    EXPECT_TRUE(arangodb::ViewNameValidator::isAllowedName(
        true, false, arangodb::velocypack::StringRef("_abc")));
    EXPECT_TRUE(arangodb::ViewNameValidator::isAllowedName(
        true, false, arangodb::velocypack::StringRef(borderline)));
    EXPECT_TRUE(arangodb::ViewNameValidator::isAllowedName(
        true, false, arangodb::velocypack::StringRef(borderlineSystem)));
    EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(
        true, false, arangodb::velocypack::StringRef(tooLong)));
  }

  // special characters
  EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(
      true, false, arangodb::velocypack::StringRef(" a + & ? = abc ")));
  EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(
      true, false, arangodb::velocypack::StringRef("<script>alert(1);")));
  EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(
      true, false, arangodb::velocypack::StringRef("a b c")));
  EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(
      true, false, arangodb::velocypack::StringRef("test123 & ' \" < > abc")));

  EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(
      true, false, arangodb::velocypack::StringRef("abc:cde")));
  EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(
      true, false, arangodb::velocypack::StringRef(".abc")));
  EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(
      true, false, arangodb::velocypack::StringRef(".123abc")));
  EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(
      true, false, arangodb::velocypack::StringRef("a\0b", 3)));

  // unicode
  EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(
      true, false, arangodb::velocypack::StringRef("mötör")));
  EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(true, false,
                                                          arangodb::velocypack::StringRef("😀")));
  EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(
      true, false, arangodb::velocypack::StringRef("😀 🍺")));
  EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(
      true, false, arangodb::velocypack::StringRef("maçã")));
  EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(
      true, false, arangodb::velocypack::StringRef("ﻚﻠﺑ ﻞﻄﻴﻓ")));
}

TEST(ViewNameValidatorTest, test_isAllowedName_extendedNames) {
  // direct (non-system)
  std::string const borderline(256, 'x');
  std::string const borderlineSystem(std::string("_") +
                                     std::string(borderline.size() - 1, 'x'));
  std::string const tooLong(borderline.size() + 1, 'x');
  {
    EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(
        false, true, arangodb::velocypack::StringRef(nullptr, 0)));
    EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(
        false, true, arangodb::velocypack::StringRef("")));
    EXPECT_TRUE(arangodb::ViewNameValidator::isAllowedName(
        false, true, arangodb::velocypack::StringRef("abc123")));
    EXPECT_TRUE(arangodb::ViewNameValidator::isAllowedName(
        false, true, arangodb::velocypack::StringRef("Abc123")));
    EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(
        false, true, arangodb::velocypack::StringRef("123abc")));
    EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(
        false, true, arangodb::velocypack::StringRef("123")));
    EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(
        false, true, arangodb::velocypack::StringRef("_123")));
    EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(
        false, true, arangodb::velocypack::StringRef("_abc")));
    EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(
        false, true, arangodb::velocypack::StringRef("_")));
    EXPECT_TRUE(arangodb::ViewNameValidator::isAllowedName(
        false, true, arangodb::velocypack::StringRef(":")));
    EXPECT_TRUE(arangodb::ViewNameValidator::isAllowedName(
        false, true, arangodb::velocypack::StringRef("abc:d")));
    EXPECT_TRUE(arangodb::ViewNameValidator::isAllowedName(
        false, true, arangodb::velocypack::StringRef(borderline)));
    EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(
        false, true, arangodb::velocypack::StringRef(tooLong)));
  }

  // direct (system)
  {
    EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(
        true, true, arangodb::velocypack::StringRef(nullptr, 0)));
    EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(
        true, true, arangodb::velocypack::StringRef("")));
    EXPECT_TRUE(arangodb::ViewNameValidator::isAllowedName(
        true, true, arangodb::velocypack::StringRef("abc123")));
    EXPECT_TRUE(arangodb::ViewNameValidator::isAllowedName(
        true, true, arangodb::velocypack::StringRef("Abc123")));
    EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(
        true, true, arangodb::velocypack::StringRef("123abc")));
    EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(
        true, true, arangodb::velocypack::StringRef("123")));
    EXPECT_TRUE(arangodb::ViewNameValidator::isAllowedName(
        true, true, arangodb::velocypack::StringRef("_123")));
    EXPECT_TRUE(arangodb::ViewNameValidator::isAllowedName(
        true, true, arangodb::velocypack::StringRef("_abc")));
    EXPECT_TRUE(arangodb::ViewNameValidator::isAllowedName(
        true, true, arangodb::velocypack::StringRef(borderline)));
    EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(
        true, true, arangodb::velocypack::StringRef(tooLong)));
  }

  // special characters
  EXPECT_TRUE(arangodb::ViewNameValidator::isAllowedName(
      true, true, arangodb::velocypack::StringRef(" a + & ? = abc ")));
  EXPECT_TRUE(arangodb::ViewNameValidator::isAllowedName(
      true, true, arangodb::velocypack::StringRef("<script>alert(1);")));
  EXPECT_TRUE(arangodb::ViewNameValidator::isAllowedName(
      true, true, arangodb::velocypack::StringRef("a b c")));
  EXPECT_TRUE(arangodb::ViewNameValidator::isAllowedName(
      true, true, arangodb::velocypack::StringRef("test123 & ' \" < > abc")));

  EXPECT_TRUE(arangodb::ViewNameValidator::isAllowedName(
      true, true, arangodb::velocypack::StringRef("abc:cde")));
  EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(
      true, true, arangodb::velocypack::StringRef(".abc")));
  EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(
      true, true, arangodb::velocypack::StringRef(".123abc")));
  EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(
      true, true, arangodb::velocypack::StringRef("a\0b", 3)));
  EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(true, true,
                                                          arangodb::velocypack::StringRef("/")));
  EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(true, true,
                                                          arangodb::velocypack::StringRef("a/")));
  EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(
      true, true, arangodb::velocypack::StringRef("a/b")));
  EXPECT_TRUE(arangodb::ViewNameValidator::isAllowedName(
      true, true, arangodb::velocypack::StringRef("a\\b")));
  EXPECT_TRUE(arangodb::ViewNameValidator::isAllowedName(
      true, true, arangodb::velocypack::StringRef("a.b.c")));

  // unicode
  EXPECT_TRUE(arangodb::ViewNameValidator::isAllowedName(
      true, true, arangodb::velocypack::StringRef("mötör")));
  EXPECT_TRUE(arangodb::ViewNameValidator::isAllowedName(true, true,
                                                         arangodb::velocypack::StringRef("😀")));
  EXPECT_TRUE(arangodb::ViewNameValidator::isAllowedName(true, true,
                                                         arangodb::velocypack::StringRef("😀 🍺")));
  EXPECT_TRUE(arangodb::ViewNameValidator::isAllowedName(
      true, true, arangodb::velocypack::StringRef("maçã")));
  EXPECT_TRUE(arangodb::ViewNameValidator::isAllowedName(
      true, true, arangodb::velocypack::StringRef("ﻚﻠﺑ ﻞﻄﻴﻓ")));
}
