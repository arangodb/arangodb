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

#include <boost/test/unit_test.hpp>

#include "Basics/StringUtils.h"

#include <iomanip>

using namespace triagens;
using namespace triagens::basics;
using namespace std;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief hex dump with ':' separator
////////////////////////////////////////////////////////////////////////////////

static string hexedump (const string &s) {
  ostringstream oss;
  oss.imbue(locale());
  bool first = true;

  for (string::const_iterator it = s.begin();  it != s.end();  it++) {
    oss << (first ? "" : ":") << hex << setw(2) << setfill('y') << string::traits_type::to_int_type(*it);
    first = false;
  }

  return oss.str();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct StringUtilsSetup {
  StringUtilsSetup () {
    BOOST_TEST_MESSAGE("setup StringUtils");
  }

  ~StringUtilsSetup () {
    BOOST_TEST_MESSAGE("teardown StringUtils");
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

BOOST_FIXTURE_TEST_SUITE (StringUtilsTest, StringUtilsSetup)

////////////////////////////////////////////////////////////////////////////////
/// @brief test_Split1
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (test_Split1) {
  vector<string> lines = StringUtils::split("Hallo\nWorld\\/Me", '\n');

  BOOST_CHECK_EQUAL(lines.size(), (size_t) 2);

  if (lines.size() == 2) {
    BOOST_CHECK_EQUAL(lines[0], "Hallo");
    BOOST_CHECK_EQUAL(lines[1], "World/Me");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test_Split2
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (test_Split2) {
  vector<string> lines = StringUtils::split("\nHallo\nWorld\n", '\n');

  BOOST_CHECK_EQUAL(lines.size(), (size_t) 4);

  if (lines.size() == 4) {
    BOOST_CHECK_EQUAL(lines[0], "");
    BOOST_CHECK_EQUAL(lines[1], "Hallo");
    BOOST_CHECK_EQUAL(lines[2], "World");
    BOOST_CHECK_EQUAL(lines[3], "");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test_Split3
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (test_Split3) {
  vector<string> lines = StringUtils::split("Hallo\nWorld\\/Me", '\n', '\0');

  BOOST_CHECK_EQUAL(lines.size(), (size_t) 2);

  if (lines.size() == 2) {
    BOOST_CHECK_EQUAL(lines[0], "Hallo");
    BOOST_CHECK_EQUAL(lines[1], "World\\/Me");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test_Tolower
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (test_Tolower) {
  string lower = StringUtils::tolower("HaLlO WoRlD!");

  BOOST_CHECK_EQUAL(lower, "hallo world!");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test_convertUTF16ToUTF8
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (test_convertUTF16ToUTF8) {
  string result;
  bool isOk;

  // both surrogates are valid
  isOk = StringUtils::convertUTF16ToUTF8("D8A4\0", "dd42\0", result);

  BOOST_CHECK(isOk);
  BOOST_CHECK_EQUAL(result.length(), (size_t) 4);
  BOOST_CHECK_EQUAL("f0:b9:85:82", hexedump(result));

  result.clear();

  // wrong low surrogate
  isOk = StringUtils::convertUTF16ToUTF8("DD42", "D8A4", result);

  BOOST_CHECK(! isOk);
  BOOST_CHECK(result.empty());

  result.clear();

  // wrong high surrogate
  isOk = StringUtils::convertUTF16ToUTF8("DC00", "DC1A", result);

  BOOST_CHECK(! isOk);
  BOOST_CHECK(result.empty());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_SUITE_END()

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
