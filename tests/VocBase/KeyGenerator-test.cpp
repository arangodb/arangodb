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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "VocBase/KeyGenerator.h"

#include <iostream>
#include <string>
#include "gtest/gtest.h"

using namespace arangodb;

namespace {
bool validateKeyWrapper(char const* key) {
  return KeyGenerator::validateKey(key, strlen(key));
}

bool validateIdWrapper(char const* id, bool extendedNames, size_t expectedSplit,
                       size_t idLength = 0) {
  size_t split;

  bool result = KeyGenerator::validateId(id, idLength ? idLength : strlen(id),
                                         extendedNames, split);
  EXPECT_EQ(split, expectedSplit);
  return result;
}
}  // namespace

TEST(KeyGeneratorTest, encodePadded) {
  ASSERT_EQ(std::string("0000000000000000"),
            KeyGeneratorHelper::encodePadded(0));
  ASSERT_EQ(std::string("0000000000000001"),
            KeyGeneratorHelper::encodePadded(1));
  ASSERT_EQ(std::string("0000000000000002"),
            KeyGeneratorHelper::encodePadded(2));
  ASSERT_EQ(std::string("0000000000000005"),
            KeyGeneratorHelper::encodePadded(5));
  ASSERT_EQ(std::string("0000000000000009"),
            KeyGeneratorHelper::encodePadded(9));
  ASSERT_EQ(std::string("000000000000000a"),
            KeyGeneratorHelper::encodePadded(10));
  ASSERT_EQ(std::string("000000000000000c"),
            KeyGeneratorHelper::encodePadded(12));
  ASSERT_EQ(std::string("000000000000000f"),
            KeyGeneratorHelper::encodePadded(15));
  ASSERT_EQ(std::string("0000000000000010"),
            KeyGeneratorHelper::encodePadded(16));
  ASSERT_EQ(std::string("0000000000000011"),
            KeyGeneratorHelper::encodePadded(17));
  ASSERT_EQ(std::string("0000000000000019"),
            KeyGeneratorHelper::encodePadded(25));
  ASSERT_EQ(std::string("0000000000000020"),
            KeyGeneratorHelper::encodePadded(32));
  ASSERT_EQ(std::string("0000000000000021"),
            KeyGeneratorHelper::encodePadded(33));
  ASSERT_EQ(std::string("000000000000003f"),
            KeyGeneratorHelper::encodePadded(63));
  ASSERT_EQ(std::string("0000000000000040"),
            KeyGeneratorHelper::encodePadded(64));
  ASSERT_EQ(std::string("000000000000007f"),
            KeyGeneratorHelper::encodePadded(127));
  ASSERT_EQ(std::string("00000000000000ff"),
            KeyGeneratorHelper::encodePadded(255));
  ASSERT_EQ(std::string("0000000000000100"),
            KeyGeneratorHelper::encodePadded(256));
  ASSERT_EQ(std::string("0000000000000101"),
            KeyGeneratorHelper::encodePadded(257));
  ASSERT_EQ(std::string("00000000000001ff"),
            KeyGeneratorHelper::encodePadded(511));
  ASSERT_EQ(std::string("0000000000000200"),
            KeyGeneratorHelper::encodePadded(512));
  ASSERT_EQ(std::string("0000000000001002"),
            KeyGeneratorHelper::encodePadded(4098));
  ASSERT_EQ(std::string("000000000000ffff"),
            KeyGeneratorHelper::encodePadded(65535));
  ASSERT_EQ(std::string("0000000000010000"),
            KeyGeneratorHelper::encodePadded(65536));
  ASSERT_EQ(std::string("0000000007a9f3bf"),
            KeyGeneratorHelper::encodePadded(128578495));
  ASSERT_EQ(std::string("00000000ffffffff"),
            KeyGeneratorHelper::encodePadded(UINT32_MAX));
  ASSERT_EQ(std::string("000019a33af7f8cf"),
            KeyGeneratorHelper::encodePadded(28188859693263ULL));
  ASSERT_EQ(std::string("03e9782f766722ab"),
            KeyGeneratorHelper::encodePadded(281888596932633259ULL));
  ASSERT_EQ(std::string("ffffffffffffffff"),
            KeyGeneratorHelper::encodePadded(UINT64_MAX));
}

TEST(KeyGeneratorTest, decodePadded) {
  auto decode = [](std::string const& value) {
    return KeyGeneratorHelper::decodePadded(value.data(), value.size());
  };

  ASSERT_EQ(uint64_t(0), decode("0000000000000000"));
  ASSERT_EQ(uint64_t(1), decode("0000000000000001"));
  ASSERT_EQ(uint64_t(2), decode("0000000000000002"));
  ASSERT_EQ(uint64_t(5), decode("0000000000000005"));
  ASSERT_EQ(uint64_t(9), decode("0000000000000009"));
  ASSERT_EQ(uint64_t(10), decode("000000000000000a"));
  ASSERT_EQ(uint64_t(12), decode("000000000000000c"));
  ASSERT_EQ(uint64_t(15), decode("000000000000000f"));
  ASSERT_EQ(uint64_t(16), decode("0000000000000010"));
  ASSERT_EQ(uint64_t(17), decode("0000000000000011"));
  ASSERT_EQ(uint64_t(25), decode("0000000000000019"));
  ASSERT_EQ(uint64_t(32), decode("0000000000000020"));
  ASSERT_EQ(uint64_t(33), decode("0000000000000021"));
  ASSERT_EQ(uint64_t(63), decode("000000000000003f"));
  ASSERT_EQ(uint64_t(64), decode("0000000000000040"));
  ASSERT_EQ(uint64_t(127), decode("000000000000007f"));
  ASSERT_EQ(uint64_t(255), decode("00000000000000ff"));
  ASSERT_EQ(uint64_t(256), decode("0000000000000100"));
  ASSERT_EQ(uint64_t(257), decode("0000000000000101"));
  ASSERT_EQ(uint64_t(511), decode("00000000000001ff"));
  ASSERT_EQ(uint64_t(512), decode("0000000000000200"));
  ASSERT_EQ(uint64_t(4098), decode("0000000000001002"));
  ASSERT_EQ(uint64_t(65535), decode("000000000000ffff"));
  ASSERT_EQ(uint64_t(65536), decode("0000000000010000"));
  ASSERT_EQ(uint64_t(128578495), decode("0000000007a9f3bf"));
  ASSERT_EQ(uint64_t(UINT32_MAX), decode("00000000ffffffff"));
  ASSERT_EQ(uint64_t(28188859693263ULL), decode("000019a33af7f8cf"));
  ASSERT_EQ(uint64_t(281888596932633259ULL), decode("03e9782f766722ab"));
  ASSERT_EQ(uint64_t(UINT64_MAX), decode("ffffffffffffffff"));
}

TEST(KeyGeneratorTest, validateKey) {
  std::string borderline(254, 'x');
  std::string tooLong(borderline.size() + 1, 'x');
  EXPECT_TRUE(::validateKeyWrapper(borderline.data()));
  EXPECT_FALSE(::validateKeyWrapper(tooLong.data()));
  EXPECT_TRUE(::validateKeyWrapper("123"));
  EXPECT_TRUE(::validateKeyWrapper("123abc"));
  EXPECT_TRUE(::validateKeyWrapper("Abc"));
  EXPECT_TRUE(::validateKeyWrapper("abc123"));
  EXPECT_TRUE(::validateKeyWrapper("!abc123"));
  EXPECT_TRUE(::validateKeyWrapper("abc@123"));
  EXPECT_FALSE(::validateKeyWrapper("/"));
  EXPECT_TRUE(::validateKeyWrapper("*abc"));
  EXPECT_TRUE(::validateKeyWrapper("1+2=3"));
  EXPECT_TRUE(::validateKeyWrapper("_abc"));
  EXPECT_TRUE(::validateKeyWrapper(":abc"));
  EXPECT_TRUE(::validateKeyWrapper("abc;"));
  EXPECT_TRUE(::validateKeyWrapper("1,2,3"));
  EXPECT_TRUE(::validateKeyWrapper("abc."));
  EXPECT_TRUE(::validateKeyWrapper("a=bc"));
  EXPECT_FALSE(::validateKeyWrapper("<x"));
  EXPECT_FALSE(::validateKeyWrapper("x>"));
  EXPECT_TRUE(::validateKeyWrapper("!"));
  EXPECT_FALSE(::validateKeyWrapper("?"));
  EXPECT_FALSE(::validateKeyWrapper("#"));
  EXPECT_TRUE(::validateKeyWrapper("%"));
  EXPECT_TRUE(::validateKeyWrapper("ab("));
  EXPECT_TRUE(::validateKeyWrapper("')"));
  EXPECT_TRUE(::validateKeyWrapper("$abc"));
  EXPECT_FALSE(::validateKeyWrapper("&abc"));
  EXPECT_FALSE(::validateKeyWrapper("\\"));
  EXPECT_FALSE(::validateKeyWrapper("^abc123"));
  EXPECT_FALSE(::validateKeyWrapper("`abc"));
  EXPECT_FALSE(::validateKeyWrapper("[abc"));
  EXPECT_FALSE(::validateKeyWrapper("]abc"));
  EXPECT_FALSE(::validateKeyWrapper("}abc"));
  EXPECT_FALSE(::validateKeyWrapper("{abc"));
  EXPECT_FALSE(::validateKeyWrapper("|"));
  EXPECT_FALSE(::validateKeyWrapper("1~23"));
  EXPECT_TRUE(::validateKeyWrapper("123"));
  EXPECT_FALSE(::validateKeyWrapper(" "));
  EXPECT_FALSE(::validateKeyWrapper("\""));
  EXPECT_TRUE(::validateKeyWrapper("a.b.c"));
  EXPECT_FALSE(::validateKeyWrapper("maçã"));
  EXPECT_FALSE(::validateKeyWrapper(""));
  EXPECT_FALSE(::validateKeyWrapper("mötör"));
  EXPECT_FALSE(::validateKeyWrapper("😀"));
  EXPECT_FALSE(::validateKeyWrapper("ﻚﻠﺑ ﻞﻄﻴﻓ"));
}

TEST(KeyGeneratorTest, validateId) {
  EXPECT_FALSE(::validateIdWrapper("abc123!", false, 0));
  EXPECT_FALSE(::validateIdWrapper("abc_123!", false, 0));
  EXPECT_FALSE(::validateIdWrapper("", false, 0));

  // ASCII
  EXPECT_TRUE(::validateIdWrapper("abc123/!", false, 6));
  EXPECT_FALSE(::validateIdWrapper("abc123/?", false, 6));
  EXPECT_TRUE(::validateIdWrapper("abc123/123", false, 6));
  EXPECT_TRUE(::validateIdWrapper("abc123/123abc", false, 6));
  EXPECT_TRUE(::validateIdWrapper("abc123/Abc", false, 6));
  EXPECT_TRUE(::validateIdWrapper("abc123/abc123", false, 6));
  EXPECT_TRUE(::validateIdWrapper("abc123/!abc123", false, 6));
  EXPECT_TRUE(::validateIdWrapper("abc123/abc@123", false, 6));
  EXPECT_FALSE(::validateIdWrapper("abc123//", false, 6));
  EXPECT_TRUE(::validateIdWrapper("abc123/*abc", false, 6));
  EXPECT_FALSE(::validateIdWrapper("a/bc123/Abc", false, 1));
  EXPECT_TRUE(::validateIdWrapper("abc123/1+2=3", false, 6));
  EXPECT_TRUE(::validateIdWrapper("abc123/_abc", false, 6));
  EXPECT_TRUE(::validateIdWrapper("abc123/:abc", false, 6));
  EXPECT_TRUE(::validateIdWrapper("abc123/abc;", false, 6));
  EXPECT_TRUE(::validateIdWrapper("abc123/1,2,3", false, 6));
  EXPECT_TRUE(::validateIdWrapper("abc123/abc.", false, 6));
  EXPECT_TRUE(::validateIdWrapper("abc123/a=bc", false, 6));
  EXPECT_FALSE(::validateIdWrapper("abc123/<x", false, 6));
  EXPECT_FALSE(::validateIdWrapper("abc123/x>", false, 6));
  EXPECT_TRUE(::validateIdWrapper("abc123/!", false, 6));
  EXPECT_FALSE(::validateIdWrapper("abc123/?", false, 6));
  EXPECT_FALSE(::validateIdWrapper("abc123/#", false, 6));
  EXPECT_TRUE(::validateIdWrapper("abc123/%", false, 6));
  EXPECT_TRUE(::validateIdWrapper("abc123/ab(", false, 6));
  EXPECT_TRUE(::validateIdWrapper("abc123/')", false, 6));
  EXPECT_TRUE(::validateIdWrapper("abc123/$abc", false, 6));
  EXPECT_FALSE(::validateIdWrapper("abc123/&abc", false, 6));
  EXPECT_FALSE(::validateIdWrapper("abc123/\\", false, 6));
  EXPECT_FALSE(::validateIdWrapper("abc123/^abc123", false, 6));
  EXPECT_FALSE(::validateIdWrapper("abc123/`abc", false, 6));
  EXPECT_FALSE(::validateIdWrapper("abc123/[abc", false, 6));
  EXPECT_FALSE(::validateIdWrapper("abc123/]abc", false, 6));
  EXPECT_FALSE(::validateIdWrapper("abc123/}abc", false, 6));
  EXPECT_FALSE(::validateIdWrapper("abc123/{abc", false, 6));
  EXPECT_FALSE(::validateIdWrapper("abc123/|", false, 6));
  EXPECT_FALSE(::validateIdWrapper("abc123/1~23", false, 6));
  EXPECT_TRUE(::validateIdWrapper("abc123/123", false, 6));
  EXPECT_FALSE(::validateIdWrapper("abc123/ ", false, 6));
  EXPECT_FALSE(::validateIdWrapper("abc123/\"", false, 6));
  EXPECT_TRUE(::validateIdWrapper("abc123/a.b.c", false, 6));
  EXPECT_FALSE(::validateIdWrapper("abc123/maçã", false, 6));
  EXPECT_FALSE(::validateIdWrapper("abc123/", false, 6));
  EXPECT_FALSE(::validateIdWrapper("abc123/mötör", false, 6));
  EXPECT_FALSE(::validateIdWrapper("abc123/😀", false, 6));
  EXPECT_FALSE(::validateIdWrapper("abc123/ﻚﻠﺑ ﻞﻄﻴﻓ", false, 6));
  EXPECT_TRUE(::validateIdWrapper(" a + & ? = abc/!", true, 14));
  EXPECT_TRUE(::validateIdWrapper("<script>alert(1);/!", true, 17));
  EXPECT_TRUE(::validateIdWrapper("a b c/123", true, 5));
  EXPECT_TRUE(::validateIdWrapper("test123 & ' \" <> abc/123abc", true, 20));
  EXPECT_TRUE(::validateIdWrapper("abc:123/Abc", true, 7));
  EXPECT_FALSE(::validateIdWrapper("a/bc123/Abc", true, 1));
  EXPECT_FALSE(::validateIdWrapper(".abc/abc123", true, 4));
  EXPECT_FALSE(::validateIdWrapper("a\0b/!abc123", true, 3, 11));

  EXPECT_FALSE(::validateIdWrapper("maçã/!", false, 6));
  EXPECT_FALSE(::validateIdWrapper("maçã/?", false, 6));
  EXPECT_FALSE(::validateIdWrapper("maçã/123", false, 6));
  EXPECT_FALSE(::validateIdWrapper("maçã/123abc", false, 6));
  EXPECT_FALSE(::validateIdWrapper("maçã/Abc", false, 6));
  EXPECT_FALSE(::validateIdWrapper("maçã/abc123", false, 6));
  EXPECT_FALSE(::validateIdWrapper("maçã/!abc123", false, 6));
  EXPECT_FALSE(::validateIdWrapper("maçã/abc@123", false, 6));
  EXPECT_FALSE(::validateIdWrapper("maçã//", false, 6));
  EXPECT_FALSE(::validateIdWrapper("maçã/*abc", false, 6));
  EXPECT_FALSE(::validateIdWrapper("maçã/1+2=3", false, 6));
  EXPECT_FALSE(::validateIdWrapper("maçã/_abc", false, 6));
  EXPECT_FALSE(::validateIdWrapper("maçã/:abc", false, 6));
  EXPECT_FALSE(::validateIdWrapper("maçã/abc;", false, 6));
  EXPECT_FALSE(::validateIdWrapper("maçã/1,2,3", false, 6));
  EXPECT_FALSE(::validateIdWrapper("maçã/abc.", false, 6));
  EXPECT_FALSE(::validateIdWrapper("maçã/a=bc", false, 6));
  EXPECT_FALSE(::validateIdWrapper("maçã/<x", false, 6));
  EXPECT_FALSE(::validateIdWrapper("maçã/x>", false, 6));
  EXPECT_FALSE(::validateIdWrapper("maçã/!", false, 6));
  EXPECT_FALSE(::validateIdWrapper("maçã/?", false, 6));
  EXPECT_FALSE(::validateIdWrapper("maçã/#", false, 6));
  EXPECT_FALSE(::validateIdWrapper("maçã/%", false, 6));
  EXPECT_FALSE(::validateIdWrapper("maçã/ab(", false, 6));
  EXPECT_FALSE(::validateIdWrapper("maçã/')", false, 6));
  EXPECT_FALSE(::validateIdWrapper("maçã/$abc", false, 6));
  EXPECT_FALSE(::validateIdWrapper("maçã/&abc", false, 6));
  EXPECT_FALSE(::validateIdWrapper("maçã/\\", false, 6));
  EXPECT_FALSE(::validateIdWrapper("maçã/^abc123", false, 6));
  EXPECT_FALSE(::validateIdWrapper("maçã/`abc", false, 6));
  EXPECT_FALSE(::validateIdWrapper("maçã/[abc", false, 6));
  EXPECT_FALSE(::validateIdWrapper("maçã/]abc", false, 6));
  EXPECT_FALSE(::validateIdWrapper("maçã/}abc", false, 6));
  EXPECT_FALSE(::validateIdWrapper("maçã/{abc", false, 6));
  EXPECT_FALSE(::validateIdWrapper("maçã/|", false, 6));
  EXPECT_FALSE(::validateIdWrapper("maçã/1~23", false, 6));
  EXPECT_FALSE(::validateIdWrapper("maçã/123", false, 6));
  EXPECT_FALSE(::validateIdWrapper("maçã/ ", false, 6));
  EXPECT_FALSE(::validateIdWrapper("maçã/\"", false, 6));
  EXPECT_FALSE(::validateIdWrapper("maçã/a.b.c", false, 6));
  EXPECT_FALSE(::validateIdWrapper("maçã/maçã", false, 6));
  EXPECT_FALSE(::validateIdWrapper("maçã/", false, 6));
  EXPECT_FALSE(::validateIdWrapper("maçã/mötör", false, 6));
  EXPECT_FALSE(::validateIdWrapper("maçã/😀", false, 6));
  EXPECT_FALSE(::validateIdWrapper("maçã/ﻚﻠﺑ ﻞﻄﻴﻓ", false, 6));

  // Special characters
  EXPECT_TRUE(::validateIdWrapper(" a + & ? = abc/!", true, 14));
  EXPECT_TRUE(::validateIdWrapper("<script>alert(1);/!", true, 17));
  EXPECT_TRUE(::validateIdWrapper("a b c/123", true, 5));
  EXPECT_TRUE(::validateIdWrapper("test123 & ' \" <> abc/123abc", true, 20));
  EXPECT_TRUE(::validateIdWrapper("abc:123/Abc", true, 7));
  EXPECT_FALSE(::validateIdWrapper(".abc/abc123", true, 4));
  EXPECT_TRUE(::validateIdWrapper("abc123/abc@123", true, 6));
  EXPECT_FALSE(::validateIdWrapper("abc123//", true, 6));
  EXPECT_TRUE(::validateIdWrapper("abc123/*abc", true, 6));
  EXPECT_TRUE(::validateIdWrapper("abc123/1+2=3", true, 6));
  EXPECT_TRUE(::validateIdWrapper("abc123/_abc", true, 6));
  EXPECT_TRUE(::validateIdWrapper("abc123/:abc", true, 6));
  EXPECT_TRUE(::validateIdWrapper("abc123/abc;", true, 6));
  EXPECT_TRUE(::validateIdWrapper("abc123/1,2,3", true, 6));
  EXPECT_TRUE(::validateIdWrapper("abc123/abc.", true, 6));
  EXPECT_TRUE(::validateIdWrapper("abc123/a=bc", true, 6));
  EXPECT_FALSE(::validateIdWrapper("abc123/<x", true, 6));
  EXPECT_FALSE(::validateIdWrapper("abc123/x>", true, 6));
  EXPECT_TRUE(::validateIdWrapper("abc123/!", true, 6));
  EXPECT_FALSE(::validateIdWrapper("abc123/?", true, 6));
  EXPECT_FALSE(::validateIdWrapper("abc123/#", true, 6));
  EXPECT_TRUE(::validateIdWrapper("abc123/%", true, 6));
  EXPECT_TRUE(::validateIdWrapper("abc123/ab(", true, 6));
  EXPECT_TRUE(::validateIdWrapper("abc123/')", true, 6));
  EXPECT_TRUE(::validateIdWrapper("abc123/$abc", true, 6));
  EXPECT_FALSE(::validateIdWrapper("abc123/&abc", true, 6));
  EXPECT_FALSE(::validateIdWrapper("abc123/\\", true, 6));
  EXPECT_FALSE(::validateIdWrapper("abc123/^abc123", true, 6));
  EXPECT_FALSE(::validateIdWrapper("abc123/`abc", true, 6));
  EXPECT_FALSE(::validateIdWrapper("abc123/[abc", true, 6));
  EXPECT_FALSE(::validateIdWrapper("abc123/]abc", true, 6));
  EXPECT_FALSE(::validateIdWrapper("abc123/}abc", true, 6));
  EXPECT_FALSE(::validateIdWrapper("abc123/{abc", true, 6));
  EXPECT_FALSE(::validateIdWrapper("abc123/|", true, 6));
  EXPECT_FALSE(::validateIdWrapper("abc123/1~23", true, 6));
  EXPECT_TRUE(::validateIdWrapper("abc123/123", true, 6));
  EXPECT_FALSE(::validateIdWrapper("abc123/ ", true, 6));
  EXPECT_FALSE(::validateIdWrapper("abc123/\"", true, 6));
  EXPECT_TRUE(::validateIdWrapper("abc123/a.b.c", true, 6));
  EXPECT_FALSE(::validateIdWrapper("abc123/maçã", true, 6));
  EXPECT_FALSE(::validateIdWrapper("abc123/", true, 6));
  EXPECT_FALSE(::validateIdWrapper("abc123/mötör", true, 6));
  EXPECT_FALSE(::validateIdWrapper("abc123/😀", true, 6));
  EXPECT_FALSE(::validateIdWrapper("abc123/ﻚﻠﺑ ﻞﻄﻴﻓ", true, 6));

  // unicode

  EXPECT_TRUE(::validateIdWrapper("maçã/abc123", true, 6));
  EXPECT_TRUE(::validateIdWrapper("mötör/abc123", true, 7));
  EXPECT_TRUE(::validateIdWrapper("😀/abc123", true, 4));
  EXPECT_TRUE(::validateIdWrapper("ﻚﻠﺑ ﻞﻄﻴﻓ/abc123", true, 22));
  EXPECT_TRUE(::validateIdWrapper("かわいい犬/abc123", true, 15));
}
