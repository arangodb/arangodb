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

#include "Basics/directories.h"
#include "Basics/StringUtils.h"
#include "Basics/Utf8Helper.h"
#include "Basics/files.h"

#include "icu-helper.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace std;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief hex dump with ':' separator
////////////////////////////////////////////////////////////////////////////////

static std::string hexdump(std::string const& s) {
  std::ostringstream oss;
  oss.imbue(locale());
  bool first = true;

  for (std::string::const_iterator it = s.begin();  it != s.end();  it++) {
    oss << (first ? "" : ":") << hex << setw(2) << setfill('y') << std::string::traits_type::to_int_type(*it);
    first = false;
  }

  return oss.str();
}

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
/// @brief test_convertUTF16ToUTF8
////////////////////////////////////////////////////////////////////////////////

SECTION("test_convertUTF16ToUTF8") {
  string result;
  bool isOk;

  // both surrogates are valid
  isOk = StringUtils::convertUTF16ToUTF8("D8A4\0", "dd42\0", result);

  CHECK(isOk);
  CHECK(result.length() ==  (size_t) 4);
  CHECK("f0:b9:85:82" ==  hexdump(result));

  result.clear();

  // wrong low surrogate
  isOk = StringUtils::convertUTF16ToUTF8("DD42", "D8A4", result);

  CHECK(! isOk);
  CHECK(result.empty());

  result.clear();

  // wrong high surrogate
  isOk = StringUtils::convertUTF16ToUTF8("DC00", "DC1A", result);

  CHECK(! isOk);
  CHECK(result.empty());
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
  CHECK(12ULL ==  StringUtils::uint64(" 012"));
  CHECK(1234ULL ==  StringUtils::uint64("1234a"));
  CHECK(18446744073709551615ULL ==  StringUtils::uint64("-1"));
  CHECK(18446744073709539271ULL ==  StringUtils::uint64("-12345"));
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
  CHECK(9ULL ==  StringUtils::uint64(" 9"));
  CHECK(9ULL ==  StringUtils::uint64("0009"));
  CHECK(12345678ULL ==  StringUtils::uint64("12345678"));
  CHECK(1234567800ULL ==  StringUtils::uint64("1234567800"));
  CHECK(1234567890123456ULL ==  StringUtils::uint64("1234567890123456"));
  CHECK(UINT64_MAX ==  StringUtils::uint64(std::to_string(UINT64_MAX)));
}

SECTION("test_uint64_check") {
  CHECK_THROWS_AS(StringUtils::uint64_check("abc"), std::invalid_argument);
  CHECK_THROWS_AS(StringUtils::uint64_check("ABC"), std::invalid_argument);
  CHECK_THROWS_AS(StringUtils::uint64_check(" foo"), std::invalid_argument);
  CHECK_THROWS_AS(StringUtils::uint64_check(""), std::invalid_argument);
  CHECK_THROWS_AS(StringUtils::uint64_check(" "), std::invalid_argument);
  CHECK(12ULL ==  StringUtils::uint64_check(" 012"));
  CHECK_THROWS_AS(StringUtils::uint64_check("1234a"), std::invalid_argument);
  CHECK(18446744073709551615ULL ==  StringUtils::uint64_check("-1"));
  CHECK(18446744073709539271ULL ==  StringUtils::uint64_check("-12345"));
  CHECK_THROWS_AS(StringUtils::uint64_check("1234."), std::invalid_argument);
  CHECK_THROWS_AS(StringUtils::uint64_check("1234.56"), std::invalid_argument);
  CHECK_THROWS_AS(StringUtils::uint64_check("1234567889123456789012345678901234567890"), std::out_of_range);
  CHECK_THROWS_AS(StringUtils::uint64_check("@"), std::invalid_argument);

  CHECK(0ULL ==  StringUtils::uint64_check("0"));
  CHECK(1ULL ==  StringUtils::uint64_check("1"));
  CHECK(12ULL ==  StringUtils::uint64_check("12"));
  CHECK(123ULL ==  StringUtils::uint64_check("123"));
  CHECK(1234ULL ==  StringUtils::uint64_check("1234"));
  CHECK(1234ULL ==  StringUtils::uint64_check("01234"));
  CHECK(9ULL ==  StringUtils::uint64_check("9"));
  CHECK(9ULL ==  StringUtils::uint64_check(" 9"));
  CHECK(9ULL ==  StringUtils::uint64_check("0009"));
  CHECK(12345678ULL ==  StringUtils::uint64_check("12345678"));
  CHECK(1234567800ULL ==  StringUtils::uint64_check("1234567800"));
  CHECK(1234567890123456ULL ==  StringUtils::uint64_check("1234567890123456"));
  CHECK(UINT64_MAX ==  StringUtils::uint64_check(std::to_string(UINT64_MAX)));
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

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
