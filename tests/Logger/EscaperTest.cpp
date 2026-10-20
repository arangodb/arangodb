////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "Logger/Escaper.h"

#include <functional>
#include <memory>
#include <string>

using namespace arangodb;

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

class EscaperTest : public ::testing::Test {
 protected:
  std::string asciiVisibleChars;
  std::string bigString;
  std::string controlChars;

  EscaperTest() {
    for (int i = 33; i <= 126; ++i) {
      asciiVisibleChars += i;
    }
    for (int i = 0; i <= 31; ++i) {
      controlChars += i;
    }
    while (bigString.size() < 1000) {
      bigString += asciiVisibleChars;
    }
  }
};

void verifyExpectedValues(
    std::string const& inputString, std::string const& expectedOutput,
    std::function<void(std::string const&, std::string&)> const& writerFn) {
  std::string output;
  writerFn(inputString, output);
  EXPECT_EQ(output, expectedOutput);
}

TEST_F(EscaperTest, test_suppress_control_retain_unicode) {
  std::function<void(std::string const&, std::string&)> writerFn =
      &Escaper<ControlCharsSuppressor,
               UnicodeCharsRetainer>::writeIntoOutputBuffer;
  verifyExpectedValues(asciiVisibleChars, asciiVisibleChars, writerFn);
  verifyExpectedValues(bigString, bigString, writerFn);
  verifyExpectedValues(controlChars, "                                ",
                       writerFn);
  verifyExpectedValues("€", "€", writerFn);
  verifyExpectedValues(" €  ", " €  ", writerFn);
  verifyExpectedValues("mötör", "mötör", writerFn);
  verifyExpectedValues("\t mötör", "  mötör", writerFn);
  verifyExpectedValues("maçã", "maçã", writerFn);
  verifyExpectedValues("\nmaçã", " maçã", writerFn);
  verifyExpectedValues("犬", "犬", writerFn);
  verifyExpectedValues("犬\r", "犬 ", writerFn);
  verifyExpectedValues("", "", writerFn);
  verifyExpectedValues("a", "a", writerFn);
  verifyExpectedValues("𐍈", "𐍈", writerFn);    //\uD800\uDF48
  verifyExpectedValues("𐍈 ", "𐍈 ", writerFn);  //\uD800\uDF48
  std::string validUnicode = "€";
  verifyExpectedValues(validUnicode.substr(0, 1), "?", writerFn);
  verifyExpectedValues(validUnicode.substr(0, 1) + "\n", "? ", writerFn);
  verifyExpectedValues("\x07", " ", writerFn);
  verifyExpectedValues(std::string("\0", 1), " ", writerFn);
  validUnicode = "𐍈";
  verifyExpectedValues(validUnicode.substr(0, 1), "?", writerFn);
  verifyExpectedValues(validUnicode.substr(0, 1) + "\n", "? ", writerFn);
}

TEST_F(EscaperTest, test_suppress_control_escape_unicode) {
  std::function<void(std::string const&, std::string&)> writerFn =
      &Escaper<ControlCharsSuppressor,
               UnicodeCharsEscaper>::writeIntoOutputBuffer;
  verifyExpectedValues(asciiVisibleChars, asciiVisibleChars, writerFn);
  verifyExpectedValues(bigString, bigString, writerFn);
  verifyExpectedValues(controlChars, "                                ",
                       writerFn);
  verifyExpectedValues("€", "\\u20AC", writerFn);
  verifyExpectedValues(" €  ", " \\u20AC  ", writerFn);
  verifyExpectedValues("mötör", "m\\u00F6t\\u00F6r", writerFn);
  verifyExpectedValues("\tmötör", " m\\u00F6t\\u00F6r", writerFn);
  verifyExpectedValues("maçã", "ma\\u00E7\\u00E3", writerFn);
  verifyExpectedValues("\nmaçã", " ma\\u00E7\\u00E3", writerFn);
  verifyExpectedValues("犬", "\\u72AC", writerFn);
  verifyExpectedValues("犬\r", "\\u72AC ", writerFn);
  verifyExpectedValues("", "", writerFn);
  verifyExpectedValues("a", "a", writerFn);
  verifyExpectedValues("𐍈", "\\uD800\\uDF48", writerFn);    //\uD800\uDF48
  verifyExpectedValues("𐍈 ", "\\uD800\\uDF48 ", writerFn);  //\uD800\uDF48
  std::string validUnicode = "€";
  verifyExpectedValues(validUnicode.substr(0, 1), "?", writerFn);
  verifyExpectedValues(validUnicode.substr(0, 1) + "\n", "? ", writerFn);
  validUnicode = "𐍈";
  verifyExpectedValues(validUnicode.substr(0, 1), "?", writerFn);
  verifyExpectedValues(validUnicode.substr(0, 1) + "\n", "? ", writerFn);
  verifyExpectedValues("\x07", " ", writerFn);
  verifyExpectedValues(std::string("\0", 1), " ", writerFn);
}

TEST_F(EscaperTest, test_escape_control_retain_unicode) {
  std::function<void(std::string const&, std::string&)> writerFn =
      &Escaper<ControlCharsEscaper,
               UnicodeCharsRetainer>::writeIntoOutputBuffer;
  verifyExpectedValues(asciiVisibleChars, asciiVisibleChars, writerFn);
  verifyExpectedValues(bigString, bigString, writerFn);
  verifyExpectedValues(
      controlChars,
      "\\x00\\x01\\x02\\x03\\x04\\x05\\x06\\x07\\b\\t\\n\\x0B\\f\\r"
      "\\x0E\\x0F\\x10\\x11\\x12\\x13\\x14\\x15\\x16\\x17\\x18\\x19\\x1A\\x1B\\"
      "x1C\\x1D\\x1E\\x1F",
      writerFn);
  verifyExpectedValues("€", "€", writerFn);
  verifyExpectedValues(" €  ", " €  ", writerFn);
  verifyExpectedValues("mötör", "mötör", writerFn);
  verifyExpectedValues("\tmötör", "\\tmötör", writerFn);
  verifyExpectedValues("maçã", "maçã", writerFn);
  verifyExpectedValues("\nmaçã", "\\nmaçã", writerFn);
  verifyExpectedValues("犬", "犬", writerFn);
  verifyExpectedValues("犬\r", "犬\\r", writerFn);
  verifyExpectedValues("", "", writerFn);
  verifyExpectedValues("a", "a", writerFn);
  verifyExpectedValues("𐍈", "𐍈", writerFn);    //\uD800\uDF48
  verifyExpectedValues("𐍈 ", "𐍈 ", writerFn);  //\uD800\uDF48
  std::string validUnicode = "€";
  verifyExpectedValues(validUnicode.substr(0, 1), "?", writerFn);
  verifyExpectedValues(validUnicode.substr(0, 1) + "\n", "?\\n", writerFn);
  validUnicode = "𐍈";
  verifyExpectedValues(validUnicode.substr(0, 1), "?", writerFn);
  verifyExpectedValues(validUnicode.substr(0, 1) + "\n", "?\\n", writerFn);
  verifyExpectedValues("\x07", "\\x07", writerFn);
  verifyExpectedValues(std::string("\0", 1), "\\x00", writerFn);
}

TEST_F(EscaperTest, test_escape_control_escape_unicode) {
  std::function<void(std::string const&, std::string&)> writerFn =
      &Escaper<ControlCharsEscaper, UnicodeCharsEscaper>::writeIntoOutputBuffer;
  verifyExpectedValues(asciiVisibleChars, asciiVisibleChars, writerFn);
  verifyExpectedValues(bigString, bigString, writerFn);
  verifyExpectedValues(
      controlChars,
      "\\x00\\x01\\x02\\x03\\x04\\x05\\x06\\x07\\b\\t\\n\\x0B\\f\\r"
      "\\x0E\\x0F\\x10\\x11\\x12\\x13\\x14\\x15\\x16\\x17\\x18\\x19\\x1A\\x1B\\"
      "x1C\\x1D\\x1E\\x1F",
      writerFn);
  verifyExpectedValues("€", "\\u20AC", writerFn);
  verifyExpectedValues(" €  ", " \\u20AC  ", writerFn);
  verifyExpectedValues("mötör", "m\\u00F6t\\u00F6r", writerFn);
  verifyExpectedValues("\tmötör", "\\tm\\u00F6t\\u00F6r", writerFn);
  verifyExpectedValues("maçã", "ma\\u00E7\\u00E3", writerFn);
  verifyExpectedValues("\nmaçã", "\\nma\\u00E7\\u00E3", writerFn);
  verifyExpectedValues("犬", "\\u72AC", writerFn);
  verifyExpectedValues("犬\r", "\\u72AC\\r", writerFn);
  verifyExpectedValues("", "", writerFn);
  verifyExpectedValues("a", "a", writerFn);
  verifyExpectedValues("𐍈", "\\uD800\\uDF48", writerFn);    //\uD800\uDF48
  verifyExpectedValues("𐍈 ", "\\uD800\\uDF48 ", writerFn);  //\uD800\uDF48
  std::string validUnicode = "€";
  verifyExpectedValues(validUnicode.substr(0, 1), "?", writerFn);
  verifyExpectedValues(validUnicode.substr(0, 1) + "\n", "?\\n", writerFn);
  validUnicode = "𐍈";
  verifyExpectedValues(validUnicode.substr(0, 1), "?", writerFn);
  verifyExpectedValues(validUnicode.substr(0, 1) + "\n", "?\\n", writerFn);
  verifyExpectedValues("\x07", "\\x07", writerFn);
  verifyExpectedValues(std::string("\0", 1), "\\x00", writerFn);
}
