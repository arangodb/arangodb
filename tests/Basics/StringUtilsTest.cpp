////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for StringUtils class
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
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

////////////////////////////////////////////////////////////////////////////////
/// @brief test_Split1
////////////////////////////////////////////////////////////////////////////////

TEST_F(StringUtilsTest, test_Split1) {
  vector<string> lines = StringUtils::split("Hallo\nWorld\\/Me", '\n');

  EXPECT_TRUE(lines.size() ==  (size_t) 2);

  if (lines.size() == 2) {
    EXPECT_TRUE(lines[0] ==  "Hallo");
    EXPECT_TRUE(lines[1] ==  "World/Me");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test_Split2
////////////////////////////////////////////////////////////////////////////////

TEST_F(StringUtilsTest, test_Split2) {
  vector<string> lines = StringUtils::split("\nHallo\nWorld\n", '\n');

  EXPECT_TRUE(lines.size() ==  (size_t) 4);

  if (lines.size() == 4) {
    EXPECT_TRUE(lines[0] ==  "");
    EXPECT_TRUE(lines[1] ==  "Hallo");
    EXPECT_TRUE(lines[2] ==  "World");
    EXPECT_TRUE(lines[3] ==  "");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test_Split3
////////////////////////////////////////////////////////////////////////////////

TEST_F(StringUtilsTest, test_Split3) {
  vector<string> lines = StringUtils::split("Hallo\nWorld\\/Me", '\n', '\0');

  EXPECT_TRUE(lines.size() ==  (size_t) 2);

  if (lines.size() == 2) {
    EXPECT_TRUE(lines[0] ==  "Hallo");
    EXPECT_TRUE(lines[1] ==  "World\\/Me");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test_Tolower
////////////////////////////////////////////////////////////////////////////////

TEST_F(StringUtilsTest, test_Tolower) {
  string lower = StringUtils::tolower("HaLlO WoRlD!");

  EXPECT_TRUE(lower ==  "hallo world!");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test_uint64
////////////////////////////////////////////////////////////////////////////////

TEST_F(StringUtilsTest, test_uint64) {
  EXPECT_TRUE(0ULL ==  StringUtils::uint64("abc"));
  EXPECT_TRUE(0ULL ==  StringUtils::uint64("ABC"));
  EXPECT_TRUE(0ULL ==  StringUtils::uint64(" foo"));
  EXPECT_TRUE(0ULL ==  StringUtils::uint64(""));
  EXPECT_TRUE(0ULL ==  StringUtils::uint64(" "));
  EXPECT_TRUE(12ULL ==  StringUtils::uint64("012"));
  EXPECT_TRUE(12ULL ==  StringUtils::uint64("00012"));
  EXPECT_TRUE(1234ULL ==  StringUtils::uint64("1234"));
  EXPECT_TRUE(1234ULL ==  StringUtils::uint64("1234a"));
#ifdef TRI_STRING_UTILS_USE_FROM_CHARS
  EXPECT_TRUE(0ULL ==  StringUtils::uint64("-1"));
  EXPECT_TRUE(0ULL ==  StringUtils::uint64("-12345"));
#else
  EXPECT_TRUE(18446744073709551615ULL ==  StringUtils::uint64("-1"));
  EXPECT_TRUE(18446744073709539271ULL ==  StringUtils::uint64("-12345"));
#endif
  EXPECT_TRUE(1234ULL ==  StringUtils::uint64("1234.56"));
  EXPECT_TRUE(0ULL ==  StringUtils::uint64("1234567890123456789012345678901234567890"));
  EXPECT_TRUE(0ULL ==  StringUtils::uint64("@"));

  EXPECT_TRUE(0ULL ==  StringUtils::uint64("0"));
  EXPECT_TRUE(1ULL ==  StringUtils::uint64("1"));
  EXPECT_TRUE(12ULL ==  StringUtils::uint64("12"));
  EXPECT_TRUE(123ULL ==  StringUtils::uint64("123"));
  EXPECT_TRUE(1234ULL ==  StringUtils::uint64("1234"));
  EXPECT_TRUE(1234ULL ==  StringUtils::uint64("01234"));
  EXPECT_TRUE(9ULL ==  StringUtils::uint64("9"));
  EXPECT_TRUE(9ULL ==  StringUtils::uint64("09"));
  EXPECT_TRUE(9ULL ==  StringUtils::uint64("0009"));
  EXPECT_TRUE(12345678ULL ==  StringUtils::uint64("12345678"));
  EXPECT_TRUE(1234567800ULL ==  StringUtils::uint64("1234567800"));
  EXPECT_TRUE(1234567890123456ULL ==  StringUtils::uint64("1234567890123456"));
  EXPECT_TRUE(UINT64_MAX ==  StringUtils::uint64(std::to_string(UINT64_MAX)));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test_uint64_trusted
////////////////////////////////////////////////////////////////////////////////

TEST_F(StringUtilsTest, test_uint64_trusted) {
  EXPECT_TRUE(0ULL ==  StringUtils::uint64_trusted("0"));
  EXPECT_TRUE(1ULL ==  StringUtils::uint64_trusted("1"));
  EXPECT_TRUE(12ULL ==  StringUtils::uint64_trusted("12"));
  EXPECT_TRUE(123ULL ==  StringUtils::uint64_trusted("123"));
  EXPECT_TRUE(1234ULL ==  StringUtils::uint64_trusted("1234"));
  EXPECT_TRUE(1234ULL ==  StringUtils::uint64_trusted("01234"));
  EXPECT_TRUE(9ULL ==  StringUtils::uint64_trusted("9"));
  EXPECT_TRUE(9ULL ==  StringUtils::uint64_trusted("0009"));
  EXPECT_TRUE(12345678ULL ==  StringUtils::uint64_trusted("12345678"));
  EXPECT_TRUE(1234567800ULL ==  StringUtils::uint64_trusted("1234567800"));
  EXPECT_TRUE(1234567890123456ULL ==  StringUtils::uint64_trusted("1234567890123456"));
  EXPECT_TRUE(UINT64_MAX ==  StringUtils::uint64_trusted(std::to_string(UINT64_MAX)));
}

TEST_F(StringUtilsTest, test_encodeHex) {
  EXPECT_TRUE("" ==  StringUtils::encodeHex(""));

  EXPECT_TRUE("00" ==  StringUtils::encodeHex(std::string("\x00", 1)));
  EXPECT_TRUE("01" ==  StringUtils::encodeHex("\x01"));
  EXPECT_TRUE("02" ==  StringUtils::encodeHex("\x02"));
  EXPECT_TRUE("03" ==  StringUtils::encodeHex("\x03"));
  EXPECT_TRUE("04" ==  StringUtils::encodeHex("\x04"));
  EXPECT_TRUE("05" ==  StringUtils::encodeHex("\x05"));
  EXPECT_TRUE("06" ==  StringUtils::encodeHex("\x06"));
  EXPECT_TRUE("07" ==  StringUtils::encodeHex("\x07"));
  EXPECT_TRUE("08" ==  StringUtils::encodeHex("\x08"));
  EXPECT_TRUE("09" ==  StringUtils::encodeHex("\x09"));
  EXPECT_TRUE("0a" ==  StringUtils::encodeHex("\x0a"));
  EXPECT_TRUE("0b" ==  StringUtils::encodeHex("\x0b"));
  EXPECT_TRUE("0c" ==  StringUtils::encodeHex("\x0c"));
  EXPECT_TRUE("0d" ==  StringUtils::encodeHex("\x0d"));
  EXPECT_TRUE("0e" ==  StringUtils::encodeHex("\x0e"));
  EXPECT_TRUE("0f" ==  StringUtils::encodeHex("\x0f"));

  EXPECT_TRUE("10" ==  StringUtils::encodeHex("\x10"));
  EXPECT_TRUE("42" ==  StringUtils::encodeHex("\x42"));
  EXPECT_TRUE("ff" ==  StringUtils::encodeHex("\xff"));
  EXPECT_TRUE("aa0009" ==  StringUtils::encodeHex(std::string("\xaa\x00\x09", 3)));
  EXPECT_TRUE("000102" ==  StringUtils::encodeHex(std::string("\x00\x01\x02", 3)));
  EXPECT_TRUE("00010203" ==  StringUtils::encodeHex(std::string("\x00\x01\x02\03", 4)));
  EXPECT_TRUE("20" ==  StringUtils::encodeHex(" "));
  EXPECT_TRUE("2a2a" ==  StringUtils::encodeHex("**"));
  EXPECT_TRUE("616263646566" ==  StringUtils::encodeHex("abcdef"));
  EXPECT_TRUE("4142434445462047" ==  StringUtils::encodeHex("ABCDEF G"));
  EXPECT_TRUE("54686520517569636b2062726f776e20466f78206a756d706564206f76657220746865206c617a7920646f6721" ==  StringUtils::encodeHex("The Quick brown Fox jumped over the lazy dog!"));
  EXPECT_TRUE("446572204bc3b674c3b67220737072c3bc6e6720c3bc62657220646965204272c3bc636b65" ==  StringUtils::encodeHex("Der Kötör sprüng über die Brücke"));
  EXPECT_TRUE("c3a4c3b6c3bcc39fc384c396c39ce282acc2b5" == StringUtils::encodeHex("äöüßÄÖÜ€µ"));
}

TEST_F(StringUtilsTest, test_decodeHex) {
  EXPECT_TRUE("" ==  StringUtils::decodeHex(""));

  EXPECT_TRUE(std::string("\x00", 1) ==  StringUtils::decodeHex("00"));
  EXPECT_TRUE("\x01" ==  StringUtils::decodeHex("01"));
  EXPECT_TRUE("\x02" ==  StringUtils::decodeHex("02"));
  EXPECT_TRUE("\x03" ==  StringUtils::decodeHex("03"));
  EXPECT_TRUE("\x04" ==  StringUtils::decodeHex("04"));
  EXPECT_TRUE("\x05" ==  StringUtils::decodeHex("05"));
  EXPECT_TRUE("\x06" ==  StringUtils::decodeHex("06"));
  EXPECT_TRUE("\x07" ==  StringUtils::decodeHex("07"));
  EXPECT_TRUE("\x08" ==  StringUtils::decodeHex("08"));
  EXPECT_TRUE("\x09" ==  StringUtils::decodeHex("09"));
  EXPECT_TRUE("\x0a" ==  StringUtils::decodeHex("0a"));
  EXPECT_TRUE("\x0b" ==  StringUtils::decodeHex("0b"));
  EXPECT_TRUE("\x0c" ==  StringUtils::decodeHex("0c"));
  EXPECT_TRUE("\x0d" ==  StringUtils::decodeHex("0d"));
  EXPECT_TRUE("\x0e" ==  StringUtils::decodeHex("0e"));
  EXPECT_TRUE("\x0f" ==  StringUtils::decodeHex("0f"));
  EXPECT_TRUE("\x0a" ==  StringUtils::decodeHex("0A"));
  EXPECT_TRUE("\x0b" ==  StringUtils::decodeHex("0B"));
  EXPECT_TRUE("\x0c" ==  StringUtils::decodeHex("0C"));
  EXPECT_TRUE("\x0d" ==  StringUtils::decodeHex("0D"));
  EXPECT_TRUE("\x0e" ==  StringUtils::decodeHex("0E"));
  EXPECT_TRUE("\x0f" ==  StringUtils::decodeHex("0F"));
  
  EXPECT_TRUE("\x1a" ==  StringUtils::decodeHex("1a"));
  EXPECT_TRUE("\x2b" ==  StringUtils::decodeHex("2b"));
  EXPECT_TRUE("\x3c" ==  StringUtils::decodeHex("3c"));
  EXPECT_TRUE("\x4d" ==  StringUtils::decodeHex("4d"));
  EXPECT_TRUE("\x5e" ==  StringUtils::decodeHex("5e"));
  EXPECT_TRUE("\x6f" ==  StringUtils::decodeHex("6f"));
  EXPECT_TRUE("\x7a" ==  StringUtils::decodeHex("7A"));
  EXPECT_TRUE("\x8b" ==  StringUtils::decodeHex("8B"));
  EXPECT_TRUE("\x9c" ==  StringUtils::decodeHex("9C"));
  EXPECT_TRUE("\xad" ==  StringUtils::decodeHex("AD"));
  EXPECT_TRUE("\xbe" ==  StringUtils::decodeHex("BE"));
  EXPECT_TRUE("\xcf" ==  StringUtils::decodeHex("CF"));
  EXPECT_TRUE("\xdf" ==  StringUtils::decodeHex("df"));
  EXPECT_TRUE("\xef" ==  StringUtils::decodeHex("eF"));
  EXPECT_TRUE("\xff" ==  StringUtils::decodeHex("ff"));
  
  EXPECT_TRUE(" " ==  StringUtils::decodeHex("20"));
  EXPECT_TRUE("**" ==  StringUtils::decodeHex("2a2a"));
  EXPECT_TRUE("abcdef" ==  StringUtils::decodeHex("616263646566"));
  EXPECT_TRUE("ABCDEF G" == StringUtils::decodeHex("4142434445462047"));

  EXPECT_TRUE("The Quick brown Fox jumped over the lazy dog!" == StringUtils::decodeHex("54686520517569636b2062726f776e20466f78206a756d706564206f76657220746865206c617a7920646f6721"));
  EXPECT_TRUE("Der Kötör sprüng über die Brücke" == StringUtils::decodeHex("446572204bc3b674c3b67220737072c3bc6e6720c3bc62657220646965204272c3bc636b65"));
  EXPECT_TRUE("äöüßÄÖÜ€µ" == StringUtils::decodeHex("c3a4c3b6c3bcc39fc384c396c39ce282acc2b5"));
  
  EXPECT_TRUE("" ==  StringUtils::decodeHex("1"));
  EXPECT_TRUE("" ==  StringUtils::decodeHex(" "));
  EXPECT_TRUE("" ==  StringUtils::decodeHex(" 2"));
  EXPECT_TRUE("" ==  StringUtils::decodeHex("1 "));
  EXPECT_TRUE("" ==  StringUtils::decodeHex("12 "));
  EXPECT_TRUE("" ==  StringUtils::decodeHex("x"));
  EXPECT_TRUE("" ==  StringUtils::decodeHex("X"));
  EXPECT_TRUE("" ==  StringUtils::decodeHex("@@@"));
  EXPECT_TRUE("" ==  StringUtils::decodeHex("111"));
  EXPECT_TRUE("" ==  StringUtils::decodeHex("1 2 3"));
  EXPECT_TRUE("" ==  StringUtils::decodeHex("1122334"));
  EXPECT_TRUE("" ==  StringUtils::decodeHex("112233 "));
  EXPECT_TRUE("" ==  StringUtils::decodeHex(" 112233"));
  EXPECT_TRUE("" ==  StringUtils::decodeHex("abcdefgh"));
}
