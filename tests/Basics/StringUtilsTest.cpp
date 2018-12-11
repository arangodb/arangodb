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

#include "catch.hpp"

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

struct StringUtilsSetup {
  StringUtilsSetup () {
    IcuInitializer::setup("./3rdParty/V8/v8/third_party/icu/common/icudtl.dat");
  }

  ~StringUtilsSetup () {
  }
};


// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("StringUtilsTest", "[string]") {
  StringUtilsSetup s;

////////////////////////////////////////////////////////////////////////////////
/// @brief test_Split1
////////////////////////////////////////////////////////////////////////////////

SECTION("test_Split1") {
  vector<string> lines = StringUtils::split("Hallo\nWorld\\/Me", '\n');

  CHECK(lines.size() ==  (size_t) 2);

  if (lines.size() == 2) {
    CHECK(lines[0] ==  "Hallo");
    CHECK(lines[1] ==  "World/Me");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test_Split2
////////////////////////////////////////////////////////////////////////////////

SECTION("test_Split2") {
  vector<string> lines = StringUtils::split("\nHallo\nWorld\n", '\n');

  CHECK(lines.size() ==  (size_t) 4);

  if (lines.size() == 4) {
    CHECK(lines[0] ==  "");
    CHECK(lines[1] ==  "Hallo");
    CHECK(lines[2] ==  "World");
    CHECK(lines[3] ==  "");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test_Split3
////////////////////////////////////////////////////////////////////////////////

SECTION("test_Split3") {
  vector<string> lines = StringUtils::split("Hallo\nWorld\\/Me", '\n', '\0');

  CHECK(lines.size() ==  (size_t) 2);

  if (lines.size() == 2) {
    CHECK(lines[0] ==  "Hallo");
    CHECK(lines[1] ==  "World\\/Me");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test_Tolower
////////////////////////////////////////////////////////////////////////////////

SECTION("test_Tolower") {
  string lower = StringUtils::tolower("HaLlO WoRlD!");

  CHECK(lower ==  "hallo world!");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test_uint64
////////////////////////////////////////////////////////////////////////////////

SECTION("test_uint64") {
  CHECK(0ULL ==  StringUtils::uint64("abc"));
  CHECK(0ULL ==  StringUtils::uint64("ABC"));
  CHECK(0ULL ==  StringUtils::uint64(" foo"));
  CHECK(0ULL ==  StringUtils::uint64(""));
  CHECK(0ULL ==  StringUtils::uint64(" "));
  CHECK(12ULL ==  StringUtils::uint64("012"));
  CHECK(12ULL ==  StringUtils::uint64("00012"));
  CHECK(1234ULL ==  StringUtils::uint64("1234"));
  CHECK(1234ULL ==  StringUtils::uint64("1234a"));
#ifdef TRI_STRING_UTILS_USE_FROM_CHARS
  CHECK(0ULL ==  StringUtils::uint64("-1"));
  CHECK(0ULL ==  StringUtils::uint64("-12345"));
#else
  CHECK(18446744073709551615ULL ==  StringUtils::uint64("-1"));
  CHECK(18446744073709539271ULL ==  StringUtils::uint64("-12345"));
#endif
  CHECK(1234ULL ==  StringUtils::uint64("1234.56"));
  CHECK(0ULL ==  StringUtils::uint64("1234567890123456789012345678901234567890"));
  CHECK(0ULL ==  StringUtils::uint64("@"));

  CHECK(0ULL ==  StringUtils::uint64("0"));
  CHECK(1ULL ==  StringUtils::uint64("1"));
  CHECK(12ULL ==  StringUtils::uint64("12"));
  CHECK(123ULL ==  StringUtils::uint64("123"));
  CHECK(1234ULL ==  StringUtils::uint64("1234"));
  CHECK(1234ULL ==  StringUtils::uint64("01234"));
  CHECK(9ULL ==  StringUtils::uint64("9"));
  CHECK(9ULL ==  StringUtils::uint64("09"));
  CHECK(9ULL ==  StringUtils::uint64("0009"));
  CHECK(12345678ULL ==  StringUtils::uint64("12345678"));
  CHECK(1234567800ULL ==  StringUtils::uint64("1234567800"));
  CHECK(1234567890123456ULL ==  StringUtils::uint64("1234567890123456"));
  CHECK(UINT64_MAX ==  StringUtils::uint64(std::to_string(UINT64_MAX)));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test_uint64_trusted
////////////////////////////////////////////////////////////////////////////////

SECTION("test_uint64_trusted") {
  CHECK(0ULL ==  StringUtils::uint64_trusted("0"));
  CHECK(1ULL ==  StringUtils::uint64_trusted("1"));
  CHECK(12ULL ==  StringUtils::uint64_trusted("12"));
  CHECK(123ULL ==  StringUtils::uint64_trusted("123"));
  CHECK(1234ULL ==  StringUtils::uint64_trusted("1234"));
  CHECK(1234ULL ==  StringUtils::uint64_trusted("01234"));
  CHECK(9ULL ==  StringUtils::uint64_trusted("9"));
  CHECK(9ULL ==  StringUtils::uint64_trusted("0009"));
  CHECK(12345678ULL ==  StringUtils::uint64_trusted("12345678"));
  CHECK(1234567800ULL ==  StringUtils::uint64_trusted("1234567800"));
  CHECK(1234567890123456ULL ==  StringUtils::uint64_trusted("1234567890123456"));
  CHECK(UINT64_MAX ==  StringUtils::uint64_trusted(std::to_string(UINT64_MAX)));
}

SECTION("test_encodeHex") {
  CHECK("" ==  StringUtils::encodeHex(""));

  CHECK("00" ==  StringUtils::encodeHex(std::string("\x00", 1)));
  CHECK("01" ==  StringUtils::encodeHex("\x01"));
  CHECK("02" ==  StringUtils::encodeHex("\x02"));
  CHECK("03" ==  StringUtils::encodeHex("\x03"));
  CHECK("04" ==  StringUtils::encodeHex("\x04"));
  CHECK("05" ==  StringUtils::encodeHex("\x05"));
  CHECK("06" ==  StringUtils::encodeHex("\x06"));
  CHECK("07" ==  StringUtils::encodeHex("\x07"));
  CHECK("08" ==  StringUtils::encodeHex("\x08"));
  CHECK("09" ==  StringUtils::encodeHex("\x09"));
  CHECK("0a" ==  StringUtils::encodeHex("\x0a"));
  CHECK("0b" ==  StringUtils::encodeHex("\x0b"));
  CHECK("0c" ==  StringUtils::encodeHex("\x0c"));
  CHECK("0d" ==  StringUtils::encodeHex("\x0d"));
  CHECK("0e" ==  StringUtils::encodeHex("\x0e"));
  CHECK("0f" ==  StringUtils::encodeHex("\x0f"));

  CHECK("10" ==  StringUtils::encodeHex("\x10"));
  CHECK("42" ==  StringUtils::encodeHex("\x42"));
  CHECK("ff" ==  StringUtils::encodeHex("\xff"));
  CHECK("aa0009" ==  StringUtils::encodeHex(std::string("\xaa\x00\x09", 3)));
  CHECK("000102" ==  StringUtils::encodeHex(std::string("\x00\x01\x02", 3)));
  CHECK("00010203" ==  StringUtils::encodeHex(std::string("\x00\x01\x02\03", 4)));
  CHECK("20" ==  StringUtils::encodeHex(" "));
  CHECK("2a2a" ==  StringUtils::encodeHex("**"));
  CHECK("616263646566" ==  StringUtils::encodeHex("abcdef"));
  CHECK("4142434445462047" ==  StringUtils::encodeHex("ABCDEF G"));
  CHECK("54686520517569636b2062726f776e20466f78206a756d706564206f76657220746865206c617a7920646f6721" ==  StringUtils::encodeHex("The Quick brown Fox jumped over the lazy dog!"));
  CHECK("446572204bc3b674c3b67220737072c3bc6e6720c3bc62657220646965204272c3bc636b65" ==  StringUtils::encodeHex("Der Kötör sprüng über die Brücke"));
  CHECK("c3a4c3b6c3bcc39fc384c396c39ce282acc2b5" == StringUtils::encodeHex("äöüßÄÖÜ€µ"));
}

SECTION("test_decodeHex") {
  CHECK("" ==  StringUtils::decodeHex(""));

  CHECK(std::string("\x00", 1) ==  StringUtils::decodeHex("00"));
  CHECK("\x01" ==  StringUtils::decodeHex("01"));
  CHECK("\x02" ==  StringUtils::decodeHex("02"));
  CHECK("\x03" ==  StringUtils::decodeHex("03"));
  CHECK("\x04" ==  StringUtils::decodeHex("04"));
  CHECK("\x05" ==  StringUtils::decodeHex("05"));
  CHECK("\x06" ==  StringUtils::decodeHex("06"));
  CHECK("\x07" ==  StringUtils::decodeHex("07"));
  CHECK("\x08" ==  StringUtils::decodeHex("08"));
  CHECK("\x09" ==  StringUtils::decodeHex("09"));
  CHECK("\x0a" ==  StringUtils::decodeHex("0a"));
  CHECK("\x0b" ==  StringUtils::decodeHex("0b"));
  CHECK("\x0c" ==  StringUtils::decodeHex("0c"));
  CHECK("\x0d" ==  StringUtils::decodeHex("0d"));
  CHECK("\x0e" ==  StringUtils::decodeHex("0e"));
  CHECK("\x0f" ==  StringUtils::decodeHex("0f"));
  CHECK("\x0a" ==  StringUtils::decodeHex("0A"));
  CHECK("\x0b" ==  StringUtils::decodeHex("0B"));
  CHECK("\x0c" ==  StringUtils::decodeHex("0C"));
  CHECK("\x0d" ==  StringUtils::decodeHex("0D"));
  CHECK("\x0e" ==  StringUtils::decodeHex("0E"));
  CHECK("\x0f" ==  StringUtils::decodeHex("0F"));
  
  CHECK("\x1a" ==  StringUtils::decodeHex("1a"));
  CHECK("\x2b" ==  StringUtils::decodeHex("2b"));
  CHECK("\x3c" ==  StringUtils::decodeHex("3c"));
  CHECK("\x4d" ==  StringUtils::decodeHex("4d"));
  CHECK("\x5e" ==  StringUtils::decodeHex("5e"));
  CHECK("\x6f" ==  StringUtils::decodeHex("6f"));
  CHECK("\x7a" ==  StringUtils::decodeHex("7A"));
  CHECK("\x8b" ==  StringUtils::decodeHex("8B"));
  CHECK("\x9c" ==  StringUtils::decodeHex("9C"));
  CHECK("\xad" ==  StringUtils::decodeHex("AD"));
  CHECK("\xbe" ==  StringUtils::decodeHex("BE"));
  CHECK("\xcf" ==  StringUtils::decodeHex("CF"));
  CHECK("\xdf" ==  StringUtils::decodeHex("df"));
  CHECK("\xef" ==  StringUtils::decodeHex("eF"));
  CHECK("\xff" ==  StringUtils::decodeHex("ff"));
  
  CHECK(" " ==  StringUtils::decodeHex("20"));
  CHECK("**" ==  StringUtils::decodeHex("2a2a"));
  CHECK("abcdef" ==  StringUtils::decodeHex("616263646566"));
  CHECK("ABCDEF G" == StringUtils::decodeHex("4142434445462047"));

  CHECK("The Quick brown Fox jumped over the lazy dog!" == StringUtils::decodeHex("54686520517569636b2062726f776e20466f78206a756d706564206f76657220746865206c617a7920646f6721"));
  CHECK("Der Kötör sprüng über die Brücke" == StringUtils::decodeHex("446572204bc3b674c3b67220737072c3bc6e6720c3bc62657220646965204272c3bc636b65"));
  CHECK("äöüßÄÖÜ€µ" == StringUtils::decodeHex("c3a4c3b6c3bcc39fc384c396c39ce282acc2b5"));
  
  CHECK("" ==  StringUtils::decodeHex("1"));
  CHECK("" ==  StringUtils::decodeHex(" "));
  CHECK("" ==  StringUtils::decodeHex(" 2"));
  CHECK("" ==  StringUtils::decodeHex("1 "));
  CHECK("" ==  StringUtils::decodeHex("12 "));
  CHECK("" ==  StringUtils::decodeHex("x"));
  CHECK("" ==  StringUtils::decodeHex("X"));
  CHECK("" ==  StringUtils::decodeHex("@@@"));
  CHECK("" ==  StringUtils::decodeHex("111"));
  CHECK("" ==  StringUtils::decodeHex("1 2 3"));
  CHECK("" ==  StringUtils::decodeHex("1122334"));
  CHECK("" ==  StringUtils::decodeHex("112233 "));
  CHECK("" ==  StringUtils::decodeHex(" 112233"));
  CHECK("" ==  StringUtils::decodeHex("abcdefgh"));
}

}

