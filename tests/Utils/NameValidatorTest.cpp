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

#include <string_view>

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
    EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                     false, false, std::string_view(nullptr, 0))
                     .ok());
    EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                     false, false, std::string_view(""))
                     .ok());
    EXPECT_TRUE(arangodb::DatabaseNameValidator::validateName(
                    false, false, std::string_view("abc123"))
                    .ok());
    EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                     false, false, std::string_view("123abc"))
                     .ok());
    EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                     false, false, std::string_view("123"))
                     .ok());
    EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                     false, false, std::string_view("_123"))
                     .ok());
    EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                     false, false, std::string_view("_abc"))
                     .ok());
    EXPECT_TRUE(arangodb::DatabaseNameValidator::validateName(
                    false, false, std::string_view(borderline))
                    .ok());
    EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                     false, false, std::string_view(borderlineSystem))
                     .ok());
    EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                     false, false, std::string_view(tooLong))
                     .ok());
  }

  // direct (system)
  {
    EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                     true, false, std::string_view(nullptr, 0))
                     .ok());
    EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                     true, false, std::string_view(""))
                     .ok());
    EXPECT_TRUE(arangodb::DatabaseNameValidator::validateName(
                    true, false, std::string_view("abc123"))
                    .ok());
    EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                     true, false, std::string_view("123abc"))
                     .ok());
    EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                     true, false, std::string_view("123"))
                     .ok());
    EXPECT_TRUE(arangodb::DatabaseNameValidator::validateName(
                    true, false, std::string_view("_123"))
                    .ok());
    EXPECT_TRUE(arangodb::DatabaseNameValidator::validateName(
                    true, false, std::string_view("_abc"))
                    .ok());
    EXPECT_TRUE(arangodb::DatabaseNameValidator::validateName(
                    true, false, std::string_view(borderline))
                    .ok());
    EXPECT_TRUE(arangodb::DatabaseNameValidator::validateName(
                    true, false, std::string_view(borderlineSystem))
                    .ok());
    EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                     true, false, std::string_view(tooLong))
                     .ok());
  }

  // special characters
  EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                   true, false, std::string_view(" a + & ? = abc "))
                   .ok());
  EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                   true, false, std::string_view("<script>alert(1);"))
                   .ok());
  EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                   true, false, std::string_view("a b c"))
                   .ok());
  EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                   true, false, std::string_view("test123 & ' \" < > abc"))
                   .ok());

  EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                   true, false, std::string_view("abc:cde"))
                   .ok());
  EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                   true, false, std::string_view(".abc"))
                   .ok());
  EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                   true, false, std::string_view("a\0b", 3))
                   .ok());
  EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                   true, false, std::string_view("/"))
                   .ok());
  EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                   true, false, std::string_view("a/"))
                   .ok());
  EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                   true, false, std::string_view("a/b"))
                   .ok());
  EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                   true, false, std::string_view("a\\b"))
                   .ok());
  EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                   true, false, std::string_view("a.b.c"))
                   .ok());
  EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                   true, false, std::string_view(" a"))
                   .ok());
  EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                   true, false, std::string_view("\na"))
                   .ok());
  EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                   true, false, std::string_view("\ta"))
                   .ok());
  EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                   true, false, std::string_view("\ra"))
                   .ok());
  EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                   true, false, std::string_view("\ba"))
                   .ok());
  EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                   true, false, std::string_view("\fa"))
                   .ok());
  EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                   true, false, std::string_view("a\n"))
                   .ok());
  EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                   true, false, std::string_view("a\t"))
                   .ok());
  EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                   true, false, std::string_view("a\r"))
                   .ok());
  EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                   true, false, std::string_view("a\b"))
                   .ok());
  EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                   true, false, std::string_view("a\f"))
                   .ok());

  // spaces
  EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                   true, false, std::string_view(" a"))
                   .ok());
  EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                   true, false, std::string_view("a "))
                   .ok());
  EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                   true, false, std::string_view("a  b"))
                   .ok());

  // unicode
  EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                   true, false, std::string_view("mötör"))
                   .ok());
  EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                   true, false, std::string_view("😀"))
                   .ok());
  EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                   true, false, std::string_view("😀 🍺"))
                   .ok());
  EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                   true, false, std::string_view("maçã"))
                   .ok());
  EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                   true, false, std::string_view("ﻚﻠﺑ ﻞﻄﻴﻓ"))
                   .ok());
}

TEST(DatabaseNameValidatorTest, test_isAllowedName_extendedNames) {
  // direct (non-system)
  std::string const borderline(128, 'x');
  std::string const tooLong(borderline.size() + 1, 'x');
  {
    EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                     false, true, std::string_view(nullptr, 0))
                     .ok());
    EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                     false, true, std::string_view(""))
                     .ok());
    EXPECT_TRUE(arangodb::DatabaseNameValidator::validateName(
                    false, true, std::string_view("abc123"))
                    .ok());
    EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                     false, true, std::string_view("123abc"))
                     .ok());
    EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                     false, true, std::string_view("123"))
                     .ok());
    EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                     false, true, std::string_view("_123"))
                     .ok());
    EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                     false, true, std::string_view("_abc"))
                     .ok());
    EXPECT_TRUE(arangodb::DatabaseNameValidator::validateName(
                    false, true, std::string_view(borderline))
                    .ok());
    EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                     false, true, std::string_view(tooLong))
                     .ok());
  }

  // direct (system)
  {
    EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                     true, true, std::string_view(nullptr, 0))
                     .ok());
    EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                     true, true, std::string_view(""))
                     .ok());
    EXPECT_TRUE(arangodb::DatabaseNameValidator::validateName(
                    true, true, std::string_view("abc123"))
                    .ok());
    EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                     true, true, std::string_view("123abc"))
                     .ok());
    EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                     true, true, std::string_view("123"))
                     .ok());
    EXPECT_TRUE(arangodb::DatabaseNameValidator::validateName(
                    true, true, std::string_view("_123"))
                    .ok());
    EXPECT_TRUE(arangodb::DatabaseNameValidator::validateName(
                    true, true, std::string_view("_abc"))
                    .ok());
    EXPECT_TRUE(arangodb::DatabaseNameValidator::validateName(
                    true, true, std::string_view(borderline))
                    .ok());
    EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                     true, true, std::string_view(tooLong))
                     .ok());
  }

  // special characters
  EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                   true, true, std::string_view(" a + & ? = abc "))
                   .ok());
  EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                   true, true, std::string_view(" a + & ? = abc"))
                   .ok());
  EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                   true, true, std::string_view("a + & ? = abc "))
                   .ok());
  EXPECT_TRUE(arangodb::DatabaseNameValidator::validateName(
                  true, true, std::string_view("a + & ? = abc"))
                  .ok());
  EXPECT_TRUE(arangodb::DatabaseNameValidator::validateName(
                  true, true, std::string_view("<script>alert(1);"))
                  .ok());
  EXPECT_TRUE(arangodb::DatabaseNameValidator::validateName(
                  true, true, std::string_view("a b c"))
                  .ok());
  EXPECT_TRUE(arangodb::DatabaseNameValidator::validateName(
                  true, true, std::string_view("test123 & ' \" < > abc"))
                  .ok());

  EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                   true, true, std::string_view("abc:cde"))
                   .ok());
  EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                   true, true, std::string_view(".abc"))
                   .ok());
  EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                   true, true, std::string_view("a\0b", 3))
                   .ok());
  EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                   true, true, std::string_view("/"))
                   .ok());
  EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                   true, true, std::string_view("a/"))
                   .ok());
  EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                   true, true, std::string_view("a/b"))
                   .ok());
  EXPECT_TRUE(arangodb::DatabaseNameValidator::validateName(
                  true, true, std::string_view("a\\b"))
                  .ok());
  EXPECT_TRUE(arangodb::DatabaseNameValidator::validateName(
                  true, true, std::string_view("a.b.c"))
                  .ok());
  EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                   true, true, std::string_view("\na"))
                   .ok());
  EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                   true, true, std::string_view("\ta"))
                   .ok());
  EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                   true, true, std::string_view("\ra"))
                   .ok());
  EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                   true, true, std::string_view("\ba"))
                   .ok());
  EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                   true, true, std::string_view("\fa"))
                   .ok());
  EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                   true, true, std::string_view("a\n"))
                   .ok());
  EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                   true, true, std::string_view("a\t"))
                   .ok());
  EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                   true, true, std::string_view("a\r"))
                   .ok());
  EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                   true, true, std::string_view("a\b"))
                   .ok());
  EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                   true, true, std::string_view("a\f"))
                   .ok());

  // spaces
  EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                   true, true, std::string_view(" a"))
                   .ok());
  EXPECT_FALSE(arangodb::DatabaseNameValidator::validateName(
                   true, true, std::string_view("a "))
                   .ok());
  EXPECT_TRUE(arangodb::DatabaseNameValidator::validateName(
                  true, true, std::string_view("a  b"))
                  .ok());

  // unicode
  EXPECT_TRUE(arangodb::DatabaseNameValidator::validateName(
                  true, true, std::string_view("mötör"))
                  .ok());
  EXPECT_TRUE(arangodb::DatabaseNameValidator::validateName(
                  true, true, std::string_view("😀"))
                  .ok());
  EXPECT_TRUE(arangodb::DatabaseNameValidator::validateName(
                  true, true, std::string_view("😀 🍺"))
                  .ok());
  EXPECT_TRUE(arangodb::DatabaseNameValidator::validateName(
                  true, true, std::string_view("maçã"))
                  .ok());
  EXPECT_TRUE(arangodb::DatabaseNameValidator::validateName(
                  true, true, std::string_view("ﻚﻠﺑ ﻞﻄﻴﻓ"))
                  .ok());
}

TEST(CollectionNameValidatorTest, test_isAllowedName_traditionalNames) {
  // direct (non-system)
  std::string const borderline(256, 'x');
  std::string const borderlineSystem(std::string("_") +
                                     std::string(borderline.size() - 1, 'x'));
  std::string const tooLong(borderline.size() + 1, 'x');
  {
    EXPECT_FALSE(arangodb::CollectionNameValidator::validateName(
                     false, false, std::string_view(nullptr, 0))
                     .ok());
    EXPECT_FALSE(arangodb::CollectionNameValidator::validateName(
                     false, false, std::string_view(""))
                     .ok());
    EXPECT_TRUE(arangodb::CollectionNameValidator::validateName(
                    false, false, std::string_view("abc123"))
                    .ok());
    EXPECT_FALSE(arangodb::CollectionNameValidator::validateName(
                     false, false, std::string_view("123abc"))
                     .ok());
    EXPECT_FALSE(arangodb::CollectionNameValidator::validateName(
                     false, false, std::string_view("123"))
                     .ok());
    EXPECT_FALSE(arangodb::CollectionNameValidator::validateName(
                     false, false, std::string_view("_123"))
                     .ok());
    EXPECT_FALSE(arangodb::CollectionNameValidator::validateName(
                     false, false, std::string_view("_abc"))
                     .ok());
    EXPECT_FALSE(arangodb::CollectionNameValidator::validateName(
                     false, false, std::string_view("_"))
                     .ok());
    EXPECT_FALSE(arangodb::CollectionNameValidator::validateName(
                     false, false, std::string_view(":"))
                     .ok());
    EXPECT_FALSE(arangodb::CollectionNameValidator::validateName(
                     false, false, std::string_view("abc:d"))
                     .ok());
    EXPECT_TRUE(arangodb::CollectionNameValidator::validateName(
                    false, false, std::string_view(borderline))
                    .ok());
    EXPECT_FALSE(arangodb::CollectionNameValidator::validateName(
                     false, false, std::string_view(borderlineSystem))
                     .ok());
    EXPECT_FALSE(arangodb::CollectionNameValidator::validateName(
                     false, false, std::string_view(tooLong))
                     .ok());
  }

  // direct (system)
  {
    EXPECT_FALSE(arangodb::CollectionNameValidator::validateName(
                     true, false, std::string_view(nullptr, 0))
                     .ok());
    EXPECT_FALSE(arangodb::CollectionNameValidator::validateName(
                     true, false, std::string_view(""))
                     .ok());
    EXPECT_TRUE(arangodb::CollectionNameValidator::validateName(
                    true, false, std::string_view("abc123"))
                    .ok());
    EXPECT_FALSE(arangodb::CollectionNameValidator::validateName(
                     true, false, std::string_view("123abc"))
                     .ok());
    EXPECT_FALSE(arangodb::CollectionNameValidator::validateName(
                     true, false, std::string_view("123"))
                     .ok());
    EXPECT_TRUE(arangodb::CollectionNameValidator::validateName(
                    true, false, std::string_view("_123"))
                    .ok());
    EXPECT_TRUE(arangodb::CollectionNameValidator::validateName(
                    true, false, std::string_view("_abc"))
                    .ok());
    EXPECT_TRUE(arangodb::CollectionNameValidator::validateName(
                    true, false, std::string_view(borderline))
                    .ok());
    EXPECT_TRUE(arangodb::CollectionNameValidator::validateName(
                    true, false, std::string_view(borderlineSystem))
                    .ok());
    EXPECT_FALSE(arangodb::CollectionNameValidator::validateName(
                     true, false, std::string_view(tooLong))
                     .ok());
  }

  // special characters
  EXPECT_FALSE(arangodb::CollectionNameValidator::validateName(
                   true, false, std::string_view(" a + & ? = abc "))
                   .ok());
  EXPECT_FALSE(arangodb::CollectionNameValidator::validateName(
                   true, false, std::string_view("<script>alert(1);"))
                   .ok());
  EXPECT_FALSE(arangodb::CollectionNameValidator::validateName(
                   true, false, std::string_view("a b c"))
                   .ok());
  EXPECT_FALSE(arangodb::CollectionNameValidator::validateName(
                   true, false, std::string_view("test123 & ' \" < > abc"))
                   .ok());

  EXPECT_FALSE(arangodb::CollectionNameValidator::validateName(
                   true, false, std::string_view("abc:cde"))
                   .ok());
  EXPECT_FALSE(arangodb::CollectionNameValidator::validateName(
                   true, false, std::string_view(".abc"))
                   .ok());
  EXPECT_FALSE(arangodb::CollectionNameValidator::validateName(
                   true, false, std::string_view("a\0b", 3))
                   .ok());

  // spaces
  EXPECT_FALSE(arangodb::CollectionNameValidator::validateName(
                   true, false, std::string_view(" a"))
                   .ok());
  EXPECT_FALSE(arangodb::CollectionNameValidator::validateName(
                   true, false, std::string_view("a "))
                   .ok());
  EXPECT_FALSE(arangodb::CollectionNameValidator::validateName(
                   true, false, std::string_view("a  b"))
                   .ok());

  // unicode
  EXPECT_FALSE(arangodb::CollectionNameValidator::validateName(
                   true, false, std::string_view("mötör"))
                   .ok());
  EXPECT_FALSE(arangodb::CollectionNameValidator::validateName(
                   true, false, std::string_view("😀"))
                   .ok());
  EXPECT_FALSE(arangodb::CollectionNameValidator::validateName(
                   true, false, std::string_view("😀 🍺"))
                   .ok());
  EXPECT_FALSE(arangodb::CollectionNameValidator::validateName(
                   true, false, std::string_view("maçã"))
                   .ok());
  EXPECT_FALSE(arangodb::CollectionNameValidator::validateName(
                   true, false, std::string_view("ﻚﻠﺑ ﻞﻄﻴﻓ"))
                   .ok());
}

TEST(CollectionNameValidatorTest, test_isAllowedName_extendedNames) {
  // direct (non-system)
  std::string const borderline(256, 'x');
  std::string const borderlineSystem(std::string("_") +
                                     std::string(borderline.size() - 1, 'x'));
  std::string const tooLong(borderline.size() + 1, 'x');
  {
    EXPECT_FALSE(arangodb::CollectionNameValidator::validateName(
                     false, true, std::string_view(nullptr, 0))
                     .ok());
    EXPECT_FALSE(arangodb::CollectionNameValidator::validateName(
                     false, true, std::string_view(""))
                     .ok());
    EXPECT_TRUE(arangodb::CollectionNameValidator::validateName(
                    false, true, std::string_view("abc123"))
                    .ok());
    EXPECT_FALSE(arangodb::CollectionNameValidator::validateName(
                     false, true, std::string_view("123abc"))
                     .ok());
    EXPECT_FALSE(arangodb::CollectionNameValidator::validateName(
                     false, true, std::string_view("123"))
                     .ok());
    EXPECT_FALSE(arangodb::CollectionNameValidator::validateName(
                     false, true, std::string_view("_123"))
                     .ok());
    EXPECT_FALSE(arangodb::CollectionNameValidator::validateName(
                     false, true, std::string_view("_abc"))
                     .ok());
    EXPECT_FALSE(arangodb::CollectionNameValidator::validateName(
                     false, true, std::string_view("_"))
                     .ok());
    EXPECT_TRUE(arangodb::CollectionNameValidator::validateName(
                    false, true, std::string_view(":"))
                    .ok());
    EXPECT_TRUE(arangodb::CollectionNameValidator::validateName(
                    false, true, std::string_view("abc:d"))
                    .ok());
    EXPECT_TRUE(arangodb::CollectionNameValidator::validateName(
                    false, true, std::string_view(borderline))
                    .ok());
    EXPECT_FALSE(arangodb::CollectionNameValidator::validateName(
                     false, true, std::string_view(tooLong))
                     .ok());
  }

  // direct (system)
  {
    EXPECT_FALSE(arangodb::CollectionNameValidator::validateName(
                     true, true, std::string_view(nullptr, 0))
                     .ok());
    EXPECT_FALSE(arangodb::CollectionNameValidator::validateName(
                     true, true, std::string_view(""))
                     .ok());
    EXPECT_TRUE(arangodb::CollectionNameValidator::validateName(
                    true, true, std::string_view("abc123"))
                    .ok());
    EXPECT_FALSE(arangodb::CollectionNameValidator::validateName(
                     true, true, std::string_view("123abc"))
                     .ok());
    EXPECT_FALSE(arangodb::CollectionNameValidator::validateName(
                     true, true, std::string_view("123"))
                     .ok());
    EXPECT_TRUE(arangodb::CollectionNameValidator::validateName(
                    true, true, std::string_view("_123"))
                    .ok());
    EXPECT_TRUE(arangodb::CollectionNameValidator::validateName(
                    true, true, std::string_view("_abc"))
                    .ok());
    EXPECT_TRUE(arangodb::CollectionNameValidator::validateName(
                    true, true, std::string_view(borderline))
                    .ok());
    EXPECT_FALSE(arangodb::CollectionNameValidator::validateName(
                     true, true, std::string_view(tooLong))
                     .ok());
  }

  // special characters
  EXPECT_TRUE(arangodb::CollectionNameValidator::validateName(
                  true, true, std::string_view("a + & ? = abc"))
                  .ok());
  EXPECT_TRUE(arangodb::CollectionNameValidator::validateName(
                  true, true, std::string_view("<script>alert(1);"))
                  .ok());
  EXPECT_TRUE(arangodb::CollectionNameValidator::validateName(
                  true, true, std::string_view("a b c"))
                  .ok());
  EXPECT_TRUE(arangodb::CollectionNameValidator::validateName(
                  true, true, std::string_view("test123 & ' \" < > abc"))
                  .ok());

  EXPECT_TRUE(arangodb::CollectionNameValidator::validateName(
                  true, true, std::string_view("abc:cde"))
                  .ok());
  EXPECT_FALSE(arangodb::CollectionNameValidator::validateName(
                   true, true, std::string_view(".abc"))
                   .ok());
  EXPECT_FALSE(arangodb::CollectionNameValidator::validateName(
                   true, true, std::string_view("a\0b", 3))
                   .ok());
  EXPECT_FALSE(arangodb::CollectionNameValidator::validateName(
                   true, true, std::string_view("/"))
                   .ok());
  EXPECT_FALSE(arangodb::CollectionNameValidator::validateName(
                   true, true, std::string_view("a/"))
                   .ok());
  EXPECT_FALSE(arangodb::CollectionNameValidator::validateName(
                   true, true, std::string_view("a/b"))
                   .ok());
  EXPECT_TRUE(arangodb::CollectionNameValidator::validateName(
                  true, true, std::string_view("a\\b"))
                  .ok());
  EXPECT_TRUE(arangodb::CollectionNameValidator::validateName(
                  true, true, std::string_view("a.b.c"))
                  .ok());

  // spaces
  EXPECT_FALSE(arangodb::CollectionNameValidator::validateName(
                   true, true, std::string_view(" a"))
                   .ok());
  EXPECT_FALSE(arangodb::CollectionNameValidator::validateName(
                   true, true, std::string_view("a "))
                   .ok());
  EXPECT_TRUE(arangodb::CollectionNameValidator::validateName(
                  true, true, std::string_view("a  b"))
                  .ok());

  // unicode
  EXPECT_TRUE(arangodb::CollectionNameValidator::validateName(
                  true, true, std::string_view("mötör"))
                  .ok());
  EXPECT_TRUE(arangodb::CollectionNameValidator::validateName(
                  true, true, std::string_view("😀"))
                  .ok());
  EXPECT_TRUE(arangodb::CollectionNameValidator::validateName(
                  true, true, std::string_view("😀 🍺"))
                  .ok());
  EXPECT_TRUE(arangodb::CollectionNameValidator::validateName(
                  true, true, std::string_view("maçã"))
                  .ok());
  EXPECT_TRUE(arangodb::CollectionNameValidator::validateName(
                  true, true, std::string_view("ﻚﻠﺑ ﻞﻄﻴﻓ"))
                  .ok());
}

TEST(IndexNameValidatorTest, test_isAllowedName_traditionalNames) {
  // direct (non-system)
  std::string const borderline(256, 'x');
  std::string const borderlineSystem(std::string("_") +
                                     std::string(borderline.size() - 1, 'x'));
  std::string const tooLong(borderline.size() + 1, 'x');

  EXPECT_FALSE(arangodb::IndexNameValidator::validateName(
                   false, std::string_view(nullptr, 0))
                   .ok());
  EXPECT_FALSE(
      arangodb::IndexNameValidator::validateName(false, std::string_view(""))
          .ok());
  EXPECT_TRUE(arangodb::IndexNameValidator::validateName(
                  false, std::string_view("abc123"))
                  .ok());
  EXPECT_TRUE(arangodb::IndexNameValidator::validateName(
                  false, std::string_view("Abc123"))
                  .ok());
  EXPECT_FALSE(arangodb::IndexNameValidator::validateName(
                   false, std::string_view("123abc"))
                   .ok());
  EXPECT_FALSE(
      arangodb::IndexNameValidator::validateName(false, std::string_view("123"))
          .ok());
  EXPECT_FALSE(arangodb::IndexNameValidator::validateName(
                   false, std::string_view("_123"))
                   .ok());
  EXPECT_FALSE(arangodb::IndexNameValidator::validateName(
                   false, std::string_view("_abc"))
                   .ok());
  EXPECT_TRUE(arangodb::IndexNameValidator::validateName(
                  false, std::string_view("abc_"))
                  .ok());
  EXPECT_TRUE(arangodb::IndexNameValidator::validateName(
                  false, std::string_view("abc-"))
                  .ok());
  EXPECT_FALSE(
      arangodb::IndexNameValidator::validateName(false, std::string_view("_"))
          .ok());
  EXPECT_FALSE(
      arangodb::IndexNameValidator::validateName(false, std::string_view(":"))
          .ok());
  EXPECT_FALSE(arangodb::IndexNameValidator::validateName(
                   false, std::string_view("abc:d"))
                   .ok());
  EXPECT_TRUE(arangodb::IndexNameValidator::validateName(
                  false, std::string_view(borderline))
                  .ok());
  EXPECT_FALSE(arangodb::IndexNameValidator::validateName(
                   false, std::string_view(borderlineSystem))
                   .ok());
  EXPECT_FALSE(arangodb::IndexNameValidator::validateName(
                   false, std::string_view(tooLong))
                   .ok());

  // special characters
  EXPECT_FALSE(arangodb::IndexNameValidator::validateName(
                   false, std::string_view(" a + & ? = abc "))
                   .ok());
  EXPECT_FALSE(arangodb::IndexNameValidator::validateName(
                   false, std::string_view("<script>alert(1);"))
                   .ok());
  EXPECT_FALSE(arangodb::IndexNameValidator::validateName(
                   false, std::string_view("a b c"))
                   .ok());
  EXPECT_FALSE(arangodb::IndexNameValidator::validateName(
                   false, std::string_view("test123 & ' \" < > abc"))
                   .ok());

  EXPECT_FALSE(arangodb::IndexNameValidator::validateName(
                   false, std::string_view("abc:cde"))
                   .ok());
  EXPECT_FALSE(arangodb::IndexNameValidator::validateName(
                   false, std::string_view(".abc"))
                   .ok());
  EXPECT_FALSE(arangodb::IndexNameValidator::validateName(
                   false, std::string_view("a\0b", 3))
                   .ok());
  EXPECT_FALSE(
      arangodb::IndexNameValidator::validateName(false, std::string_view("a/b"))
          .ok());
  EXPECT_FALSE(
      arangodb::IndexNameValidator::validateName(false, std::string_view("//"))
          .ok());
  EXPECT_FALSE(
      arangodb::IndexNameValidator::validateName(false, std::string_view("/\\"))
          .ok());

  // spaces
  EXPECT_FALSE(
      arangodb::IndexNameValidator::validateName(false, std::string_view(" a"))
          .ok());
  EXPECT_FALSE(
      arangodb::IndexNameValidator::validateName(false, std::string_view("a "))
          .ok());
  EXPECT_FALSE(arangodb::IndexNameValidator::validateName(
                   false, std::string_view("a  b"))
                   .ok());

  // unicode
  EXPECT_FALSE(arangodb::IndexNameValidator::validateName(
                   false, std::string_view("mötör"))
                   .ok());
  EXPECT_FALSE(
      arangodb::IndexNameValidator::validateName(false, std::string_view("😀"))
          .ok());
  EXPECT_FALSE(
      arangodb::IndexNameValidator::validateName(false, std::string_view("😀 🍺"))
          .ok());
  EXPECT_FALSE(arangodb::IndexNameValidator::validateName(
                   false, std::string_view("maçã"))
                   .ok());
  EXPECT_FALSE(arangodb::IndexNameValidator::validateName(
                   false, std::string_view("ﻚﻠﺑ ﻞﻄﻴﻓ"))
                   .ok());
}

TEST(IndexNameValidatorTest, test_isAllowedName_extendedNames) {
  // direct (non-system)
  std::string const borderline(256, 'x');
  std::string const borderlineSystem(std::string("_") +
                                     std::string(borderline.size() - 1, 'x'));
  std::string const tooLong(borderline.size() + 1, 'x');

  EXPECT_FALSE(arangodb::IndexNameValidator::validateName(
                   true, std::string_view(nullptr, 0))
                   .ok());
  EXPECT_FALSE(
      arangodb::IndexNameValidator::validateName(true, std::string_view(""))
          .ok());
  EXPECT_TRUE(arangodb::IndexNameValidator::validateName(
                  true, std::string_view("abc123"))
                  .ok());
  EXPECT_TRUE(arangodb::IndexNameValidator::validateName(
                  true, std::string_view("Abc123"))
                  .ok());
  EXPECT_FALSE(arangodb::IndexNameValidator::validateName(
                   true, std::string_view("123abc"))
                   .ok());
  EXPECT_FALSE(
      arangodb::IndexNameValidator::validateName(true, std::string_view("123"))
          .ok());
  EXPECT_TRUE(
      arangodb::IndexNameValidator::validateName(true, std::string_view("_123"))
          .ok());
  EXPECT_TRUE(
      arangodb::IndexNameValidator::validateName(true, std::string_view("_abc"))
          .ok());
  EXPECT_TRUE(
      arangodb::IndexNameValidator::validateName(true, std::string_view("abc_"))
          .ok());
  EXPECT_TRUE(
      arangodb::IndexNameValidator::validateName(true, std::string_view("abc-"))
          .ok());
  EXPECT_TRUE(arangodb::IndexNameValidator::validateName(
                  true, std::string_view(borderline))
                  .ok());
  EXPECT_TRUE(arangodb::IndexNameValidator::validateName(
                  true, std::string_view(borderlineSystem))
                  .ok());
  EXPECT_FALSE(arangodb::IndexNameValidator::validateName(
                   true, std::string_view(tooLong))
                   .ok());

  // special characters
  EXPECT_TRUE(arangodb::IndexNameValidator::validateName(
                  true, std::string_view("a + & ? = abc"))
                  .ok());
  EXPECT_TRUE(arangodb::IndexNameValidator::validateName(
                  true, std::string_view("<script>alert(1);"))
                  .ok());
  EXPECT_TRUE(arangodb::IndexNameValidator::validateName(
                  true, std::string_view("a b c"))
                  .ok());
  EXPECT_TRUE(arangodb::IndexNameValidator::validateName(
                  true, std::string_view("test123 & ' \" < > abc"))
                  .ok());
  EXPECT_TRUE(arangodb::IndexNameValidator::validateName(
                  true, std::string_view("abc:cde"))
                  .ok());
  EXPECT_TRUE(
      arangodb::IndexNameValidator::validateName(true, std::string_view(".abc"))
          .ok());
  EXPECT_FALSE(arangodb::IndexNameValidator::validateName(
                   true, std::string_view("a\0b", 3))
                   .ok());
  EXPECT_FALSE(
      arangodb::IndexNameValidator::validateName(true, std::string_view("/"))
          .ok());
  EXPECT_FALSE(
      arangodb::IndexNameValidator::validateName(true, std::string_view("/\\"))
          .ok());
  EXPECT_FALSE(
      arangodb::IndexNameValidator::validateName(true, std::string_view("a/"))
          .ok());
  EXPECT_FALSE(
      arangodb::IndexNameValidator::validateName(true, std::string_view("a/b"))
          .ok());
  EXPECT_TRUE(
      arangodb::IndexNameValidator::validateName(true, std::string_view("a\\b"))
          .ok());
  EXPECT_TRUE(arangodb::IndexNameValidator::validateName(
                  true, std::string_view("a.b.c"))
                  .ok());

  // spaces
  EXPECT_FALSE(
      arangodb::IndexNameValidator::validateName(true, std::string_view(" a"))
          .ok());
  EXPECT_FALSE(
      arangodb::IndexNameValidator::validateName(true, std::string_view("a "))
          .ok());
  EXPECT_TRUE(
      arangodb::IndexNameValidator::validateName(true, std::string_view("a  b"))
          .ok());

  // unicode
  EXPECT_TRUE(arangodb::IndexNameValidator::validateName(
                  true, std::string_view("mötör"))
                  .ok());
  EXPECT_TRUE(
      arangodb::IndexNameValidator::validateName(true, std::string_view("😀"))
          .ok());
  EXPECT_TRUE(
      arangodb::IndexNameValidator::validateName(true, std::string_view("😀 🍺"))
          .ok());
  EXPECT_TRUE(
      arangodb::IndexNameValidator::validateName(true, std::string_view("maçã"))
          .ok());
  EXPECT_TRUE(arangodb::IndexNameValidator::validateName(
                  true, std::string_view("ﻚﻠﺑ ﻞﻄﻴﻓ"))
                  .ok());
}

TEST(AnalyzerNameValidatorTest, test_isAllowedName_traditionalNames) {
  // direct (non-system)
  std::string const borderline(64, 'x');
  std::string const borderlineSystem(std::string("_") +
                                     std::string(borderline.size() - 1, 'x'));
  std::string const tooLong(borderline.size() + 1, 'x');

  EXPECT_FALSE(arangodb::AnalyzerNameValidator::validateName(
                   false, std::string_view(nullptr, 0))
                   .ok());
  EXPECT_FALSE(
      arangodb::AnalyzerNameValidator::validateName(false, std::string_view(""))
          .ok());
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::validateName(
                  false, std::string_view("abc123"))
                  .ok());
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::validateName(
                  false, std::string_view("Abc123"))
                  .ok());
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::validateName(
                   false, std::string_view("123abc"))
                   .ok());
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::validateName(
                   false, std::string_view("123"))
                   .ok());
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::validateName(
                   false, std::string_view("_123"))
                   .ok());
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::validateName(
                   false, std::string_view("_abc"))
                   .ok());
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::validateName(
                  false, std::string_view("abc_"))
                  .ok());
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::validateName(
                  false, std::string_view("abc-"))
                  .ok());
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::validateName(
                   false, std::string_view("_"))
                   .ok());
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::validateName(
                   false, std::string_view(":"))
                   .ok());
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::validateName(
                   false, std::string_view("abc:d"))
                   .ok());
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::validateName(
                  false, std::string_view(borderline))
                  .ok());
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::validateName(
                   false, std::string_view(borderlineSystem))
                   .ok());
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::validateName(
                   false, std::string_view(tooLong))
                   .ok());

  // special characters
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::validateName(
                   false, std::string_view(" a + & ? = abc "))
                   .ok());
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::validateName(
                   false, std::string_view("<script>alert(1);"))
                   .ok());
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::validateName(
                   false, std::string_view("a b c"))
                   .ok());
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::validateName(
                   false, std::string_view("test123 & ' \" < > abc"))
                   .ok());

  EXPECT_FALSE(arangodb::AnalyzerNameValidator::validateName(
                   false, std::string_view("abc:cde"))
                   .ok());
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::validateName(
                   false, std::string_view(".abc"))
                   .ok());
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::validateName(
                   false, std::string_view("a\0b", 3))
                   .ok());
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::validateName(
                   false, std::string_view("a/b"))
                   .ok());
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::validateName(
                   false, std::string_view("//"))
                   .ok());
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::validateName(
                   false, std::string_view("/\\"))
                   .ok());
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::validateName(
                   false, std::string_view("a:b"))
                   .ok());
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::validateName(
                   false, std::string_view(":"))
                   .ok());
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::validateName(
                   false, std::string_view("aaaa::"))
                   .ok());
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::validateName(
                   false, std::string_view(":aaaa"))
                   .ok());
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::validateName(
                   false, std::string_view("abcdef::gghh"))
                   .ok());

  // unicode
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::validateName(
                   false, std::string_view("mötör"))
                   .ok());
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::validateName(
                   false, std::string_view("😀"))
                   .ok());
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::validateName(
                   false, std::string_view("😀 🍺"))
                   .ok());
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::validateName(
                   false, std::string_view("maçã"))
                   .ok());
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::validateName(
                   false, std::string_view("ﻚﻠﺑ ﻞﻄﻴﻓ"))
                   .ok());
}

TEST(AnalyzerNameValidatorTest, test_isAllowedName_extendedNames) {
  // direct (non-system)
  std::string const borderline(64, 'x');
  std::string const borderlineSystem(std::string("_") +
                                     std::string(borderline.size() - 1, 'x'));
  std::string const tooLong(borderline.size() + 1, 'x');

  EXPECT_FALSE(arangodb::AnalyzerNameValidator::validateName(
                   true, std::string_view(nullptr, 0))
                   .ok());
  EXPECT_FALSE(
      arangodb::AnalyzerNameValidator::validateName(true, std::string_view(""))
          .ok());
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::validateName(
                  true, std::string_view("abc123"))
                  .ok());
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::validateName(
                  true, std::string_view("Abc123"))
                  .ok());
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::validateName(
                   true, std::string_view("123abc"))
                   .ok());
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::validateName(
                   true, std::string_view("123"))
                   .ok());
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::validateName(
                  true, std::string_view("_123"))
                  .ok());
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::validateName(
                  true, std::string_view("_abc"))
                  .ok());
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::validateName(
                  true, std::string_view("abc_"))
                  .ok());
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::validateName(
                  true, std::string_view("abc-"))
                  .ok());
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::validateName(
                  true, std::string_view(borderline))
                  .ok());
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::validateName(
                  true, std::string_view(borderlineSystem))
                  .ok());
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::validateName(
                   true, std::string_view(tooLong))
                   .ok());

  // special characters
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::validateName(
                  true, std::string_view(" a + & ? = abc "))
                  .ok());
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::validateName(
                  true, std::string_view("<script>alert(1);"))
                  .ok());
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::validateName(
                  true, std::string_view("a b c"))
                  .ok());
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::validateName(
                  true, std::string_view("test123 & ' \" < > abc"))
                  .ok());
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::validateName(
                  true, std::string_view(".abc"))
                  .ok());
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::validateName(
                   true, std::string_view("a\0b", 3))
                   .ok());
  EXPECT_FALSE(
      arangodb::AnalyzerNameValidator::validateName(true, std::string_view("/"))
          .ok());
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::validateName(
                   true, std::string_view("/\\"))
                   .ok());
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::validateName(
                   true, std::string_view("a/"))
                   .ok());
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::validateName(
                   true, std::string_view("a/b"))
                   .ok());
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::validateName(
                  true, std::string_view("a\\b"))
                  .ok());
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::validateName(
                  true, std::string_view("a.b.c"))
                  .ok());
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::validateName(
                   true, std::string_view("a:b"))
                   .ok());
  EXPECT_FALSE(
      arangodb::AnalyzerNameValidator::validateName(true, std::string_view(":"))
          .ok());
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::validateName(
                   true, std::string_view("abc:cde"))
                   .ok());
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::validateName(
                   true, std::string_view("aaaa::"))
                   .ok());
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::validateName(
                   true, std::string_view(":aaaa"))
                   .ok());
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::validateName(
                   true, std::string_view("abcdef::gghh"))
                   .ok());

  // unicode
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::validateName(
                  true, std::string_view("mötör"))
                  .ok());
  EXPECT_TRUE(
      arangodb::AnalyzerNameValidator::validateName(true, std::string_view("😀"))
          .ok());
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::validateName(
                  true, std::string_view("😀 🍺"))
                  .ok());
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::validateName(
                  true, std::string_view("maçã"))
                  .ok());
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::validateName(
                  true, std::string_view("ﻚﻠﺑ ﻞﻄﻴﻓ"))
                  .ok());
}

TEST(ViewNameValidatorTest, test_isAllowedName_traditionalNames) {
  // direct (non-system)
  std::string const borderline(64, 'x');
  std::string const borderlineSystem(std::string("_") +
                                     std::string(borderline.size() - 1, 'x'));
  std::string const tooLong(borderline.size() + 1, 'x');

  {
    EXPECT_FALSE(arangodb::ViewNameValidator::validateName(
                     false, false, std::string_view(nullptr, 0))
                     .ok());
    EXPECT_FALSE(arangodb::ViewNameValidator::validateName(false, false,
                                                           std::string_view(""))
                     .ok());
    EXPECT_TRUE(arangodb::ViewNameValidator::validateName(
                    false, false, std::string_view("abc123"))
                    .ok());
    EXPECT_TRUE(arangodb::ViewNameValidator::validateName(
                    false, false, std::string_view("Abc123"))
                    .ok());
    EXPECT_FALSE(arangodb::ViewNameValidator::validateName(
                     false, false, std::string_view("123abc"))
                     .ok());
    EXPECT_FALSE(arangodb::ViewNameValidator::validateName(
                     false, false, std::string_view("123"))
                     .ok());
    EXPECT_FALSE(arangodb::ViewNameValidator::validateName(
                     false, false, std::string_view("_123"))
                     .ok());
    EXPECT_FALSE(arangodb::ViewNameValidator::validateName(
                     false, false, std::string_view("_abc"))
                     .ok());
    EXPECT_FALSE(arangodb::ViewNameValidator::validateName(
                     false, false, std::string_view("_"))
                     .ok());
    EXPECT_FALSE(arangodb::ViewNameValidator::validateName(
                     false, false, std::string_view(":"))
                     .ok());
    EXPECT_FALSE(arangodb::ViewNameValidator::validateName(
                     false, false, std::string_view("abc:d"))
                     .ok());
    EXPECT_TRUE(arangodb::ViewNameValidator::validateName(
                    false, false, std::string_view(borderline))
                    .ok());
    EXPECT_FALSE(arangodb::ViewNameValidator::validateName(
                     false, false, std::string_view(borderlineSystem))
                     .ok());
    EXPECT_FALSE(arangodb::ViewNameValidator::validateName(
                     false, false, std::string_view(tooLong))
                     .ok());
  }

  // direct (system)
  {
    EXPECT_FALSE(arangodb::ViewNameValidator::validateName(
                     true, false, std::string_view(nullptr, 0))
                     .ok());
    EXPECT_FALSE(arangodb::ViewNameValidator::validateName(true, false,
                                                           std::string_view(""))
                     .ok());
    EXPECT_TRUE(arangodb::ViewNameValidator::validateName(
                    true, false, std::string_view("abc123"))
                    .ok());
    EXPECT_TRUE(arangodb::ViewNameValidator::validateName(
                    true, false, std::string_view("Abc123"))
                    .ok());
    EXPECT_FALSE(arangodb::ViewNameValidator::validateName(
                     true, false, std::string_view("123abc"))
                     .ok());
    EXPECT_FALSE(arangodb::ViewNameValidator::validateName(
                     true, false, std::string_view("123"))
                     .ok());
    EXPECT_TRUE(arangodb::ViewNameValidator::validateName(
                    true, false, std::string_view("_123"))
                    .ok());
    EXPECT_TRUE(arangodb::ViewNameValidator::validateName(
                    true, false, std::string_view("_abc"))
                    .ok());
    EXPECT_TRUE(arangodb::ViewNameValidator::validateName(
                    true, false, std::string_view(borderline))
                    .ok());
    EXPECT_TRUE(arangodb::ViewNameValidator::validateName(
                    true, false, std::string_view(borderlineSystem))
                    .ok());
    EXPECT_FALSE(arangodb::ViewNameValidator::validateName(
                     true, false, std::string_view(tooLong))
                     .ok());
  }

  // special characters
  EXPECT_FALSE(arangodb::ViewNameValidator::validateName(
                   true, false, std::string_view(" a + & ? = abc "))
                   .ok());
  EXPECT_FALSE(arangodb::ViewNameValidator::validateName(
                   true, false, std::string_view("<script>alert(1);"))
                   .ok());
  EXPECT_FALSE(arangodb::ViewNameValidator::validateName(
                   true, false, std::string_view("a b c"))
                   .ok());
  EXPECT_FALSE(arangodb::ViewNameValidator::validateName(
                   true, false, std::string_view("test123 & ' \" < > abc"))
                   .ok());

  EXPECT_FALSE(arangodb::ViewNameValidator::validateName(
                   true, false, std::string_view("abc:cde"))
                   .ok());
  EXPECT_FALSE(arangodb::ViewNameValidator::validateName(
                   true, false, std::string_view(".abc"))
                   .ok());
  EXPECT_FALSE(arangodb::ViewNameValidator::validateName(
                   true, false, std::string_view(".123abc"))
                   .ok());
  EXPECT_FALSE(arangodb::ViewNameValidator::validateName(
                   true, false, std::string_view("a\0b", 3))
                   .ok());

  // spaces
  EXPECT_FALSE(arangodb::ViewNameValidator::validateName(true, false,
                                                         std::string_view(" a"))
                   .ok());
  EXPECT_FALSE(arangodb::ViewNameValidator::validateName(true, false,
                                                         std::string_view("a "))
                   .ok());
  EXPECT_FALSE(arangodb::ViewNameValidator::validateName(
                   true, false, std::string_view("a  b"))
                   .ok());

  // unicode
  EXPECT_FALSE(arangodb::ViewNameValidator::validateName(
                   true, false, std::string_view("mötör"))
                   .ok());
  EXPECT_FALSE(arangodb::ViewNameValidator::validateName(true, false,
                                                         std::string_view("😀"))
                   .ok());
  EXPECT_FALSE(arangodb::ViewNameValidator::validateName(
                   true, false, std::string_view("😀 🍺"))
                   .ok());
  EXPECT_FALSE(arangodb::ViewNameValidator::validateName(
                   true, false, std::string_view("maçã"))
                   .ok());
  EXPECT_FALSE(arangodb::ViewNameValidator::validateName(
                   true, false, std::string_view("ﻚﻠﺑ ﻞﻄﻴﻓ"))
                   .ok());
}

TEST(ViewNameValidatorTest, test_isAllowedName_extendedNames) {
  // direct (non-system)
  std::string const borderline(256, 'x');
  std::string const borderlineSystem(std::string("_") +
                                     std::string(borderline.size() - 1, 'x'));
  std::string const tooLong(borderline.size() + 1, 'x');

  {
    EXPECT_FALSE(arangodb::ViewNameValidator::validateName(
                     false, true, std::string_view(nullptr, 0))
                     .ok());
    EXPECT_FALSE(arangodb::ViewNameValidator::validateName(false, true,
                                                           std::string_view(""))
                     .ok());
    EXPECT_TRUE(arangodb::ViewNameValidator::validateName(
                    false, true, std::string_view("abc123"))
                    .ok());
    EXPECT_TRUE(arangodb::ViewNameValidator::validateName(
                    false, true, std::string_view("Abc123"))
                    .ok());
    EXPECT_FALSE(arangodb::ViewNameValidator::validateName(
                     false, true, std::string_view("123abc"))
                     .ok());
    EXPECT_FALSE(arangodb::ViewNameValidator::validateName(
                     false, true, std::string_view("123"))
                     .ok());
    EXPECT_FALSE(arangodb::ViewNameValidator::validateName(
                     false, true, std::string_view("_123"))
                     .ok());
    EXPECT_FALSE(arangodb::ViewNameValidator::validateName(
                     false, true, std::string_view("_abc"))
                     .ok());
    EXPECT_FALSE(arangodb::ViewNameValidator::validateName(
                     false, true, std::string_view("_"))
                     .ok());
    EXPECT_TRUE(arangodb::ViewNameValidator::validateName(false, true,
                                                          std::string_view(":"))
                    .ok());
    EXPECT_TRUE(arangodb::ViewNameValidator::validateName(
                    false, true, std::string_view("abc:d"))
                    .ok());
    EXPECT_TRUE(arangodb::ViewNameValidator::validateName(
                    false, true, std::string_view(borderline))
                    .ok());
    EXPECT_FALSE(arangodb::ViewNameValidator::validateName(
                     false, true, std::string_view(tooLong))
                     .ok());
  }

  // direct (system)
  {
    EXPECT_FALSE(arangodb::ViewNameValidator::validateName(
                     true, true, std::string_view(nullptr, 0))
                     .ok());
    EXPECT_FALSE(arangodb::ViewNameValidator::validateName(true, true,
                                                           std::string_view(""))
                     .ok());
    EXPECT_TRUE(arangodb::ViewNameValidator::validateName(
                    true, true, std::string_view("abc123"))
                    .ok());
    EXPECT_TRUE(arangodb::ViewNameValidator::validateName(
                    true, true, std::string_view("Abc123"))
                    .ok());
    EXPECT_FALSE(arangodb::ViewNameValidator::validateName(
                     true, true, std::string_view("123abc"))
                     .ok());
    EXPECT_FALSE(arangodb::ViewNameValidator::validateName(
                     true, true, std::string_view("123"))
                     .ok());
    EXPECT_TRUE(arangodb::ViewNameValidator::validateName(
                    true, true, std::string_view("_123"))
                    .ok());
    EXPECT_TRUE(arangodb::ViewNameValidator::validateName(
                    true, true, std::string_view("_abc"))
                    .ok());
    EXPECT_TRUE(arangodb::ViewNameValidator::validateName(
                    true, true, std::string_view(borderline))
                    .ok());
    EXPECT_FALSE(arangodb::ViewNameValidator::validateName(
                     true, true, std::string_view(tooLong))
                     .ok());
  }

  // special characters
  EXPECT_TRUE(arangodb::ViewNameValidator::validateName(
                  true, true, std::string_view("a + & ? = abc"))
                  .ok());
  EXPECT_TRUE(arangodb::ViewNameValidator::validateName(
                  true, true, std::string_view("<script>alert(1);"))
                  .ok());
  EXPECT_TRUE(arangodb::ViewNameValidator::validateName(
                  true, true, std::string_view("a b c"))
                  .ok());
  EXPECT_TRUE(arangodb::ViewNameValidator::validateName(
                  true, true, std::string_view("test123 & ' \" < > abc"))
                  .ok());

  EXPECT_TRUE(arangodb::ViewNameValidator::validateName(
                  true, true, std::string_view("abc:cde"))
                  .ok());
  EXPECT_FALSE(arangodb::ViewNameValidator::validateName(
                   true, true, std::string_view(".abc"))
                   .ok());
  EXPECT_FALSE(arangodb::ViewNameValidator::validateName(
                   true, true, std::string_view(".123abc"))
                   .ok());
  EXPECT_FALSE(arangodb::ViewNameValidator::validateName(
                   true, true, std::string_view("a\0b", 3))
                   .ok());
  EXPECT_FALSE(arangodb::ViewNameValidator::validateName(true, true,
                                                         std::string_view("/"))
                   .ok());
  EXPECT_FALSE(arangodb::ViewNameValidator::validateName(true, true,
                                                         std::string_view("a/"))
                   .ok());
  EXPECT_FALSE(arangodb::ViewNameValidator::validateName(
                   true, true, std::string_view("a/b"))
                   .ok());
  EXPECT_TRUE(arangodb::ViewNameValidator::validateName(
                  true, true, std::string_view("a\\b"))
                  .ok());
  EXPECT_TRUE(arangodb::ViewNameValidator::validateName(
                  true, true, std::string_view("a.b.c"))
                  .ok());

  // spaces
  EXPECT_FALSE(arangodb::ViewNameValidator::validateName(true, true,
                                                         std::string_view(" a"))
                   .ok());
  EXPECT_FALSE(arangodb::ViewNameValidator::validateName(true, true,
                                                         std::string_view("a "))
                   .ok());
  EXPECT_TRUE(arangodb::ViewNameValidator::validateName(
                  true, true, std::string_view("a  b"))
                  .ok());

  // unicode
  EXPECT_TRUE(arangodb::ViewNameValidator::validateName(
                  true, true, std::string_view("mötör"))
                  .ok());
  EXPECT_TRUE(arangodb::ViewNameValidator::validateName(true, true,
                                                        std::string_view("😀"))
                  .ok());
  EXPECT_TRUE(arangodb::ViewNameValidator::validateName(true, true,
                                                        std::string_view("😀 🍺"))
                  .ok());
  EXPECT_TRUE(arangodb::ViewNameValidator::validateName(
                  true, true, std::string_view("maçã"))
                  .ok());
  EXPECT_TRUE(arangodb::ViewNameValidator::validateName(
                  true, true, std::string_view("ﻚﻠﺑ ﻞﻄﻴﻓ"))
                  .ok());
}
