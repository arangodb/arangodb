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
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"

#include "gtest/gtest.h"

#include "Logger/Escaper.h"

#include <string>
#include <memory>

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

template<typename EscaperType>
void verifyExpectedValues(std::string const& inputString,
                          std::string const& expectedOutput,
                          EscaperType& escaper) {
  std::string output;
  escaper.writeIntoOutputBuffer(inputString, output);
  EXPECT_EQ(output.compare(expectedOutput), 0);
  EXPECT_EQ(output, expectedOutput);
}

TEST_F(EscaperTest, test_suppress_control_retain_unicode) {
  Escaper<ControlCharsSuppressor, UnicodeCharsRetainer> escaper;
  verifyExpectedValues(asciiVisibleChars, asciiVisibleChars, escaper);
  verifyExpectedValues(bigString, bigString, escaper);
  verifyExpectedValues(controlChars, "                                ",
                       escaper);
  verifyExpectedValues("€", "€", escaper);
  verifyExpectedValues(" €  ", " €  ", escaper);
  verifyExpectedValues("mötör", "mötör", escaper);
  verifyExpectedValues("\t mötör", "  mötör", escaper);
  verifyExpectedValues("maçã", "maçã", escaper);
  verifyExpectedValues("\nmaçã", " maçã", escaper);
  verifyExpectedValues("犬", "犬", escaper);
  verifyExpectedValues("犬\r", "犬 ", escaper);
  verifyExpectedValues("", "", escaper);
  verifyExpectedValues("a", "a", escaper);
  verifyExpectedValues("𐍈", "𐍈", escaper);    //\uD800\uDF48
  verifyExpectedValues("𐍈 ", "𐍈 ", escaper);  //\uD800\uDF48
  std::string validUnicode = "€";
  verifyExpectedValues(validUnicode.substr(0, 1), "?", escaper);
  verifyExpectedValues(validUnicode.substr(0, 1) + "\n", "? ", escaper);
  verifyExpectedValues("\x07", " ", escaper);
  verifyExpectedValues(std::string("\0", 1), " ", escaper);
  validUnicode = "𐍈";
  verifyExpectedValues(validUnicode.substr(0, 1), "?", escaper);
  verifyExpectedValues(validUnicode.substr(0, 1) + "\n", "? ", escaper);
}

TEST_F(EscaperTest, test_suppress_control_escape_unicode) {
  Escaper<ControlCharsSuppressor, UnicodeCharsEscaper> escaper;
  verifyExpectedValues(asciiVisibleChars, asciiVisibleChars, escaper);
  verifyExpectedValues(bigString, bigString, escaper);
  verifyExpectedValues(controlChars, "                                ",
                       escaper);
  verifyExpectedValues("€", "\\u20AC", escaper);
  verifyExpectedValues(" €  ", " \\u20AC  ", escaper);
  verifyExpectedValues("mötör", "m\\u00F6t\\u00F6r", escaper);
  verifyExpectedValues("\tmötör", " m\\u00F6t\\u00F6r", escaper);
  verifyExpectedValues("maçã", "ma\\u00E7\\u00E3", escaper);
  verifyExpectedValues("\nmaçã", " ma\\u00E7\\u00E3", escaper);
  verifyExpectedValues("犬", "\\u72AC", escaper);
  verifyExpectedValues("犬\r", "\\u72AC ", escaper);
  verifyExpectedValues("", "", escaper);
  verifyExpectedValues("a", "a", escaper);
  verifyExpectedValues("𐍈", "\\uD800\\uDF48", escaper);    //\uD800\uDF48
  verifyExpectedValues("𐍈 ", "\\uD800\\uDF48 ", escaper);  //\uD800\uDF48
  std::string validUnicode = "€";
  verifyExpectedValues(validUnicode.substr(0, 1), "?", escaper);
  verifyExpectedValues(validUnicode.substr(0, 1) + "\n", "? ", escaper);
  validUnicode = "𐍈";
  verifyExpectedValues(validUnicode.substr(0, 1), "?", escaper);
  verifyExpectedValues(validUnicode.substr(0, 1) + "\n", "? ", escaper);
  verifyExpectedValues("\x07", " ", escaper);
  verifyExpectedValues(std::string("\0", 1), " ", escaper);
}

TEST_F(EscaperTest, test_escape_control_retain_unicode) {
  Escaper<ControlCharsEscaper, UnicodeCharsRetainer> escaper;
  verifyExpectedValues(asciiVisibleChars, asciiVisibleChars, escaper);
  verifyExpectedValues(bigString, bigString, escaper);
  verifyExpectedValues(
      controlChars,
      "\\x00\\x01\\x02\\x03\\x04\\x05\\x06\\x07\\x08\\t\\n\\x0B\\x0C\\r"
      "\\x0E\\x0F\\x10\\x11\\x12\\x13\\x14\\x15\\x16\\x17\\x18\\x19\\x1A\\x1B\\"
      "x1C\\x1D\\x1E\\x1F",
      escaper);
  verifyExpectedValues("€", "€", escaper);
  verifyExpectedValues(" €  ", " €  ", escaper);
  verifyExpectedValues("mötör", "mötör", escaper);
  verifyExpectedValues("\tmötör", "\\tmötör", escaper);
  verifyExpectedValues("maçã", "maçã", escaper);
  verifyExpectedValues("\nmaçã", "\\nmaçã", escaper);
  verifyExpectedValues("犬", "犬", escaper);
  verifyExpectedValues("犬\r", "犬\\r", escaper);
  verifyExpectedValues("", "", escaper);
  verifyExpectedValues("a", "a", escaper);
  verifyExpectedValues("𐍈", "𐍈", escaper);    //\uD800\uDF48
  verifyExpectedValues("𐍈 ", "𐍈 ", escaper);  //\uD800\uDF48
  std::string validUnicode = "€";
  verifyExpectedValues(validUnicode.substr(0, 1), "?", escaper);
  verifyExpectedValues(validUnicode.substr(0, 1) + "\n", "?\\n", escaper);
  validUnicode = "𐍈";
  verifyExpectedValues(validUnicode.substr(0, 1), "?", escaper);
  verifyExpectedValues(validUnicode.substr(0, 1) + "\n", "?\\n", escaper);
  verifyExpectedValues("\x07", "\\x07", escaper);
  verifyExpectedValues(std::string("\0", 1), "\\x00", escaper);
}

TEST_F(EscaperTest, test_escape_control_escape_unicode) {
  Escaper<ControlCharsEscaper, UnicodeCharsEscaper> escaper;
  verifyExpectedValues(asciiVisibleChars, asciiVisibleChars, escaper);
  verifyExpectedValues(bigString, bigString, escaper);
  verifyExpectedValues(
      controlChars,
      "\\x00\\x01\\x02\\x03\\x04\\x05\\x06\\x07\\x08\\t\\n\\x0B\\x0C\\r"
      "\\x0E\\x0F\\x10\\x11\\x12\\x13\\x14\\x15\\x16\\x17\\x18\\x19\\x1A\\x1B\\"
      "x1C\\x1D\\x1E\\x1F",
      escaper);
  verifyExpectedValues("€", "\\u20AC", escaper);
  verifyExpectedValues(" €  ", " \\u20AC  ", escaper);
  verifyExpectedValues("mötör", "m\\u00F6t\\u00F6r", escaper);
  verifyExpectedValues("\tmötör", "\\tm\\u00F6t\\u00F6r", escaper);
  verifyExpectedValues("maçã", "ma\\u00E7\\u00E3", escaper);
  verifyExpectedValues("\nmaçã", "\\nma\\u00E7\\u00E3", escaper);
  verifyExpectedValues("犬", "\\u72AC", escaper);
  verifyExpectedValues("犬\r", "\\u72AC\\r", escaper);
  verifyExpectedValues("", "", escaper);
  verifyExpectedValues("a", "a", escaper);
  verifyExpectedValues("𐍈", "\\uD800\\uDF48", escaper);    //\uD800\uDF48
  verifyExpectedValues("𐍈 ", "\\uD800\\uDF48 ", escaper);  //\uD800\uDF48
  std::string validUnicode = "€";
  verifyExpectedValues(validUnicode.substr(0, 1), "?", escaper);
  verifyExpectedValues(validUnicode.substr(0, 1) + "\n", "?\\n", escaper);
  validUnicode = "𐍈";
  verifyExpectedValues(validUnicode.substr(0, 1), "?", escaper);
  verifyExpectedValues(validUnicode.substr(0, 1) + "\n", "?\\n", escaper);
  verifyExpectedValues("\x07", "\\x07", escaper);
  verifyExpectedValues(std::string("\0", 1), "\\x00", escaper);
}
