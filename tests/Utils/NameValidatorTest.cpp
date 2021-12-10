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
  std::string const borderlineSystem(std::string("_") + std::string(borderline.size() - 1, 'x'));
  std::string const tooLong(borderline.size() + 1, 'x');
  {
    EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(false, false, std::string_view(nullptr, 0)));
    EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(false, false, std::string_view("")));
    EXPECT_TRUE(arangodb::DatabaseNameValidator::isAllowedName(false, false, std::string_view("abc123")));
    EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(false, false, std::string_view("123abc")));
    EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(false, false, std::string_view("123")));
    EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(false, false, std::string_view("_123")));
    EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(false, false, std::string_view("_abc")));
    EXPECT_TRUE(arangodb::DatabaseNameValidator::isAllowedName(false, false, std::string_view(borderline)));  
    EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(false, false, std::string_view(borderlineSystem)));  
    EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(false, false, std::string_view(tooLong))); 
  }

  // direct (system)
  {
    EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(true, false, std::string_view(nullptr, 0)));
    EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(true, false, std::string_view("")));
    EXPECT_TRUE(arangodb::DatabaseNameValidator::isAllowedName(true, false, std::string_view("abc123")));
    EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(true, false, std::string_view("123abc")));
    EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(true, false, std::string_view("123")));
    EXPECT_TRUE(arangodb::DatabaseNameValidator::isAllowedName(true, false, std::string_view("_123")));
    EXPECT_TRUE(arangodb::DatabaseNameValidator::isAllowedName(true, false, std::string_view("_abc")));
    EXPECT_TRUE(arangodb::DatabaseNameValidator::isAllowedName(true, false, std::string_view(borderline)));  
    EXPECT_TRUE(arangodb::DatabaseNameValidator::isAllowedName(true, false, std::string_view(borderlineSystem)));  
    EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(true, false, std::string_view(tooLong)));  
  }

  // special characters
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(true, false, std::string_view(" a + & ? = abc ")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(true, false, std::string_view("<script>alert(1);")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(true, false, std::string_view("a b c")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(true, false, std::string_view("test123 & ' \" < > abc")));
  
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(true, false, std::string_view("abc:cde")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(true, false, std::string_view(".abc")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(true, false, std::string_view("a\0b", 3)));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(true, false, std::string_view("/")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(true, false, std::string_view("a/")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(true, false, std::string_view("a/b")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(true, false, std::string_view("a\\b")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(true, false, std::string_view("a.b.c")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(true, false, std::string_view(" a")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(true, false, std::string_view("\na")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(true, false, std::string_view("\ta")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(true, false, std::string_view("\ra")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(true, false, std::string_view("\ba")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(true, false, std::string_view("\fa")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(true, false, std::string_view("a\n")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(true, false, std::string_view("a\t")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(true, false, std::string_view("a\r")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(true, false, std::string_view("a\b")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(true, false, std::string_view("a\f")));
  
  // unicode 
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(true, false, std::string_view("mötör")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(true, false, std::string_view("😀")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(true, false, std::string_view("😀 🍺")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(true, false, std::string_view("maçã")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(true, false, std::string_view("ﻚﻠﺑ ﻞﻄﻴﻓ")));
}

TEST(DatabaseNameValidatorTest, test_isAllowedName_extendedNames) {
  // direct (non-system)
  std::string const borderline(128, 'x');
  std::string const tooLong(borderline.size() + 1, 'x');
  {
    EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(false, true, std::string_view(nullptr, 0)));
    EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(false, true, std::string_view("")));
    EXPECT_TRUE(arangodb::DatabaseNameValidator::isAllowedName(false, true, std::string_view("abc123")));
    EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(false, true, std::string_view("123abc")));
    EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(false, true, std::string_view("123")));
    EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(false, true, std::string_view("_123")));
    EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(false, true, std::string_view("_abc")));
    EXPECT_TRUE(arangodb::DatabaseNameValidator::isAllowedName(false, true, std::string_view(borderline)));  
    EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(false, true, std::string_view(tooLong))); 
  }

  // direct (system)
  {
    EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(true, true, std::string_view(nullptr, 0)));
    EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(true, true, std::string_view("")));
    EXPECT_TRUE(arangodb::DatabaseNameValidator::isAllowedName(true, true, std::string_view("abc123")));
    EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(true, true, std::string_view("123abc")));
    EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(true, true, std::string_view("123")));
    EXPECT_TRUE(arangodb::DatabaseNameValidator::isAllowedName(true, true, std::string_view("_123")));
    EXPECT_TRUE(arangodb::DatabaseNameValidator::isAllowedName(true, true, std::string_view("_abc")));
    EXPECT_TRUE(arangodb::DatabaseNameValidator::isAllowedName(true, true, std::string_view(borderline)));  
    EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(true, true, std::string_view(tooLong)));  
  }
  
  // special characters
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(true, true, std::string_view(" a + & ? = abc ")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(true, true, std::string_view(" a + & ? = abc")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(true, true, std::string_view("a + & ? = abc ")));
  EXPECT_TRUE(arangodb::DatabaseNameValidator::isAllowedName(true, true, std::string_view("a + & ? = abc")));
  EXPECT_TRUE(arangodb::DatabaseNameValidator::isAllowedName(true, true, std::string_view("<script>alert(1);")));
  EXPECT_TRUE(arangodb::DatabaseNameValidator::isAllowedName(true, true, std::string_view("a b c")));
  EXPECT_TRUE(arangodb::DatabaseNameValidator::isAllowedName(true, true, std::string_view("test123 & ' \" < > abc")));
  
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(true, true, std::string_view("abc:cde")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(true, true, std::string_view(".abc")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(true, true, std::string_view("a\0b", 3)));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(true, true, std::string_view("/")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(true, true, std::string_view("a/")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(true, true, std::string_view("a/b")));
  EXPECT_TRUE(arangodb::DatabaseNameValidator::isAllowedName(true, true, std::string_view("a\\b")));
  EXPECT_TRUE(arangodb::DatabaseNameValidator::isAllowedName(true, true, std::string_view("a.b.c")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(true, true, std::string_view("\na")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(true, true, std::string_view("\ta")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(true, true, std::string_view("\ra")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(true, true, std::string_view("\ba")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(true, true, std::string_view("\fa")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(true, true, std::string_view("a\n")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(true, true, std::string_view("a\t")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(true, true, std::string_view("a\r")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(true, true, std::string_view("a\b")));
  EXPECT_FALSE(arangodb::DatabaseNameValidator::isAllowedName(true, true, std::string_view("a\f")));
  
  // unicode 
  EXPECT_TRUE(arangodb::DatabaseNameValidator::isAllowedName(true, true, std::string_view("mötör")));
  EXPECT_TRUE(arangodb::DatabaseNameValidator::isAllowedName(true, true, std::string_view("😀")));
  EXPECT_TRUE(arangodb::DatabaseNameValidator::isAllowedName(true, true, std::string_view("😀 🍺")));
  EXPECT_TRUE(arangodb::DatabaseNameValidator::isAllowedName(true, true, std::string_view("maçã")));
  EXPECT_TRUE(arangodb::DatabaseNameValidator::isAllowedName(true, true, std::string_view("ﻚﻠﺑ ﻞﻄﻴﻓ")));
}

TEST(CollectionNameValidatorTest, test_isAllowedName_traditionalNames) {
  // direct (non-system)
  std::string const borderline(256, 'x');
  std::string const borderlineSystem(std::string("_") + std::string(borderline.size() - 1, 'x'));
  std::string const tooLong(borderline.size() + 1, 'x');
  {
    EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(false, false, std::string_view(nullptr, 0)));
    EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(false, false, std::string_view("")));
    EXPECT_TRUE(arangodb::CollectionNameValidator::isAllowedName(false, false, std::string_view("abc123")));
    EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(false, false, std::string_view("123abc")));
    EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(false, false, std::string_view("123")));
    EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(false, false, std::string_view("_123")));
    EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(false, false, std::string_view("_abc")));
    EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(false, false, std::string_view("_")));
    EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(false, false, std::string_view(":")));
    EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(false, false, std::string_view("abc:d")));
    EXPECT_TRUE(arangodb::CollectionNameValidator::isAllowedName(false, false, std::string_view(borderline)));  
    EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(false, false, std::string_view(borderlineSystem)));  
    EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(false, false, std::string_view(tooLong))); 
  }

  // direct (system)
  {
    EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(true, false, std::string_view(nullptr, 0)));
    EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(true, false, std::string_view("")));
    EXPECT_TRUE(arangodb::CollectionNameValidator::isAllowedName(true, false, std::string_view("abc123")));
    EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(true, false, std::string_view("123abc")));
    EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(true, false, std::string_view("123")));
    EXPECT_TRUE(arangodb::CollectionNameValidator::isAllowedName(true, false, std::string_view("_123")));
    EXPECT_TRUE(arangodb::CollectionNameValidator::isAllowedName(true, false, std::string_view("_abc")));
    EXPECT_TRUE(arangodb::CollectionNameValidator::isAllowedName(true, false, std::string_view(borderline)));  
    EXPECT_TRUE(arangodb::CollectionNameValidator::isAllowedName(true, false, std::string_view(borderlineSystem)));  
    EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(true, false, std::string_view(tooLong)));  
  }

  // special characters
  EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(true, false, std::string_view(" a + & ? = abc ")));
  EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(true, false, std::string_view("<script>alert(1);")));
  EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(true, false, std::string_view("a b c")));
  EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(true, false, std::string_view("test123 & ' \" < > abc")));
  
  EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(true, false, std::string_view("abc:cde")));
  EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(true, false, std::string_view(".abc")));
  EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(true, false, std::string_view("a\0b", 3)));
  
  // unicode 
  EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(true, false, std::string_view("mötör")));
  EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(true, false, std::string_view("😀")));
  EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(true, false, std::string_view("😀 🍺")));
  EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(true, false, std::string_view("maçã")));
  EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(true, false, std::string_view("ﻚﻠﺑ ﻞﻄﻴﻓ")));
}

TEST(CollectionNameValidatorTest, test_isAllowedName_extendedNames) {
  // direct (non-system)
  std::string const borderline(256, 'x');
  std::string const borderlineSystem(std::string("_") + std::string(borderline.size() - 1, 'x'));
  std::string const tooLong(borderline.size() + 1, 'x');
  {
    EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(false, true, std::string_view(nullptr, 0)));
    EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(false, true, std::string_view("")));
    EXPECT_TRUE(arangodb::CollectionNameValidator::isAllowedName(false, true, std::string_view("abc123")));
    EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(false, true, std::string_view("123abc")));
    EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(false, true, std::string_view("123")));
    EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(false, true, std::string_view("_123")));
    EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(false, true, std::string_view("_abc")));
    EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(false, true, std::string_view("_")));
    EXPECT_TRUE(arangodb::CollectionNameValidator::isAllowedName(false, true, std::string_view(":")));
    EXPECT_TRUE(arangodb::CollectionNameValidator::isAllowedName(false, true, std::string_view("abc:d")));
    EXPECT_TRUE(arangodb::CollectionNameValidator::isAllowedName(false, true, std::string_view(borderline)));  
    EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(false, true, std::string_view(tooLong))); 
  }

  // direct (system)
  {
    EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(true, true, std::string_view(nullptr, 0)));
    EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(true, true, std::string_view("")));
    EXPECT_TRUE(arangodb::CollectionNameValidator::isAllowedName(true, true, std::string_view("abc123")));
    EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(true, true, std::string_view("123abc")));
    EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(true, true, std::string_view("123")));
    EXPECT_TRUE(arangodb::CollectionNameValidator::isAllowedName(true, true, std::string_view("_123")));
    EXPECT_TRUE(arangodb::CollectionNameValidator::isAllowedName(true, true, std::string_view("_abc")));
    EXPECT_TRUE(arangodb::CollectionNameValidator::isAllowedName(true, true, std::string_view(borderline)));  
    EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(true, true, std::string_view(tooLong)));  
  }
  
  // special characters
  EXPECT_TRUE(arangodb::CollectionNameValidator::isAllowedName(true, true, std::string_view(" a + & ? = abc ")));
  EXPECT_TRUE(arangodb::CollectionNameValidator::isAllowedName(true, true, std::string_view("<script>alert(1);")));
  EXPECT_TRUE(arangodb::CollectionNameValidator::isAllowedName(true, true, std::string_view("a b c")));
  EXPECT_TRUE(arangodb::CollectionNameValidator::isAllowedName(true, true, std::string_view("test123 & ' \" < > abc")));
  
  EXPECT_TRUE(arangodb::CollectionNameValidator::isAllowedName(true, true, std::string_view("abc:cde")));
  EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(true, true, std::string_view(".abc")));
  EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(true, true, std::string_view("a\0b", 3)));
  EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(true, true, std::string_view("/")));
  EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(true, true, std::string_view("a/")));
  EXPECT_FALSE(arangodb::CollectionNameValidator::isAllowedName(true, true, std::string_view("a/b")));
  EXPECT_TRUE(arangodb::CollectionNameValidator::isAllowedName(true, true, std::string_view("a\\b")));
  EXPECT_TRUE(arangodb::CollectionNameValidator::isAllowedName(true, true, std::string_view("a.b.c")));
  
  // unicode 
  EXPECT_TRUE(arangodb::CollectionNameValidator::isAllowedName(true, true, std::string_view("mötör")));
  EXPECT_TRUE(arangodb::CollectionNameValidator::isAllowedName(true, true, std::string_view("😀")));
  EXPECT_TRUE(arangodb::CollectionNameValidator::isAllowedName(true, true, std::string_view("😀 🍺")));
  EXPECT_TRUE(arangodb::CollectionNameValidator::isAllowedName(true, true, std::string_view("maçã")));
  EXPECT_TRUE(arangodb::CollectionNameValidator::isAllowedName(true, true, std::string_view("ﻚﻠﺑ ﻞﻄﻴﻓ")));
}

TEST(IndexNameValidatorTest, test_isAllowedName_traditionalNames) {
  // direct (non-system)
  std::string const borderline(256, 'x');
  std::string const borderlineSystem(std::string("_") + std::string(borderline.size() - 1, 'x'));
  std::string const tooLong(borderline.size() + 1, 'x');
  EXPECT_FALSE(arangodb::IndexNameValidator::isAllowedName(false,  std::string_view(nullptr, 0)));
  EXPECT_FALSE(arangodb::IndexNameValidator::isAllowedName(false, std::string_view("")));
  EXPECT_TRUE(arangodb::IndexNameValidator::isAllowedName(false, std::string_view("abc123")));
  EXPECT_TRUE(arangodb::IndexNameValidator::isAllowedName(false, std::string_view("Abc123")));
  EXPECT_FALSE(arangodb::IndexNameValidator::isAllowedName(false, std::string_view("123abc")));
  EXPECT_FALSE(arangodb::IndexNameValidator::isAllowedName(false, std::string_view("123")));
  EXPECT_FALSE(arangodb::IndexNameValidator::isAllowedName(false, std::string_view("_123")));
  EXPECT_FALSE(arangodb::IndexNameValidator::isAllowedName(false, std::string_view("_abc")));
  EXPECT_TRUE(arangodb::IndexNameValidator::isAllowedName(false, std::string_view("abc_")));
  EXPECT_TRUE(arangodb::IndexNameValidator::isAllowedName(false, std::string_view("abc-")));
  EXPECT_FALSE(arangodb::IndexNameValidator::isAllowedName(false, std::string_view("_")));
  EXPECT_FALSE(arangodb::IndexNameValidator::isAllowedName(false, std::string_view(":")));
  EXPECT_FALSE(arangodb::IndexNameValidator::isAllowedName(false, std::string_view("abc:d")));
  EXPECT_TRUE(arangodb::IndexNameValidator::isAllowedName(false, std::string_view(borderline)));  
  EXPECT_FALSE(arangodb::IndexNameValidator::isAllowedName(false, std::string_view(borderlineSystem)));  
  EXPECT_FALSE(arangodb::IndexNameValidator::isAllowedName(false, std::string_view(tooLong))); 

  // special characters
  EXPECT_FALSE(arangodb::IndexNameValidator::isAllowedName(false, std::string_view(" a + & ? = abc ")));
  EXPECT_FALSE(arangodb::IndexNameValidator::isAllowedName(false, std::string_view("<script>alert(1);")));
  EXPECT_FALSE(arangodb::IndexNameValidator::isAllowedName(false, std::string_view("a b c")));
  EXPECT_FALSE(arangodb::IndexNameValidator::isAllowedName(false, std::string_view("test123 & ' \" < > abc")));
  
  EXPECT_FALSE(arangodb::IndexNameValidator::isAllowedName(false, std::string_view("abc:cde")));
  EXPECT_FALSE(arangodb::IndexNameValidator::isAllowedName(false, std::string_view(".abc")));
  EXPECT_FALSE(arangodb::IndexNameValidator::isAllowedName(false, std::string_view("a\0b", 3)));
  EXPECT_FALSE(arangodb::IndexNameValidator::isAllowedName(false, std::string_view("a/b")));
  EXPECT_FALSE(arangodb::IndexNameValidator::isAllowedName(false, std::string_view("//")));
  EXPECT_FALSE(arangodb::IndexNameValidator::isAllowedName(false, std::string_view("/\\")));
  
  // unicode 
  EXPECT_FALSE(arangodb::IndexNameValidator::isAllowedName(false, std::string_view("mötör")));
  EXPECT_FALSE(arangodb::IndexNameValidator::isAllowedName(false, std::string_view("😀")));
  EXPECT_FALSE(arangodb::IndexNameValidator::isAllowedName(false, std::string_view("😀 🍺")));
  EXPECT_FALSE(arangodb::IndexNameValidator::isAllowedName(false, std::string_view("maçã")));
  EXPECT_FALSE(arangodb::IndexNameValidator::isAllowedName(false, std::string_view("ﻚﻠﺑ ﻞﻄﻴﻓ")));
}

TEST(IndexNameValidatorTest, test_isAllowedName_extendedNames) {
  // direct (non-system)
  std::string const borderline(256, 'x');
  std::string const borderlineSystem(std::string("_") + std::string(borderline.size() - 1, 'x'));
  std::string const tooLong(borderline.size() + 1, 'x');
  EXPECT_FALSE(arangodb::IndexNameValidator::isAllowedName(true, std::string_view(nullptr, 0)));
  EXPECT_FALSE(arangodb::IndexNameValidator::isAllowedName(true, std::string_view("")));
  EXPECT_TRUE(arangodb::IndexNameValidator::isAllowedName(true, std::string_view("abc123")));
  EXPECT_TRUE(arangodb::IndexNameValidator::isAllowedName(true, std::string_view("Abc123")));
  EXPECT_FALSE(arangodb::IndexNameValidator::isAllowedName(true, std::string_view("123abc")));
  EXPECT_FALSE(arangodb::IndexNameValidator::isAllowedName(true, std::string_view("123")));
  EXPECT_TRUE(arangodb::IndexNameValidator::isAllowedName(true, std::string_view("_123")));
  EXPECT_TRUE(arangodb::IndexNameValidator::isAllowedName(true, std::string_view("_abc")));
  EXPECT_TRUE(arangodb::IndexNameValidator::isAllowedName(true, std::string_view("abc_")));
  EXPECT_TRUE(arangodb::IndexNameValidator::isAllowedName(true, std::string_view("abc-")));
  EXPECT_TRUE(arangodb::IndexNameValidator::isAllowedName(true, std::string_view(borderline)));  
  EXPECT_TRUE(arangodb::IndexNameValidator::isAllowedName(true, std::string_view(borderlineSystem))); 
  EXPECT_FALSE(arangodb::IndexNameValidator::isAllowedName(true, std::string_view(tooLong))); 

  // special characters
  EXPECT_TRUE(arangodb::IndexNameValidator::isAllowedName(true, std::string_view(" a + & ? = abc ")));
  EXPECT_TRUE(arangodb::IndexNameValidator::isAllowedName(true, std::string_view("<script>alert(1);")));
  EXPECT_TRUE(arangodb::IndexNameValidator::isAllowedName(true, std::string_view("a b c")));
  EXPECT_TRUE(arangodb::IndexNameValidator::isAllowedName(true, std::string_view("test123 & ' \" < > abc")));
  EXPECT_TRUE(arangodb::IndexNameValidator::isAllowedName(true, std::string_view("abc:cde")));
  EXPECT_TRUE(arangodb::IndexNameValidator::isAllowedName(true, std::string_view(".abc")));
  EXPECT_FALSE(arangodb::IndexNameValidator::isAllowedName(true, std::string_view("a\0b", 3)));
  EXPECT_FALSE(arangodb::IndexNameValidator::isAllowedName(true, std::string_view("/")));
  EXPECT_FALSE(arangodb::IndexNameValidator::isAllowedName(true, std::string_view("/\\")));
  EXPECT_FALSE(arangodb::IndexNameValidator::isAllowedName(true, std::string_view("a/")));
  EXPECT_FALSE(arangodb::IndexNameValidator::isAllowedName(true, std::string_view("a/b")));
  EXPECT_TRUE(arangodb::IndexNameValidator::isAllowedName(true, std::string_view("a\\b")));
  EXPECT_TRUE(arangodb::IndexNameValidator::isAllowedName(true, std::string_view("a.b.c")));
  
  // unicode 
  EXPECT_TRUE(arangodb::IndexNameValidator::isAllowedName(true, std::string_view("mötör")));
  EXPECT_TRUE(arangodb::IndexNameValidator::isAllowedName(true, std::string_view("😀")));
  EXPECT_TRUE(arangodb::IndexNameValidator::isAllowedName(true, std::string_view("😀 🍺")));
  EXPECT_TRUE(arangodb::IndexNameValidator::isAllowedName(true, std::string_view("maçã")));
  EXPECT_TRUE(arangodb::IndexNameValidator::isAllowedName(true, std::string_view("ﻚﻠﺑ ﻞﻄﻴﻓ")));
}

TEST(AnalyzerNameValidatorTest, test_isAllowedName_traditionalNames) {
  // direct (non-system)
  std::string const borderline(64, 'x');
  std::string const borderlineSystem(std::string("_") + std::string(borderline.size() - 1, 'x'));
  std::string const tooLong(borderline.size() + 1, 'x');
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(false,  std::string_view(nullptr, 0)));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(false, std::string_view("")));
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::isAllowedName(false, std::string_view("abc123")));
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::isAllowedName(false, std::string_view("Abc123")));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(false, std::string_view("123abc")));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(false, std::string_view("123")));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(false, std::string_view("_123")));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(false, std::string_view("_abc")));
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::isAllowedName(false, std::string_view("abc_")));
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::isAllowedName(false, std::string_view("abc-")));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(false, std::string_view("_")));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(false, std::string_view(":")));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(false, std::string_view("abc:d")));
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::isAllowedName(false, std::string_view(borderline)));  
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(false, std::string_view(borderlineSystem)));  
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(false, std::string_view(tooLong))); 

  // special characters
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(false, std::string_view(" a + & ? = abc ")));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(false, std::string_view("<script>alert(1);")));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(false, std::string_view("a b c")));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(false, std::string_view("test123 & ' \" < > abc")));
  
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(false, std::string_view("abc:cde")));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(false, std::string_view(".abc")));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(false, std::string_view("a\0b", 3)));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(false, std::string_view("a/b")));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(false, std::string_view("//")));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(false, std::string_view("/\\")));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(false, std::string_view("a:b")));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(false, std::string_view(":")));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(false, std::string_view("aaaa::")));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(false, std::string_view(":aaaa")));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(false, std::string_view("abcdef::gghh")));
  
  // unicode 
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(false, std::string_view("mötör")));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(false, std::string_view("😀")));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(false, std::string_view("😀 🍺")));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(false, std::string_view("maçã")));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(false, std::string_view("ﻚﻠﺑ ﻞﻄﻴﻓ")));
}

TEST(AnalyzerNameValidatorTest, test_isAllowedName_extendedNames) {
  // direct (non-system)
  std::string const borderline(64, 'x');
  std::string const borderlineSystem(std::string("_") + std::string(borderline.size() - 1, 'x'));
  std::string const tooLong(borderline.size() + 1, 'x');
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(true, std::string_view(nullptr, 0)));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(true, std::string_view("")));
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::isAllowedName(true, std::string_view("abc123")));
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::isAllowedName(true, std::string_view("Abc123")));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(true, std::string_view("123abc")));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(true, std::string_view("123")));
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::isAllowedName(true, std::string_view("_123")));
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::isAllowedName(true, std::string_view("_abc")));
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::isAllowedName(true, std::string_view("abc_")));
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::isAllowedName(true, std::string_view("abc-")));
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::isAllowedName(true, std::string_view(borderline)));  
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::isAllowedName(true, std::string_view(borderlineSystem))); 
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(true, std::string_view(tooLong))); 

  // special characters
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::isAllowedName(true, std::string_view(" a + & ? = abc ")));
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::isAllowedName(true, std::string_view("<script>alert(1);")));
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::isAllowedName(true, std::string_view("a b c")));
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::isAllowedName(true, std::string_view("test123 & ' \" < > abc")));
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::isAllowedName(true, std::string_view(".abc")));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(true, std::string_view("a\0b", 3)));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(true, std::string_view("/")));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(true, std::string_view("/\\")));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(true, std::string_view("a/")));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(true, std::string_view("a/b")));
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::isAllowedName(true, std::string_view("a\\b")));
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::isAllowedName(true, std::string_view("a.b.c")));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(true, std::string_view("a:b")));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(true, std::string_view(":")));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(true, std::string_view("abc:cde")));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(true, std::string_view("aaaa::")));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(true, std::string_view(":aaaa")));
  EXPECT_FALSE(arangodb::AnalyzerNameValidator::isAllowedName(true, std::string_view("abcdef::gghh")));
  
  // unicode 
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::isAllowedName(true, std::string_view("mötör")));
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::isAllowedName(true, std::string_view("😀")));
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::isAllowedName(true, std::string_view("😀 🍺")));
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::isAllowedName(true, std::string_view("maçã")));
  EXPECT_TRUE(arangodb::AnalyzerNameValidator::isAllowedName(true, std::string_view("ﻚﻠﺑ ﻞﻄﻴﻓ")));
}

TEST(ViewNameValidatorTest, test_isAllowedName_traditionalNames) {
  // direct (non-system)
  std::string const borderline(64, 'x');
  std::string const borderlineSystem(std::string("_") + std::string(borderline.size() - 1, 'x'));
  std::string const tooLong(borderline.size() + 1, 'x');
  {
    EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(false, false, std::string_view(nullptr, 0)));
    EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(false, false, std::string_view("")));
    EXPECT_TRUE(arangodb::ViewNameValidator::isAllowedName(false, false, std::string_view("abc123")));
    EXPECT_TRUE(arangodb::ViewNameValidator::isAllowedName(false, false, std::string_view("Abc123")));
    EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(false, false, std::string_view("123abc")));
    EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(false, false, std::string_view("123")));
    EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(false, false, std::string_view("_123")));
    EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(false, false, std::string_view("_abc")));
    EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(false, false, std::string_view("_")));
    EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(false, false, std::string_view(":")));
    EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(false, false, std::string_view("abc:d")));
    EXPECT_TRUE(arangodb::ViewNameValidator::isAllowedName(false, false, std::string_view(borderline)));  
    EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(false, false, std::string_view(borderlineSystem)));  
    EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(false, false, std::string_view(tooLong))); 
  }

  // direct (system)
  {
    EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(true, false, std::string_view(nullptr, 0)));
    EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(true, false, std::string_view("")));
    EXPECT_TRUE(arangodb::ViewNameValidator::isAllowedName(true, false, std::string_view("abc123")));
    EXPECT_TRUE(arangodb::ViewNameValidator::isAllowedName(true, false, std::string_view("Abc123")));
    EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(true, false, std::string_view("123abc")));
    EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(true, false, std::string_view("123")));
    EXPECT_TRUE(arangodb::ViewNameValidator::isAllowedName(true, false, std::string_view("_123")));
    EXPECT_TRUE(arangodb::ViewNameValidator::isAllowedName(true, false, std::string_view("_abc")));
    EXPECT_TRUE(arangodb::ViewNameValidator::isAllowedName(true, false, std::string_view(borderline)));  
    EXPECT_TRUE(arangodb::ViewNameValidator::isAllowedName(true, false, std::string_view(borderlineSystem)));  
    EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(true, false, std::string_view(tooLong)));  
  }

  // special characters
  EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(true, false, std::string_view(" a + & ? = abc ")));
  EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(true, false, std::string_view("<script>alert(1);")));
  EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(true, false, std::string_view("a b c")));
  EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(true, false, std::string_view("test123 & ' \" < > abc")));
  
  EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(true, false, std::string_view("abc:cde")));
  EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(true, false, std::string_view(".abc")));
  EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(true, false, std::string_view(".123abc")));
  EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(true, false, std::string_view("a\0b", 3)));
  
  // unicode 
  EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(true, false, std::string_view("mötör")));
  EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(true, false, std::string_view("😀")));
  EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(true, false, std::string_view("😀 🍺")));
  EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(true, false, std::string_view("maçã")));
  EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(true, false, std::string_view("ﻚﻠﺑ ﻞﻄﻴﻓ")));
}

TEST(ViewNameValidatorTest, test_isAllowedName_extendedNames) {
  // direct (non-system)
  std::string const borderline(256, 'x');
  std::string const borderlineSystem(std::string("_") + std::string(borderline.size() - 1, 'x'));
  std::string const tooLong(borderline.size() + 1, 'x');
  {
    EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(false, true, std::string_view(nullptr, 0)));
    EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(false, true, std::string_view("")));
    EXPECT_TRUE(arangodb::ViewNameValidator::isAllowedName(false, true, std::string_view("abc123")));
    EXPECT_TRUE(arangodb::ViewNameValidator::isAllowedName(false, true, std::string_view("Abc123")));
    EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(false, true, std::string_view("123abc")));
    EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(false, true, std::string_view("123")));
    EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(false, true, std::string_view("_123")));
    EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(false, true, std::string_view("_abc")));
    EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(false, true, std::string_view("_")));
    EXPECT_TRUE(arangodb::ViewNameValidator::isAllowedName(false, true, std::string_view(":")));
    EXPECT_TRUE(arangodb::ViewNameValidator::isAllowedName(false, true, std::string_view("abc:d")));
    EXPECT_TRUE(arangodb::ViewNameValidator::isAllowedName(false, true, std::string_view(borderline)));  
    EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(false, true, std::string_view(tooLong))); 
  }

  // direct (system)
  {
    EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(true, true, std::string_view(nullptr, 0)));
    EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(true, true, std::string_view("")));
    EXPECT_TRUE(arangodb::ViewNameValidator::isAllowedName(true, true, std::string_view("abc123")));
    EXPECT_TRUE(arangodb::ViewNameValidator::isAllowedName(true, true, std::string_view("Abc123")));
    EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(true, true, std::string_view("123abc")));
    EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(true, true, std::string_view("123")));
    EXPECT_TRUE(arangodb::ViewNameValidator::isAllowedName(true, true, std::string_view("_123")));
    EXPECT_TRUE(arangodb::ViewNameValidator::isAllowedName(true, true, std::string_view("_abc")));
    EXPECT_TRUE(arangodb::ViewNameValidator::isAllowedName(true, true, std::string_view(borderline)));  
    EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(true, true, std::string_view(tooLong)));  
  }
  
  // special characters
  EXPECT_TRUE(arangodb::ViewNameValidator::isAllowedName(true, true, std::string_view(" a + & ? = abc ")));
  EXPECT_TRUE(arangodb::ViewNameValidator::isAllowedName(true, true, std::string_view("<script>alert(1);")));
  EXPECT_TRUE(arangodb::ViewNameValidator::isAllowedName(true, true, std::string_view("a b c")));
  EXPECT_TRUE(arangodb::ViewNameValidator::isAllowedName(true, true, std::string_view("test123 & ' \" < > abc")));
  
  EXPECT_TRUE(arangodb::ViewNameValidator::isAllowedName(true, true, std::string_view("abc:cde")));
  EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(true, true, std::string_view(".abc")));
  EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(true, true, std::string_view(".123abc")));
  EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(true, true, std::string_view("a\0b", 3)));
  EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(true, true, std::string_view("/")));
  EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(true, true, std::string_view("a/")));
  EXPECT_FALSE(arangodb::ViewNameValidator::isAllowedName(true, true, std::string_view("a/b")));
  EXPECT_TRUE(arangodb::ViewNameValidator::isAllowedName(true, true, std::string_view("a\\b")));
  EXPECT_TRUE(arangodb::ViewNameValidator::isAllowedName(true, true, std::string_view("a.b.c")));
  
  // unicode 
  EXPECT_TRUE(arangodb::ViewNameValidator::isAllowedName(true, true, std::string_view("mötör")));
  EXPECT_TRUE(arangodb::ViewNameValidator::isAllowedName(true, true, std::string_view("😀")));
  EXPECT_TRUE(arangodb::ViewNameValidator::isAllowedName(true, true, std::string_view("😀 🍺")));
  EXPECT_TRUE(arangodb::ViewNameValidator::isAllowedName(true, true, std::string_view("maçã")));
  EXPECT_TRUE(arangodb::ViewNameValidator::isAllowedName(true, true, std::string_view("ﻚﻠﺑ ﻞﻄﻴﻓ")));
}


