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
/// @author Dr. Frank Celler
/// @author Copyright 2007-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"

#include "gtest/gtest.h"

#include <iomanip>
#include <sstream>

#include "Basics/directories.h"
#include "Basics/StringUtils.h"
#include "Basics/Utf8Helper.h"
#include "Basics/files.h"

#include "icu-helper.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace std;

using namespace std::literals::string_literals;

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

class StringUtilsTest : public ::testing::Test {
 protected:
  StringUtilsTest () {
    IcuInitializer::setup("./3rdParty/V8/v8/third_party/icu/common/icudtl.dat");
  }
};


// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(StringUtilsTest, test_SplitEmptyness) {
  EXPECT_EQ(StringUtils::split("", '\0'), (std::vector<std::string>{}));
  EXPECT_EQ(StringUtils::split({"a\0b\0c", 5}, '\0'), (std::vector<std::string>{"a", "b", "c"}));

  EXPECT_EQ(StringUtils::split("", '/'), (std::vector<std::string>{}));
  EXPECT_EQ(StringUtils::split("/", '/'), (std::vector<std::string>{"", ""}));
  EXPECT_EQ(StringUtils::split("/1", '/'), (std::vector<std::string>{"", "1"}));
  EXPECT_EQ(StringUtils::split("1/", '/'), (std::vector<std::string>{"1", ""}));
  EXPECT_EQ(StringUtils::split("//", '/'), (std::vector<std::string>{"", "", ""}));
  EXPECT_EQ(StringUtils::split("knurps", '/'), (std::vector<std::string>{"knurps"}));
  
  
  EXPECT_EQ(StringUtils::split("", "/"), (std::vector<std::string>{}));
  EXPECT_EQ(StringUtils::split("/", "/"), (std::vector<std::string>{"", ""}));
  EXPECT_EQ(StringUtils::split("/1", "/"), (std::vector<std::string>{"", "1"}));
  EXPECT_EQ(StringUtils::split("1/", "/"), (std::vector<std::string>{"1", ""}));
  EXPECT_EQ(StringUtils::split("//", "/"), (std::vector<std::string>{"", "", ""}));
  EXPECT_EQ(StringUtils::split("knurps", "/"), (std::vector<std::string>{"knurps"}));
  
  EXPECT_EQ(StringUtils::split("", "abc"), (std::vector<std::string>{}));
  EXPECT_EQ(StringUtils::split("/", "abc"), (std::vector<std::string>{"/"}));
  EXPECT_EQ(StringUtils::split("/1", "abc"), (std::vector<std::string>{"/1"}));
  EXPECT_EQ(StringUtils::split("1/", "abc"), (std::vector<std::string>{"1/"}));
  EXPECT_EQ(StringUtils::split("//", "abc"), (std::vector<std::string>{"//"}));
  
  EXPECT_EQ(StringUtils::split("abcdefg", "abc"), (std::vector<std::string>{"", "", "", "defg"}));
  EXPECT_EQ(StringUtils::split("foo-split-bar-split-baz", "-sp"), (std::vector<std::string>{"foo", "", "", "lit", "bar", "", "", "lit", "baz"}));

  EXPECT_EQ(StringUtils::split("this-line.is,split", ".,-"), (std::vector<std::string>{"this", "line", "is", "split"}));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test_Split1
////////////////////////////////////////////////////////////////////////////////

TEST_F(StringUtilsTest, test_Split1) {
  vector<string> lines = StringUtils::split("Hallo\nWorld\\/Me", '\n');

  EXPECT_EQ(lines.size(), 2U);
  EXPECT_EQ(lines[0],  "Hallo");
  EXPECT_EQ(lines[1],  "World\\/Me");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test_Split2
////////////////////////////////////////////////////////////////////////////////

TEST_F(StringUtilsTest, test_Split2) {
  vector<string> lines = StringUtils::split("\nHallo\nWorld\n", '\n');

  EXPECT_EQ(lines.size(), 4U);

  EXPECT_EQ(lines[0],  "");
  EXPECT_EQ(lines[1],  "Hallo");
  EXPECT_EQ(lines[2],  "World");
  EXPECT_EQ(lines[3],  "");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test_Split3
////////////////////////////////////////////////////////////////////////////////

TEST_F(StringUtilsTest, test_Split3) {
  vector<string> lines = StringUtils::split("Hallo\nWorld\\/Me", '\n');

  EXPECT_EQ(lines.size(), 2U);
  
  EXPECT_EQ(lines[0],  "Hallo");
  EXPECT_EQ(lines[1],  "World\\/Me");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test_tolower
////////////////////////////////////////////////////////////////////////////////

TEST_F(StringUtilsTest, test_tolower) {
  EXPECT_EQ(StringUtils::tolower(""), "");
  EXPECT_EQ(StringUtils::tolower(" "), " ");
  EXPECT_EQ(StringUtils::tolower("12345"), "12345");
  EXPECT_EQ(StringUtils::tolower("a"), "a");
  EXPECT_EQ(StringUtils::tolower("A"), "a");
  EXPECT_EQ(StringUtils::tolower("ä"), "ä");
  EXPECT_EQ(StringUtils::tolower("Ä"), "Ä");
  EXPECT_EQ(StringUtils::tolower("HeLlO WoRlD!"), "hello world!");
  EXPECT_EQ(StringUtils::tolower("hello-world-nono "), "hello-world-nono ");
  EXPECT_EQ(StringUtils::tolower("HELLo-world-NONO "), "hello-world-nono ");
  EXPECT_EQ(StringUtils::tolower(" The quick \r\nbrown Fox"), " the quick \r\nbrown fox");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test_toupper
////////////////////////////////////////////////////////////////////////////////

TEST_F(StringUtilsTest, test_toupper) {
  EXPECT_EQ(StringUtils::toupper(""), "");
  EXPECT_EQ(StringUtils::toupper(" "), " ");
  EXPECT_EQ(StringUtils::toupper("12345"), "12345");
  EXPECT_EQ(StringUtils::toupper("a"), "A");
  EXPECT_EQ(StringUtils::toupper("A"), "A");
  EXPECT_EQ(StringUtils::toupper("ä"), "ä");
  EXPECT_EQ(StringUtils::toupper("Ä"), "Ä");
  EXPECT_EQ(StringUtils::toupper("HeLlO WoRlD!"), "HELLO WORLD!");
  EXPECT_EQ(StringUtils::toupper("hello-world-nono "), "HELLO-WORLD-NONO ");
  EXPECT_EQ(StringUtils::toupper("HELLo-world-NONO "), "HELLO-WORLD-NONO ");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test_uint64
////////////////////////////////////////////////////////////////////////////////

TEST_F(StringUtilsTest, test_uint64) {
  EXPECT_EQ(0ULL,  StringUtils::uint64("abc"s));
  EXPECT_EQ(0ULL,  StringUtils::uint64("ABC"s));
  EXPECT_EQ(0ULL,  StringUtils::uint64(" foo"s));
  EXPECT_EQ(0ULL,  StringUtils::uint64(""s));
  EXPECT_EQ(0ULL,  StringUtils::uint64(" "s));
  EXPECT_EQ(12ULL,  StringUtils::uint64("012"s));
  EXPECT_EQ(12ULL,  StringUtils::uint64("00012"s));
  EXPECT_EQ(1234ULL,  StringUtils::uint64("1234"s));
  EXPECT_EQ(1234ULL,  StringUtils::uint64("1234a"s));
  EXPECT_EQ(0ULL,  StringUtils::uint64("-1"s));
  EXPECT_EQ(0ULL,  StringUtils::uint64("-12345"s));
  EXPECT_EQ(1234ULL,  StringUtils::uint64("1234.56"s));
  EXPECT_EQ(0ULL,  StringUtils::uint64("1234567890123456789012345678901234567890"s));
  EXPECT_EQ(0ULL,  StringUtils::uint64("@"s));

  EXPECT_EQ(0ULL,  StringUtils::uint64("0"s));
  EXPECT_EQ(1ULL,  StringUtils::uint64("1"s));
  EXPECT_EQ(12ULL,  StringUtils::uint64("12"s));
  EXPECT_EQ(123ULL,  StringUtils::uint64("123"s));
  EXPECT_EQ(1234ULL,  StringUtils::uint64("1234"s));
  EXPECT_EQ(1234ULL,  StringUtils::uint64("01234"s));
  EXPECT_EQ(9ULL,  StringUtils::uint64("9"s));
  EXPECT_EQ(9ULL,  StringUtils::uint64("09"s));
  EXPECT_EQ(9ULL,  StringUtils::uint64("0009"s));
  EXPECT_EQ(12345678ULL,  StringUtils::uint64("12345678"s));
  EXPECT_EQ(1234567800ULL,  StringUtils::uint64("1234567800"s));
  EXPECT_EQ(1234567890123456ULL,  StringUtils::uint64("1234567890123456"s));
  EXPECT_EQ(UINT64_MAX,  StringUtils::uint64(std::to_string(UINT64_MAX)));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test_uint64_trusted
////////////////////////////////////////////////////////////////////////////////

TEST_F(StringUtilsTest, test_uint64_trusted) {
  EXPECT_EQ(0ULL,  StringUtils::uint64_trusted("0"));
  EXPECT_EQ(1ULL,  StringUtils::uint64_trusted("1"));
  EXPECT_EQ(12ULL,  StringUtils::uint64_trusted("12"));
  EXPECT_EQ(123ULL,  StringUtils::uint64_trusted("123"));
  EXPECT_EQ(1234ULL,  StringUtils::uint64_trusted("1234"));
  EXPECT_EQ(1234ULL,  StringUtils::uint64_trusted("01234"));
  EXPECT_EQ(9ULL,  StringUtils::uint64_trusted("9"));
  EXPECT_EQ(9ULL,  StringUtils::uint64_trusted("0009"));
  EXPECT_EQ(12345678ULL,  StringUtils::uint64_trusted("12345678"));
  EXPECT_EQ(1234567800ULL,  StringUtils::uint64_trusted("1234567800"));
  EXPECT_EQ(1234567890123456ULL,  StringUtils::uint64_trusted("1234567890123456"));
  EXPECT_EQ(UINT64_MAX,  StringUtils::uint64_trusted(std::to_string(UINT64_MAX)));
}

TEST_F(StringUtilsTest, test_encodeHex) {
  EXPECT_EQ("",  StringUtils::encodeHex(""));

  EXPECT_EQ("00",  StringUtils::encodeHex(std::string("\x00", 1)));
  EXPECT_EQ("01",  StringUtils::encodeHex("\x01"));
  EXPECT_EQ("02",  StringUtils::encodeHex("\x02"));
  EXPECT_EQ("03",  StringUtils::encodeHex("\x03"));
  EXPECT_EQ("04",  StringUtils::encodeHex("\x04"));
  EXPECT_EQ("05",  StringUtils::encodeHex("\x05"));
  EXPECT_EQ("06",  StringUtils::encodeHex("\x06"));
  EXPECT_EQ("07",  StringUtils::encodeHex("\x07"));
  EXPECT_EQ("08",  StringUtils::encodeHex("\x08"));
  EXPECT_EQ("09",  StringUtils::encodeHex("\x09"));
  EXPECT_EQ("0a",  StringUtils::encodeHex("\x0a"));
  EXPECT_EQ("0b",  StringUtils::encodeHex("\x0b"));
  EXPECT_EQ("0c",  StringUtils::encodeHex("\x0c"));
  EXPECT_EQ("0d",  StringUtils::encodeHex("\x0d"));
  EXPECT_EQ("0e",  StringUtils::encodeHex("\x0e"));
  EXPECT_EQ("0f",  StringUtils::encodeHex("\x0f"));

  EXPECT_EQ("10",  StringUtils::encodeHex("\x10"));
  EXPECT_EQ("42",  StringUtils::encodeHex("\x42"));
  EXPECT_EQ("ff",  StringUtils::encodeHex("\xff"));
  EXPECT_EQ("aa0009",  StringUtils::encodeHex(std::string("\xaa\x00\x09", 3)));
  EXPECT_EQ("000102",  StringUtils::encodeHex(std::string("\x00\x01\x02", 3)));
  EXPECT_EQ("00010203",  StringUtils::encodeHex(std::string("\x00\x01\x02\03", 4)));
  EXPECT_EQ("20",  StringUtils::encodeHex(" "));
  EXPECT_EQ("2a2a",  StringUtils::encodeHex("**"));
  EXPECT_EQ("616263646566",  StringUtils::encodeHex("abcdef"));
  EXPECT_EQ("4142434445462047",  StringUtils::encodeHex("ABCDEF G"));
  EXPECT_EQ("54686520517569636b2062726f776e20466f78206a756d706564206f76657220746865206c617a7920646f6721",  StringUtils::encodeHex("The Quick brown Fox jumped over the lazy dog!"));
  EXPECT_EQ("446572204bc3b674c3b67220737072c3bc6e6720c3bc62657220646965204272c3bc636b65",  StringUtils::encodeHex("Der Kötör sprüng über die Brücke"));
  EXPECT_EQ("c3a4c3b6c3bcc39fc384c396c39ce282acc2b5", StringUtils::encodeHex("äöüßÄÖÜ€µ"));
}

TEST_F(StringUtilsTest, test_decodeHex) {
  EXPECT_EQ("",  StringUtils::decodeHex(""));

  EXPECT_EQ(std::string("\x00", 1),  StringUtils::decodeHex("00"));
  EXPECT_EQ("\x01",  StringUtils::decodeHex("01"));
  EXPECT_EQ("\x02",  StringUtils::decodeHex("02"));
  EXPECT_EQ("\x03",  StringUtils::decodeHex("03"));
  EXPECT_EQ("\x04",  StringUtils::decodeHex("04"));
  EXPECT_EQ("\x05",  StringUtils::decodeHex("05"));
  EXPECT_EQ("\x06",  StringUtils::decodeHex("06"));
  EXPECT_EQ("\x07",  StringUtils::decodeHex("07"));
  EXPECT_EQ("\x08",  StringUtils::decodeHex("08"));
  EXPECT_EQ("\x09",  StringUtils::decodeHex("09"));
  EXPECT_EQ("\x0a",  StringUtils::decodeHex("0a"));
  EXPECT_EQ("\x0b",  StringUtils::decodeHex("0b"));
  EXPECT_EQ("\x0c",  StringUtils::decodeHex("0c"));
  EXPECT_EQ("\x0d",  StringUtils::decodeHex("0d"));
  EXPECT_EQ("\x0e",  StringUtils::decodeHex("0e"));
  EXPECT_EQ("\x0f",  StringUtils::decodeHex("0f"));
  EXPECT_EQ("\x0a",  StringUtils::decodeHex("0A"));
  EXPECT_EQ("\x0b",  StringUtils::decodeHex("0B"));
  EXPECT_EQ("\x0c",  StringUtils::decodeHex("0C"));
  EXPECT_EQ("\x0d",  StringUtils::decodeHex("0D"));
  EXPECT_EQ("\x0e",  StringUtils::decodeHex("0E"));
  EXPECT_EQ("\x0f",  StringUtils::decodeHex("0F"));
  
  EXPECT_EQ("\x1a",  StringUtils::decodeHex("1a"));
  EXPECT_EQ("\x2b",  StringUtils::decodeHex("2b"));
  EXPECT_EQ("\x3c",  StringUtils::decodeHex("3c"));
  EXPECT_EQ("\x4d",  StringUtils::decodeHex("4d"));
  EXPECT_EQ("\x5e",  StringUtils::decodeHex("5e"));
  EXPECT_EQ("\x6f",  StringUtils::decodeHex("6f"));
  EXPECT_EQ("\x7a",  StringUtils::decodeHex("7A"));
  EXPECT_EQ("\x8b",  StringUtils::decodeHex("8B"));
  EXPECT_EQ("\x9c",  StringUtils::decodeHex("9C"));
  EXPECT_EQ("\xad",  StringUtils::decodeHex("AD"));
  EXPECT_EQ("\xbe",  StringUtils::decodeHex("BE"));
  EXPECT_EQ("\xcf",  StringUtils::decodeHex("CF"));
  EXPECT_EQ("\xdf",  StringUtils::decodeHex("df"));
  EXPECT_EQ("\xef",  StringUtils::decodeHex("eF"));
  EXPECT_EQ("\xff",  StringUtils::decodeHex("ff"));
  
  EXPECT_EQ(" ",  StringUtils::decodeHex("20"));
  EXPECT_EQ("**",  StringUtils::decodeHex("2a2a"));
  EXPECT_EQ("abcdef",  StringUtils::decodeHex("616263646566"));
  EXPECT_EQ("ABCDEF G", StringUtils::decodeHex("4142434445462047"));

  EXPECT_EQ("The Quick brown Fox jumped over the lazy dog!", StringUtils::decodeHex("54686520517569636b2062726f776e20466f78206a756d706564206f76657220746865206c617a7920646f6721"));
  EXPECT_EQ("Der Kötör sprüng über die Brücke", StringUtils::decodeHex("446572204bc3b674c3b67220737072c3bc6e6720c3bc62657220646965204272c3bc636b65"));
  EXPECT_EQ("äöüßÄÖÜ€µ", StringUtils::decodeHex("c3a4c3b6c3bcc39fc384c396c39ce282acc2b5"));
  
  EXPECT_EQ("",  StringUtils::decodeHex("1"));
  EXPECT_EQ("",  StringUtils::decodeHex(" "));
  EXPECT_EQ("",  StringUtils::decodeHex(" 2"));
  EXPECT_EQ("",  StringUtils::decodeHex("1 "));
  EXPECT_EQ("",  StringUtils::decodeHex("12 "));
  EXPECT_EQ("",  StringUtils::decodeHex("x"));
  EXPECT_EQ("",  StringUtils::decodeHex("X"));
  EXPECT_EQ("",  StringUtils::decodeHex("@@@"));
  EXPECT_EQ("",  StringUtils::decodeHex("111"));
  EXPECT_EQ("",  StringUtils::decodeHex("1 2 3"));
  EXPECT_EQ("",  StringUtils::decodeHex("1122334"));
  EXPECT_EQ("",  StringUtils::decodeHex("112233 "));
  EXPECT_EQ("",  StringUtils::decodeHex(" 112233"));
  EXPECT_EQ("",  StringUtils::decodeHex("abcdefgh"));
}
