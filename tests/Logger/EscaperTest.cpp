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
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"

#include "gtest/gtest.h"

#include "Logger/Escaper.h"

#include <string.h>
#include <string>

#ifdef TRI_HAVE_UNISTD_H
#include <unistd.h>
#endif

using namespace arangodb;

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

template <typename EscaperType>
void verifyExpectedValues(std::string const& inputString, std::string const& expectedOutput,
                          size_t expectedSize, EscaperType& escaper) {
  size_t messageSize = escaper.determineOutputBufferSize(inputString);
  EXPECT_EQ(messageSize, expectedSize);
  auto buffer = std::make_unique<char[]>(messageSize);
  char* output = buffer.get();
  escaper.writeIntoOutputBuffer(inputString, output);
  size_t outputBufferSize = output - buffer.get();
  std::string outputString(buffer.get(), outputBufferSize);
  EXPECT_EQ(outputString.compare(expectedOutput), 0);
  EXPECT_EQ(outputString, expectedOutput);
}

TEST(EscaperTest, test_suppress_control_retain_unicode) {
  Escaper<ControlCharsSuppressor, UnicodeCharsRetainer> escaper;
  verifyExpectedValues("€", "€", 12, escaper);
  verifyExpectedValues(" €  ", " €  ", 24, escaper);
  verifyExpectedValues("mötör", "mötör", 28, escaper);
  verifyExpectedValues("\t mötör", "  mötör", 36, escaper);
  verifyExpectedValues("maçã", "maçã", 24, escaper);
  verifyExpectedValues("\nmaçã", " maçã", 28, escaper);
  verifyExpectedValues("犬", "犬", 12, escaper);
  verifyExpectedValues("犬\r", "犬 ", 16, escaper);
  verifyExpectedValues("", "", 0, escaper);
  verifyExpectedValues("a", "a", 4, escaper);
  verifyExpectedValues("𐍈", "𐍈", 16, escaper); //\uD800\uDF48
  verifyExpectedValues("𐍈 ", "𐍈 ", 20, escaper); //\uD800\uDF48
  std::string validUnicode = "€";
  verifyExpectedValues(validUnicode.substr(0, 1), "?", 4, escaper);
  verifyExpectedValues(validUnicode.substr(0, 1) + "\n", "? ", 8, escaper);
  verifyExpectedValues("\x07", " ", 4, escaper);
  verifyExpectedValues(std::string("\0", 1), " ", 4, escaper);
  validUnicode = "𐍈";
  verifyExpectedValues(validUnicode.substr(0, 1), "?", 4, escaper);
  verifyExpectedValues(validUnicode.substr(0, 1) + "\n", "? ", 8, escaper);
  // invalid unicode: '\ufffe', '\U110000','\ud800', 'test\xFE'
}

TEST(EscaperTest, test_suppress_control_escape_unicode) {
  Escaper<ControlCharsSuppressor, UnicodeCharsEscaper> escaper;
  verifyExpectedValues("€", "\\u20AC", 18, escaper);
  verifyExpectedValues(" €  ", " \\u20AC  ", 36, escaper);
  verifyExpectedValues("mötör", "m\\u00F6t\\u00F6r", 42, escaper);
  verifyExpectedValues("\tmötör", " m\\u00F6t\\u00F6r", 48, escaper);
  verifyExpectedValues("maçã", "ma\\u00E7\\u00E3", 36, escaper);
  verifyExpectedValues("\nmaçã", " ma\\u00E7\\u00E3", 42, escaper);
  verifyExpectedValues("犬", "\\u72AC", 18, escaper);
  verifyExpectedValues("犬\r", "\\u72AC ", 24, escaper);
  verifyExpectedValues("", "", 0, escaper);
  verifyExpectedValues("a", "a", 6, escaper);
  verifyExpectedValues("𐍈", "\\uD800\\uDF48", 24, escaper); //\uD800\uDF48
  verifyExpectedValues("𐍈 ", "\\uD800\\uDF48 ", 30, escaper); //\uD800\uDF48
  std::string validUnicode = "€";
  verifyExpectedValues(validUnicode.substr(0, 1), "?", 6, escaper);
  verifyExpectedValues(validUnicode.substr(0, 1) + "\n", "? ", 12, escaper);
  validUnicode = "𐍈";
  verifyExpectedValues(validUnicode.substr(0, 1), "?", 6, escaper);
  verifyExpectedValues(validUnicode.substr(0, 1) + "\n", "? ", 12, escaper);
  verifyExpectedValues("\x07", " ", 6, escaper);
  verifyExpectedValues(std::string("\0", 1), " ", 6, escaper);
}
TEST(EscaperTest, test_escape_control_retain_unicode) {
  Escaper<ControlCharsEscaper, UnicodeCharsRetainer> escaper;
  verifyExpectedValues("€", "€", 12, escaper);
  verifyExpectedValues(" €  ", " €  ", 24, escaper);
  verifyExpectedValues("mötör", "mötör", 28, escaper);
  verifyExpectedValues("\tmötör", "\\tmötör", 32, escaper);
  verifyExpectedValues("maçã", "maçã", 24, escaper);
  verifyExpectedValues("\nmaçã", "\\nmaçã", 28, escaper);
  verifyExpectedValues("犬", "犬", 12, escaper);
  verifyExpectedValues("犬\r", "犬\\r", 16, escaper);
  verifyExpectedValues("", "", 0, escaper);
  verifyExpectedValues("a", "a", 4, escaper);
  verifyExpectedValues("𐍈", "𐍈", 16, escaper); //\uD800\uDF48
  verifyExpectedValues("𐍈 ", "𐍈 ", 20, escaper); //\uD800\uDF48
  std::string validUnicode = "€";
  verifyExpectedValues(validUnicode.substr(0, 1), "?", 4, escaper);
  verifyExpectedValues(validUnicode.substr(0, 1) + "\n", "?\\n", 8, escaper);
  validUnicode = "𐍈";
  verifyExpectedValues(validUnicode.substr(0, 1), "?", 4, escaper);
  verifyExpectedValues(validUnicode.substr(0, 1) + "\n", "?\\n", 8, escaper);
  verifyExpectedValues("\x07", "\\x07", 4, escaper);
  verifyExpectedValues(std::string("\0", 1), "\\x00", 4, escaper);
}
TEST(EscaperTest, test_escape_control_escape_unicode) {
  Escaper<ControlCharsEscaper, UnicodeCharsEscaper> escaper;
  verifyExpectedValues("€", "\\u20AC", 18, escaper);
  verifyExpectedValues(" €  ", " \\u20AC  ", 36, escaper);
  verifyExpectedValues("mötör", "m\\u00F6t\\u00F6r", 42, escaper);
  verifyExpectedValues("\tmötör", "\\tm\\u00F6t\\u00F6r", 48, escaper);
  verifyExpectedValues("maçã", "ma\\u00E7\\u00E3", 36, escaper);
  verifyExpectedValues("\nmaçã", "\\nma\\u00E7\\u00E3", 42, escaper);
  verifyExpectedValues("犬", "\\u72AC", 18, escaper);
  verifyExpectedValues("犬\r", "\\u72AC\\r", 24, escaper);
  verifyExpectedValues("", "", 0, escaper);
  verifyExpectedValues("a", "a", 6, escaper);
  verifyExpectedValues("𐍈", "\\uD800\\uDF48", 24, escaper); //\uD800\uDF48
  verifyExpectedValues("𐍈 ", "\\uD800\\uDF48 ", 30, escaper); //\uD800\uDF48
  std::string validUnicode = "€";
  verifyExpectedValues(validUnicode.substr(0, 1), "?", 6, escaper);
  verifyExpectedValues(validUnicode.substr(0, 1) + "\n", "?\\n", 12, escaper);
  validUnicode = "𐍈";
  verifyExpectedValues(validUnicode.substr(0, 1), "?", 6, escaper);
  verifyExpectedValues(validUnicode.substr(0, 1) + "\n", "?\\n", 12, escaper);
  verifyExpectedValues("\x07", "\\x07", 6, escaper);
  verifyExpectedValues(std::string("\0", 1), "\\x00", 6, escaper);
}
