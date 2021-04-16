////////////////////////////////////////////////////////////////////////////////
/// @brief Library to build up VPack documents.
///
/// DISCLAIMER
///
/// Copyright 2015 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
/// @author Jan Steemann
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include <ostream>
#include <string>

#include "tests-common.h"

namespace arangodb {
namespace velocypack {

extern void enableNativeStringFunctions();
extern void enableBuiltinStringFunctions();

}
}

TEST(ParserTest, CreateWithoutOptions) {
  ASSERT_VELOCYPACK_EXCEPTION(new Parser(nullptr), Exception::InternalError);
}

TEST(ParserTest, GarbageCatchVelocyPackException) {
  std::string const value("z");

  Parser parser;
  try {
    parser.parse(value);
    ASSERT_TRUE(false);
  } catch (Exception const& ex) {
    ASSERT_STREQ("Expecting digit", ex.what());
  }
}

TEST(ParserTest, GarbageCatchStdException) {
  std::string const value("z");

  Parser parser;
  try {
    parser.parse(value);
    ASSERT_TRUE(false);
  } catch (std::exception const& ex) {
    ASSERT_STREQ("Expecting digit", ex.what());
  }
}

TEST(ParserTest, Garbage1) {
  std::string const value("z");

  Parser parser;
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value), Exception::ParseError);
  ASSERT_EQ(0U, parser.errorPos());
}

TEST(ParserTest, Garbage2) {
  std::string const value("foo");

  Parser parser;
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value), Exception::ParseError);
  ASSERT_EQ(1U, parser.errorPos());
}

TEST(ParserTest, Garbage3) {
  std::string const value("truth");

  Parser parser;
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value), Exception::ParseError);
  ASSERT_EQ(3U, parser.errorPos());
}

TEST(ParserTest, Garbage4) {
  std::string const value("tru");

  Parser parser;
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value), Exception::ParseError);
  ASSERT_EQ(2U, parser.errorPos());
}

TEST(ParserTest, Garbage5) {
  std::string const value("truebar");

  Parser parser;
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value), Exception::ParseError);
  ASSERT_EQ(4U, parser.errorPos());
}

TEST(ParserTest, Garbage6) {
  std::string const value("fals");

  Parser parser;
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value), Exception::ParseError);
  ASSERT_EQ(3U, parser.errorPos());
}

TEST(ParserTest, Garbage7) {
  std::string const value("falselaber");

  Parser parser;
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value), Exception::ParseError);
  ASSERT_EQ(5U, parser.errorPos());
}

TEST(ParserTest, Garbage8) {
  std::string const value("zauberzauber");

  Parser parser;
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value), Exception::ParseError);
  ASSERT_EQ(0U, parser.errorPos());
}

TEST(ParserTest, Garbage9) {
  std::string const value("true,");

  Parser parser;
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value), Exception::ParseError);
  ASSERT_EQ(4U, parser.errorPos());
}

TEST(ParserTest, Punctuation1) {
  std::string const value(",");

  Parser parser;
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value), Exception::ParseError);
  ASSERT_EQ(0U, parser.errorPos());
}

TEST(ParserTest, Punctuation2) {
  std::string const value("/");

  Parser parser;
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value), Exception::ParseError);
  ASSERT_EQ(0U, parser.errorPos());
}

TEST(ParserTest, Punctuation3) {
  std::string const value("@");

  Parser parser;
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value), Exception::ParseError);
  ASSERT_EQ(0U, parser.errorPos());
}

TEST(ParserTest, Punctuation4) {
  std::string const value(":");

  Parser parser;
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value), Exception::ParseError);
  ASSERT_EQ(0U, parser.errorPos());
}

TEST(ParserTest, Punctuation5) {
  std::string const value("!");

  Parser parser;
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value), Exception::ParseError);
  ASSERT_EQ(0U, parser.errorPos());
}

TEST(ParserTest, NullInvalid) {
  std::string const value("nork");

  Parser parser;
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value), Exception::ParseError);
  ASSERT_EQ(1U, parser.errorPos());
}

TEST(ParserTest, Null) {
  std::string const value("null");

  Parser parser;
  ValueLength len = parser.parse(value);
  ASSERT_EQ(1ULL, len);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());
  checkBuild(s, ValueType::Null, 1ULL);

  checkDump(s, value);
}

TEST(ParserTest, FalseInvalid) {
  std::string const value("fork");

  Parser parser;
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value), Exception::ParseError);
  ASSERT_EQ(1U, parser.errorPos());
}

TEST(ParserTest, False) {
  std::string const value("false");

  Parser parser;
  ValueLength len = parser.parse(value);
  ASSERT_EQ(1ULL, len);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());
  checkBuild(s, ValueType::Bool, 1ULL);
  ASSERT_FALSE(s.getBool());

  checkDump(s, value);
}

TEST(ParserTest, TrueInvalid) {
  std::string const value("tork");

  Parser parser;
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value), Exception::ParseError);
  ASSERT_EQ(1U, parser.errorPos());
}

TEST(ParserTest, True) {
  std::string const value("true");

  Parser parser;
  ValueLength len = parser.parse(value);
  ASSERT_EQ(1ULL, len);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());
  checkBuild(s, ValueType::Bool, 1ULL);
  ASSERT_TRUE(s.getBool());

  checkDump(s, value);
}

TEST(ParserTest, Zero) {
  std::string const value("0");

  Parser parser;
  ValueLength len = parser.parse(value);
  ASSERT_EQ(1ULL, len);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());
  checkBuild(s, ValueType::SmallInt, 1ULL);
  ASSERT_EQ(0, s.getSmallInt());

  checkDump(s, value);
}

TEST(ParserTest, ZeroInvalid) {
  std::string const value("00");

  Parser parser;
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value), Exception::ParseError);
  ASSERT_EQ(1u, parser.errorPos());
}

TEST(ParserTest, NumberIncomplete) {
  std::string const value("-");

  Parser parser;
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value), Exception::ParseError);
  ASSERT_EQ(0u, parser.errorPos());
}

TEST(ParserTest, Int1) {
  std::string const value("1");

  Parser parser;
  ValueLength len = parser.parse(value);
  ASSERT_EQ(1ULL, len);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());
  checkBuild(s, ValueType::SmallInt, 1ULL);
  ASSERT_EQ(1, s.getSmallInt());

  checkDump(s, value);
}

TEST(ParserTest, IntM1) {
  std::string const value("-1");

  Parser parser;
  ValueLength len = parser.parse(value);
  ASSERT_EQ(1ULL, len);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());
  checkBuild(s, ValueType::SmallInt, 1ULL);
  ASSERT_EQ(-1LL, s.getSmallInt());

  checkDump(s, value);
}

TEST(ParserTest, Int2) {
  std::string const value("100000");

  Parser parser;
  ValueLength len = parser.parse(value);
  ASSERT_EQ(1ULL, len);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());
  checkBuild(s, ValueType::UInt, 4ULL);
  ASSERT_EQ(100000ULL, s.getUInt());

  checkDump(s, value);
}

TEST(ParserTest, Int3) {
  std::string const value("-100000");

  Parser parser;
  ValueLength len = parser.parse(value);
  ASSERT_EQ(1ULL, len);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());
  checkBuild(s, ValueType::Int, 4ULL);
  ASSERT_EQ(-100000LL, s.getInt());

  checkDump(s, value);
}

TEST(ParserTest, UIntMaxNeg) {
  std::string value("-");
  value.append(std::to_string(UINT64_MAX));

  Parser parser;
  ValueLength len = parser.parse(value);
  ASSERT_EQ(1ULL, len);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());
  checkBuild(s, ValueType::Double, 9ULL);
  // handle rounding errors
  ASSERT_DOUBLE_EQ(-18446744073709551615., s.getDouble());
}

TEST(ParserTest, IntMin) {
  std::string const value(std::to_string(INT64_MIN));

  Parser parser;
  ValueLength len = parser.parse(value);
  ASSERT_EQ(1ULL, len);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());
  checkBuild(s, ValueType::Int, 9ULL);
  ASSERT_EQ(INT64_MIN, s.getInt());

  checkDump(s, value);
}

TEST(ParserTest, IntMinMinusOne) {
  std::string const value("-9223372036854775809");  // INT64_MIN - 1

  Parser parser;
  ValueLength len = parser.parse(value);
  ASSERT_EQ(1ULL, len);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());
  checkBuild(s, ValueType::Double, 9ULL);
  ASSERT_DOUBLE_EQ(-9223372036854775809., s.getDouble());
}

TEST(ParserTest, IntMax) {
  std::string const value(std::to_string(INT64_MAX));

  Parser parser;
  ValueLength len = parser.parse(value);
  ASSERT_EQ(1ULL, len);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());
  checkBuild(s, ValueType::UInt, 9ULL);
  ASSERT_EQ(static_cast<uint64_t>(INT64_MAX), s.getUInt());

  checkDump(s, value);
}

TEST(ParserTest, IntMaxPlusOne) {
  std::string const value("9223372036854775808");  // INT64_MAX + 1

  Parser parser;
  ValueLength len = parser.parse(value);
  ASSERT_EQ(1ULL, len);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());
  checkBuild(s, ValueType::UInt, 9ULL);
  ASSERT_EQ(static_cast<uint64_t>(INT64_MAX) + 1, s.getUInt());

  checkDump(s, value);
}

TEST(ParserTest, UIntMax) {
  std::string const value(std::to_string(UINT64_MAX));

  Parser parser;
  ValueLength len = parser.parse(value);
  ASSERT_EQ(1ULL, len);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());
  checkBuild(s, ValueType::UInt, 9ULL);
  ASSERT_EQ(UINT64_MAX, s.getUInt());

  checkDump(s, value);
}

TEST(ParserTest, UIntMaxPlusOne) {
  std::string const value("18446744073709551616");  // UINT64_MAX + 1

  Parser parser;
  ValueLength len = parser.parse(value);
  ASSERT_EQ(1ULL, len);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());
  checkBuild(s, ValueType::Double, 9ULL);
  ASSERT_DOUBLE_EQ(18446744073709551616., s.getDouble());
}

TEST(ParserTest, Double1) {
  std::string const value("1.0124");

  Parser parser;
  ValueLength len = parser.parse(value);
  ASSERT_EQ(1ULL, len);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());
  checkBuild(s, ValueType::Double, 9ULL);
  ASSERT_EQ(1.0124, s.getDouble());

  checkDump(s, value);
}

TEST(ParserTest, Double2) {
  std::string const value("-1.0124");

  Parser parser;
  ValueLength len = parser.parse(value);
  ASSERT_EQ(1ULL, len);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());
  checkBuild(s, ValueType::Double, 9ULL);
  ASSERT_EQ(-1.0124, s.getDouble());

  checkDump(s, value);
}

TEST(ParserTest, DoubleScientificWithoutDot1) {
  std::string const value("-3e12");

  Parser parser;
  ValueLength len = parser.parse(value);
  ASSERT_EQ(1ULL, len);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());
  checkBuild(s, ValueType::Double, 9ULL);
  ASSERT_EQ(-3.e12, s.getDouble());

  std::string const valueOut("-3e+12");
  checkDump(s, valueOut);
}

TEST(ParserTest, DoubleScientificWithoutDot2) {
  std::string const value("3e12");

  Parser parser;
  ValueLength len = parser.parse(value);
  ASSERT_EQ(1ULL, len);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());
  checkBuild(s, ValueType::Double, 9ULL);
  ASSERT_EQ(3e12, s.getDouble());

  std::string const valueOut("3e+12");
  checkDump(s, valueOut);
}

TEST(ParserTest, DoubleScientific1) {
  std::string const value("-1.0124e42");

  Parser parser;
  ValueLength len = parser.parse(value);
  ASSERT_EQ(1ULL, len);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());
  checkBuild(s, ValueType::Double, 9ULL);
  ASSERT_EQ(-1.0124e42, s.getDouble());

  std::string const valueOut("-1.0124e+42");
  checkDump(s, valueOut);
}

TEST(ParserTest, DoubleScientific2) {
  std::string const value("-1.0124e+42");

  Parser parser;
  ValueLength len = parser.parse(value);
  ASSERT_EQ(1ULL, len);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());
  checkBuild(s, ValueType::Double, 9ULL);
  ASSERT_EQ(-1.0124e42, s.getDouble());

  checkDump(s, value);
}

TEST(ParserTest, DoubleScientific3) {
  std::string const value("3122243.0124e-42");

  Parser parser;
  ValueLength len = parser.parse(value);
  ASSERT_EQ(1ULL, len);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());
  checkBuild(s, ValueType::Double, 9ULL);
  ASSERT_EQ(3122243.0124e-42, s.getDouble());

  std::string const valueOut("3.1222430124e-36");
  checkDump(s, valueOut);
}

TEST(ParserTest, DoubleScientific4) {
  std::string const value("2335431.0124E-42");

  Parser parser;
  ValueLength len = parser.parse(value);
  ASSERT_EQ(1ULL, len);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());
  checkBuild(s, ValueType::Double, 9ULL);
  ASSERT_EQ(2335431.0124E-42, s.getDouble());

  std::string const valueOut("2.3354310124e-36");
  checkDump(s, valueOut);
}

TEST(ParserTest, DoubleScientific5) {
  std::string const value("3122243.0124e+42");

  Parser parser;
  ValueLength len = parser.parse(value);
  ASSERT_EQ(1ULL, len);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());
  checkBuild(s, ValueType::Double, 9ULL);
  ASSERT_EQ(3122243.0124e+42, s.getDouble());

  std::string const valueOut("3.1222430124e+48");
  checkDump(s, valueOut);
}

TEST(ParserTest, DoubleNeg) {
  std::string const value("-184467440737095516161");

  Parser parser;
  ValueLength len = parser.parse(value);
  ASSERT_EQ(1ULL, len);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());
  checkBuild(s, ValueType::Double, 9ULL);
  ASSERT_DOUBLE_EQ(-184467440737095516161., s.getDouble());
}

TEST(ParserTest, DoublePrecision1) {
  std::string const value("0.3");

  Parser parser;
  ValueLength len = parser.parse(value);
  ASSERT_EQ(1ULL, len);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());
  checkBuild(s, ValueType::Double, 9ULL);
  ASSERT_DOUBLE_EQ(0.3, s.getDouble());
}

TEST(ParserTest, DoublePrecision2) {
  std::string const value("0.33");

  Parser parser;
  ValueLength len = parser.parse(value);
  ASSERT_EQ(1ULL, len);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());
  checkBuild(s, ValueType::Double, 9ULL);
  ASSERT_DOUBLE_EQ(0.33, s.getDouble());
}

TEST(ParserTest, DoublePrecision3) {
  std::string const value("0.67");

  Parser parser;
  ValueLength len = parser.parse(value);
  ASSERT_EQ(1ULL, len);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());
  checkBuild(s, ValueType::Double, 9ULL);
  ASSERT_DOUBLE_EQ(0.67, s.getDouble());
}

TEST(ParserTest, DoubleBroken1) {
  std::string const value("1234.");

  ASSERT_VELOCYPACK_EXCEPTION(Parser::fromJson(value), Exception::ParseError);
}

TEST(ParserTest, DoubleBroken2) {
  std::string const value("1234.a");

  ASSERT_VELOCYPACK_EXCEPTION(Parser::fromJson(value), Exception::ParseError);
}

TEST(ParserTest, DoubleBrokenExponent) {
  std::string const value("1234.33e");

  ASSERT_VELOCYPACK_EXCEPTION(Parser::fromJson(value), Exception::ParseError);
}

TEST(ParserTest, DoubleBrokenExponent2) {
  std::string const value("1234.33e-");

  ASSERT_VELOCYPACK_EXCEPTION(Parser::fromJson(value), Exception::ParseError);
}

TEST(ParserTest, DoubleBrokenExponent3) {
  std::string const value("1234.33e+");

  ASSERT_VELOCYPACK_EXCEPTION(Parser::fromJson(value), Exception::ParseError);
}

TEST(ParserTest, DoubleBrokenExponent4) {
  std::string const value("1234.33ea");

  ASSERT_VELOCYPACK_EXCEPTION(Parser::fromJson(value), Exception::ParseError);
}

TEST(ParserTest, DoubleBrokenExponent5) {
  std::string const value(
      "1e22222222222222222222222222222222222222222222222222222222222222");

  ASSERT_VELOCYPACK_EXCEPTION(Parser::fromJson(value),
                              Exception::NumberOutOfRange);
}

TEST(ParserTest, IntMinusInf) {
  std::string const value(
      "-99999999999999999999999999999999999999999999999999999999999999999999999"
      "999999999999999999999999999999999999999999999999999999999999999999999999"
      "999999999999999999999999999999999999999999999999999999999999999999999999"
      "999999999999999999999999999999999999999999999999999999999999999999999999"
      "999999999999999999999999999999999999999999999999999999999999999999999999"
      "999999999999999999999999999999999999999999999999999999999999999999999999"
      "999999999999999999999999999999999999999999999999999999999999999999999999"
      "999999999999999999999999999999999999999999999999999999999999999999999999"
      "999999999999999999999999999999999999999999999999999999999999999999999999"
      "999999999999999999999999999999999999999999999999999999999999999999999999"
      "999999999999999999999999999999999999999999999999999999999999999999999999"
      "999999999999999999999999999999999999999999999999999999999999999999999999"
      "999999999999999999999999999999999999999999999999999999999999999999999999"
      "9999999999999999999999999999999999999999999999999999999999999999");

  Parser parser;
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value), Exception::NumberOutOfRange);
}

TEST(ParserTest, IntPlusInf) {
  std::string const value(
      "999999999999999999999999999999999999999999999999999999999999999999999999"
      "999999999999999999999999999999999999999999999999999999999999999999999999"
      "999999999999999999999999999999999999999999999999999999999999999999999999"
      "999999999999999999999999999999999999999999999999999999999999999999999999"
      "999999999999999999999999999999999999999999999999999999999999999999999999"
      "999999999999999999999999999999999999999999999999999999999999999999999999"
      "999999999999999999999999999999999999999999999999999999999999999999999999"
      "999999999999999999999999999999999999999999999999999999999999999999999999"
      "999999999999999999999999999999999999999999999999999999999999999999999999"
      "999999999999999999999999999999999999999999999999999999999999999999999999"
      "999999999999999999999999999999999999999999999999999999999999999999999999"
      "999999999999999999999999999999999999999999999999999999999999999999999999"
      "999999999999999999999999999999999999999999999999999999999999999999999999"
      "999999999999999999999999999999999999999999999999999999999999999");

  Parser parser;
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value), Exception::NumberOutOfRange);
}

TEST(ParserTest, DoubleMinusInf) {
  std::string const value("-1.2345e999");

  Parser parser;
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value), Exception::NumberOutOfRange);
}

TEST(ParserTest, DoublePlusInf) {
  std::string const value("1.2345e999");

  Parser parser;
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value), Exception::NumberOutOfRange);
}

TEST(ParserTest, Empty) {
  std::string const value("");

  Parser parser;
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value), Exception::ParseError);
  ASSERT_EQ(0U, parser.errorPos());
}

TEST(ParserTest, WhitespaceOnly) {
  std::string const value("  ");

  Parser parser;
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value), Exception::ParseError);
  ASSERT_EQ(1U, parser.errorPos());
}

TEST(ParserTest, LongerString1) {
  std::string const value("\"01234567890123456789012345678901\"");
  ASSERT_EQ(0U, (value.size() - 2) %
                    16);  // string payload should be a multiple of 16

  Options options;
  options.validateUtf8Strings = true;

  Parser parser(&options);
  parser.parse(value);
  Slice s(parser.builder().slice());

  std::string parsed = s.copyString();
  ASSERT_EQ(value.substr(1, value.size() - 2), parsed);
}

TEST(ParserTest, LongerString2) {
  std::string const value("\"this is a long string (longer than 16 bytes)\"");

  Parser parser;
  parser.parse(value);
  Slice s(parser.builder().slice());

  std::string parsed = s.copyString();
  ASSERT_EQ(value.substr(1, value.size() - 2), parsed);
}

TEST(ParserTest, UnterminatedStringLiteral) {
  std::string const value("\"der hund");

  Parser parser;
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value), Exception::ParseError);
  ASSERT_EQ(8U, parser.errorPos());
}

TEST(ParserTest, UnterminatedEscapeSequence) {
  std::string const value("\"der hund\\");

  Parser parser;
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value), Exception::ParseError);
  ASSERT_EQ(9U, parser.errorPos());
}

TEST(ParserTest, UnterminatedEscapeUnicodeSequence1) {
  std::string const value("\"\\u\"");

  Parser parser;
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value), Exception::ParseError);
  ASSERT_EQ(3U, parser.errorPos());
}

TEST(ParserTest, UnterminatedEscapeUnicodeSequence2) {
  std::string const value("\"\\u0\"");

  Parser parser;
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value), Exception::ParseError);
  ASSERT_EQ(4U, parser.errorPos());
}

TEST(ParserTest, UnterminatedEscapeUnicodeSequence3) {
  std::string const value("\"\\u01\"");

  Parser parser;
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value), Exception::ParseError);
  ASSERT_EQ(5U, parser.errorPos());
}

TEST(ParserTest, UnterminatedEscapeUnicodeSequence4) {
  std::string const value("\"\\u012\"");

  Parser parser;
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value), Exception::ParseError);
  ASSERT_EQ(6U, parser.errorPos());
}

TEST(ParserTest, UnterminatedEscapeUnicodeSequence5) {
  std::string const value("\"\\u");

  Parser parser;
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value), Exception::ParseError);
  ASSERT_EQ(2U, parser.errorPos());
}

TEST(ParserTest, UnterminatedEscapeUnicodeSequence6) {
  std::string const value("\"\\u1");

  Parser parser;
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value), Exception::ParseError);
  ASSERT_EQ(3U, parser.errorPos());
}

TEST(ParserTest, UnterminatedEscapeUnicodeSequence7) {
  std::string const value("\"\\u12");

  Parser parser;
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value), Exception::ParseError);
  ASSERT_EQ(4U, parser.errorPos());
}

TEST(ParserTest, UnterminatedEscapeUnicodeSequence8) {
  std::string const value("\"\\u123");

  Parser parser;
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value), Exception::ParseError);
  ASSERT_EQ(5U, parser.errorPos());
}

TEST(ParserTest, UnterminatedEscapeUnicodeSequence9) {
  std::string const value("\"\\u1234");

  Parser parser;
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value), Exception::ParseError);
  ASSERT_EQ(6U, parser.errorPos());
}

TEST(ParserTest, InvalidEscapeUnicodeSequence1) {
  std::string const value("\"\\uz\"");

  Parser parser;
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value), Exception::ParseError);
  ASSERT_EQ(3U, parser.errorPos());
}

TEST(ParserTest, InvalidEscapeUnicodeSequence2) {
  std::string const value("\"\\uzzzz\"");

  Parser parser;
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value), Exception::ParseError);
  ASSERT_EQ(3U, parser.errorPos());
}

TEST(ParserTest, InvalidEscapeUnicodeSequence3) {
  std::string const value("\"\\U----\"");

  Parser parser;
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value), Exception::ParseError);
  ASSERT_EQ(2U, parser.errorPos());
}

TEST(ParserTest, InvalidEscapeSequence1) {
  std::string const value("\"\\x\"");

  Parser parser;
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value), Exception::ParseError);
  ASSERT_EQ(2U, parser.errorPos());
}

TEST(ParserTest, InvalidEscapeSequence2) {
  std::string const value("\"\\z\"");

  Parser parser;
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value), Exception::ParseError);
  ASSERT_EQ(2U, parser.errorPos());
}

TEST(ParserTest, StringLiteral) {
  std::string const value("\"der hund ging in den wald und aß den fuxx\"");

  Parser parser;
  ValueLength len = parser.parse(value);
  ASSERT_EQ(1ULL, len);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());
  std::string const correct = "der hund ging in den wald und aß den fuxx";
  checkBuild(s, ValueType::String, 1 + correct.size());
  char const* p = s.getString(len);
  ASSERT_EQ(correct.size(), len);
  ASSERT_EQ(0, strncmp(correct.c_str(), p, len));
  std::string out = s.copyString();
  ASSERT_EQ(correct, out);

  std::string valueOut = "\"der hund ging in den wald und aß den fuxx\"";
  checkDump(s, valueOut);
}

TEST(ParserTest, StringLiteralEmpty) {
  std::string const value("\"\"");

  Parser parser;
  ValueLength len = parser.parse(value);
  ASSERT_EQ(1ULL, len);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());
  checkBuild(s, ValueType::String, 1ULL);
  char const* p = s.getString(len);
  ASSERT_EQ(0, strncmp("", p, len));
  ASSERT_EQ(0ULL, len);
  std::string out = s.copyString();
  std::string empty;
  ASSERT_EQ(empty, out);

  checkDump(s, value);
}

TEST(ParserTest, StringTwoByteUTF8) {
  Options options;
  options.validateUtf8Strings = true;

  std::string const value("\"\xc2\xa2\"");

  Parser parser(&options);
  parser.parse(value);

  std::shared_ptr<Builder> b = parser.steal();
  Slice s(b->slice());
  ASSERT_EQ(value, s.toJson());
}

TEST(ParserTest, StringThreeByteUTF8) {
  Options options;
  options.validateUtf8Strings = true;

  std::string const value("\"\xe2\x82\xac\"");

  Parser parser(&options);
  parser.parse(value);

  std::shared_ptr<Builder> b = parser.steal();
  Slice s(b->slice());
  ASSERT_EQ(value, s.toJson());
}

TEST(ParserTest, StringFourByteUTF8) {
  Options options;
  options.validateUtf8Strings = true;

  std::string const value("\"\xf0\xa4\xad\xa2\"");

  Parser parser(&options);
  parser.parse(value);

  std::shared_ptr<Builder> b = parser.steal();
  Slice s(b->slice());
  ASSERT_EQ(value, s.toJson());
}

TEST(ParserTest, StringLiteralInvalidUtfValue1) {
  Options options;
  options.validateUtf8Strings = true;

  std::string value;
  value.push_back('"');
  value.push_back(static_cast<unsigned char>(0x80));
  value.push_back('"');

  Parser parser(&options);
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value),
                              Exception::InvalidUtf8Sequence);
  ASSERT_EQ(1U, parser.errorPos());

  options.validateUtf8Strings = false;
  ASSERT_EQ(1ULL, parser.parse(value));
}

TEST(ParserTest, StringLiteralInvalidUtfValue2) {
  Options options;
  options.validateUtf8Strings = true;

  std::string value;
  value.push_back('"');
  value.push_back(static_cast<unsigned char>(0xff));
  value.push_back(static_cast<unsigned char>(0xff));
  value.push_back('"');

  Parser parser(&options);
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value),
                              Exception::InvalidUtf8Sequence);
  ASSERT_EQ(1U, parser.errorPos());
  options.validateUtf8Strings = false;
  ASSERT_EQ(1ULL, parser.parse(value));
}

TEST(ParserTest, StringLiteralInvalidUtfValueLongString) {
  Options options;
  options.validateUtf8Strings = true;

  std::string value;
  value.push_back('"');
  for (std::size_t i = 0; i < 100; ++i) {
    value.push_back(static_cast<unsigned char>(0x80));
  }
  value.push_back('"');

  Parser parser(&options);
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value),
                              Exception::InvalidUtf8Sequence);
  ASSERT_EQ(1U, parser.errorPos());
  options.validateUtf8Strings = false;
  ASSERT_EQ(1ULL, parser.parse(value));
}

TEST(ParserTest, StringLiteralControlCharacter) {
  for (char c = 0; c < 0x20; c++) {
    std::string value;
    value.push_back('"');
    value.push_back(c);
    value.push_back('"');

    Parser parser;
    ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value),
                                Exception::UnexpectedControlCharacter);
    ASSERT_EQ(1U, parser.errorPos());
  }
}

TEST(ParserTest, StringLiteralUnfinishedUtfSequence1) {
  std::string const value("\"\\u\"");

  Parser parser;
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value), Exception::ParseError);
  ASSERT_EQ(3U, parser.errorPos());
}

TEST(ParserTest, StringLiteralUnfinishedUtfSequence2) {
  std::string const value("\"\\u0\"");

  Parser parser;
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value), Exception::ParseError);
  ASSERT_EQ(4U, parser.errorPos());
}

TEST(ParserTest, StringLiteralUnfinishedUtfSequence3) {
  std::string const value("\"\\u01\"");

  Parser parser;
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value), Exception::ParseError);
  ASSERT_EQ(5U, parser.errorPos());
}

TEST(ParserTest, StringLiteralUnfinishedUtfSequence4) {
  std::string const value("\"\\u012\"");

  Parser parser;
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value), Exception::ParseError);
  ASSERT_EQ(6U, parser.errorPos());
}

TEST(ParserTest, StringLiteralUtf8SequenceLowerCase) {
  std::string const value("\"der m\\u00d6ter\"");

  Parser parser;
  ValueLength len = parser.parse(value);
  ASSERT_EQ(1ULL, len);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());
  checkBuild(s, ValueType::String, 11ULL);
  char const* p = s.getString(len);
  ASSERT_EQ(10ULL, len);
  std::string correct = "der m\xc3\x96ter";
  ASSERT_EQ(0, strncmp(correct.c_str(), p, len));
  std::string out = s.copyString();
  ASSERT_EQ(correct, out);

  std::string const valueOut("\"der mÖter\"");
  checkDump(s, valueOut);
}

TEST(ParserTest, StringLiteralUtf8SequenceUpperCase) {
  std::string const value("\"der m\\u00D6ter\"");

  Parser parser;
  ValueLength len = parser.parse(value);
  ASSERT_EQ(1ULL, len);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());
  std::string correct = "der mÖter";
  checkBuild(s, ValueType::String, 1 + correct.size());
  char const* p = s.getString(len);
  ASSERT_EQ(correct.size(), len);
  ASSERT_EQ(0, strncmp(correct.c_str(), p, len));
  std::string out = s.copyString();
  ASSERT_EQ(correct, out);

  checkDump(s, std::string("\"der mÖter\""));
}

TEST(ParserTest, StringLiteralUtf8Chars) {
  std::string const value("\"der mötör klötörte mät dän fößen\"");

  Parser parser;
  ValueLength len = parser.parse(value);
  ASSERT_EQ(1ULL, len);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());
  std::string correct = "der mötör klötörte mät dän fößen";
  checkBuild(s, ValueType::String, 1 + correct.size());
  char const* p = s.getString(len);
  ASSERT_EQ(correct.size(), len);
  ASSERT_EQ(0, strncmp(correct.c_str(), p, len));
  std::string out = s.copyString();
  ASSERT_EQ(correct, out);

  checkDump(s, value);
}

TEST(ParserTest, StringLiteralWithSpecials) {
  std::string const value(
      "  \"der\\thund\\nging\\rin\\fden\\\\wald\\\"und\\b\\nden'fux\"  ");

  Parser parser;
  ValueLength len = parser.parse(value);
  ASSERT_EQ(1ULL, len);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());
  std::string correct = "der\thund\nging\rin\fden\\wald\"und\b\nden'fux";
  checkBuild(s, ValueType::String, 1 + correct.size());
  char const* p = s.getString(len);
  ASSERT_EQ(correct.size(), len);
  ASSERT_EQ(0, strncmp(correct.c_str(), p, len));
  std::string out = s.copyString();
  ASSERT_EQ(correct, out);

  std::string const valueOut(
      "\"der\\thund\\nging\\rin\\fden\\\\wald\\\"und\\b\\nden'fux\"");
  checkDump(s, valueOut);
}

TEST(ParserTest, StringLiteralWithSurrogatePairs) {
  std::string const value("\"\\ud800\\udc00\\udbff\\udfff\\udbc8\\udf45\"");

  Parser parser;
  ValueLength len = parser.parse(value);
  ASSERT_EQ(1ULL, len);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());
  std::string correct = "\xf0\x90\x80\x80\xf4\x8f\xbf\xbf\xf4\x82\x8d\x85";
  checkBuild(s, ValueType::String, 1 + correct.size());
  char const* p = s.getString(len);
  ASSERT_EQ(correct.size(), len);
  ASSERT_EQ(0, strncmp(correct.c_str(), p, len));
  std::string out = s.copyString();
  ASSERT_EQ(correct, out);

  std::string const valueOut(
      "\"\xf0\x90\x80\x80\xf4\x8f\xbf\xbf\xf4\x82\x8d\x85\"");
  checkDump(s, valueOut);
}

TEST(ParserTest, EmptyArray) {
  std::string const value("[]");

  Parser parser;
  ValueLength len = parser.parse(value);
  ASSERT_EQ(1ULL, len);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());
  checkBuild(s, ValueType::Array, 1);
  ASSERT_EQ(0ULL, s.length());

  checkDump(s, value);
}

TEST(ParserTest, WhitespacedArray) {
  std::string const value("  [    ]   ");

  Parser parser;
  ValueLength len = parser.parse(value);
  ASSERT_EQ(1ULL, len);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());
  checkBuild(s, ValueType::Array, 1);
  ASSERT_EQ(0ULL, s.length());

  std::string const valueOut = "[]";
  checkDump(s, valueOut);
}

TEST(ParserTest, Array1) {
  std::string const value("[1]");

  Parser parser;
  ValueLength len = parser.parse(value);
  ASSERT_EQ(1ULL, len);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());
  checkBuild(s, ValueType::Array, 3);
  ASSERT_EQ(1ULL, s.length());
  Slice ss = s[0];
  checkBuild(ss, ValueType::SmallInt, 1);
  ASSERT_EQ(1ULL, ss.getUInt());

  checkDump(s, value);
}

TEST(ParserTest, Array2) {
  std::string const value("[1,2]");

  Parser parser;
  ValueLength len = parser.parse(value);
  ASSERT_EQ(1ULL, len);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());
  checkBuild(s, ValueType::Array, 4);
  ASSERT_EQ(2ULL, s.length());
  Slice ss = s[0];
  checkBuild(ss, ValueType::SmallInt, 1);
  ASSERT_EQ(1ULL, ss.getUInt());
  ss = s[1];
  checkBuild(ss, ValueType::SmallInt, 1);
  ASSERT_EQ(2ULL, ss.getUInt());

  checkDump(s, value);
}

TEST(ParserTest, Array3) {
  std::string const value("[-1,2, 4.5, 3, -99.99]");
  Parser parser;
  ValueLength len = parser.parse(value);
  ASSERT_EQ(1ULL, len);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());
  checkBuild(s, ValueType::Array, 29);
  ASSERT_EQ(5ULL, s.length());

  Slice ss = s[0];
  checkBuild(ss, ValueType::SmallInt, 1);
  ASSERT_EQ(-1LL, ss.getInt());

  ss = s[1];
  checkBuild(ss, ValueType::SmallInt, 1);
  ASSERT_EQ(2ULL, ss.getUInt());

  ss = s[2];
  checkBuild(ss, ValueType::Double, 9);
  ASSERT_EQ(4.5, ss.getDouble());

  ss = s[3];
  checkBuild(ss, ValueType::SmallInt, 1);
  ASSERT_EQ(3ULL, ss.getUInt());

  ss = s[4];
  checkBuild(ss, ValueType::Double, 9);
  ASSERT_EQ(-99.99, ss.getDouble());

  std::string const valueOut = "[-1,2,4.5,3,-99.99]";
  checkDump(s, valueOut);
}

TEST(ParserTest, Array4) {
  std::string const value(
      "[\"foo\", \"bar\", \"baz\", null, true, false, -42.23 ]");

  Parser parser;
  ValueLength len = parser.parse(value);
  ASSERT_EQ(1ULL, len);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());
  checkBuild(s, ValueType::Array, 34);
  ASSERT_EQ(7ULL, s.length());

  Slice ss = s[0];
  checkBuild(ss, ValueType::String, 4);
  std::string correct = "foo";
  ASSERT_EQ(correct, ss.copyString());

  ss = s[1];
  checkBuild(ss, ValueType::String, 4);
  correct = "bar";
  ASSERT_EQ(correct, ss.copyString());

  ss = s[2];
  checkBuild(ss, ValueType::String, 4);
  correct = "baz";
  ASSERT_EQ(correct, ss.copyString());

  ss = s[3];
  checkBuild(ss, ValueType::Null, 1);

  ss = s[4];
  checkBuild(ss, ValueType::Bool, 1);
  ASSERT_TRUE(ss.getBool());

  ss = s[5];
  checkBuild(ss, ValueType::Bool, 1);
  ASSERT_FALSE(ss.getBool());

  ss = s[6];
  checkBuild(ss, ValueType::Double, 9);
  ASSERT_EQ(-42.23, ss.getDouble());

  std::string const valueOut =
      "[\"foo\",\"bar\",\"baz\",null,true,false,-42.23]";
  checkDump(s, valueOut);
}

TEST(ParserTest, NestedArray1) {
  std::string const value("[ [ ] ]");

  Parser parser;
  ValueLength len = parser.parse(value);
  ASSERT_EQ(1ULL, len);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());
  checkBuild(s, ValueType::Array, 3);
  ASSERT_EQ(1ULL, s.length());

  Slice ss = s[0];
  checkBuild(ss, ValueType::Array, 1);
  ASSERT_EQ(0ULL, ss.length());

  std::string const valueOut = "[[]]";
  checkDump(s, valueOut);
}

TEST(ParserTest, NestedArray2) {
  std::string const value("[ [ ],[[]],[],[ [[ [], [ ], [ ] ], [ ] ] ], [] ]");
  Parser parser;
  ValueLength len = parser.parse(value);
  ASSERT_EQ(1ULL, len);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());
  checkBuild(s, ValueType::Array, 27);
  ASSERT_EQ(5ULL, s.length());

  Slice ss = s[0];
  checkBuild(ss, ValueType::Array, 1);
  ASSERT_EQ(0ULL, ss.length());

  ss = s[1];
  checkBuild(ss, ValueType::Array, 3);
  ASSERT_EQ(1ULL, ss.length());

  Slice sss = ss[0];
  checkBuild(sss, ValueType::Array, 1);
  ASSERT_EQ(0ULL, sss.length());

  ss = s[2];
  checkBuild(ss, ValueType::Array, 1);
  ASSERT_EQ(0ULL, ss.length());

  ss = s[3];
  checkBuild(ss, ValueType::Array, 13);
  ASSERT_EQ(1ULL, ss.length());

  sss = ss[0];
  checkBuild(sss, ValueType::Array, 11);
  ASSERT_EQ(2ULL, sss.length());

  Slice ssss = sss[0];
  checkBuild(ssss, ValueType::Array, 5);
  ASSERT_EQ(3ULL, ssss.length());

  Slice sssss = ssss[0];
  checkBuild(sssss, ValueType::Array, 1);
  ASSERT_EQ(0ULL, sssss.length());

  sssss = ssss[1];
  checkBuild(sssss, ValueType::Array, 1);
  ASSERT_EQ(0ULL, sssss.length());

  sssss = ssss[2];
  checkBuild(sssss, ValueType::Array, 1);
  ASSERT_EQ(0ULL, sssss.length());

  ssss = sss[1];
  checkBuild(ssss, ValueType::Array, 1);
  ASSERT_EQ(0ULL, ssss.length());

  ss = s[4];
  checkBuild(ss, ValueType::Array, 1);
  ASSERT_EQ(0ULL, ss.length());

  std::string const valueOut = "[[],[[]],[],[[[[],[],[]],[]]],[]]";
  checkDump(s, valueOut);
}

TEST(ParserTest, NestedArray3) {
  std::string const value(
      "[ [ \"foo\", [ \"bar\", \"baz\", null ], true, false ], -42.23 ]");

  Parser parser;
  ValueLength len = parser.parse(value);
  ASSERT_EQ(1ULL, len);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());
  checkBuild(s, ValueType::Array, 42);
  ASSERT_EQ(2ULL, s.length());

  Slice ss = s[0];
  checkBuild(ss, ValueType::Array, 28);
  ASSERT_EQ(4ULL, ss.length());

  Slice sss = ss[0];
  checkBuild(sss, ValueType::String, 4);
  std::string correct = "foo";
  ASSERT_EQ(correct, sss.copyString());

  sss = ss[1];
  checkBuild(sss, ValueType::Array, 15);
  ASSERT_EQ(3ULL, sss.length());

  Slice ssss = sss[0];
  checkBuild(ssss, ValueType::String, 4);
  correct = "bar";
  ASSERT_EQ(correct, ssss.copyString());

  ssss = sss[1];
  checkBuild(ssss, ValueType::String, 4);
  correct = "baz";
  ASSERT_EQ(correct, ssss.copyString());

  ssss = sss[2];
  checkBuild(ssss, ValueType::Null, 1);

  sss = ss[2];
  checkBuild(sss, ValueType::Bool, 1);
  ASSERT_TRUE(sss.getBool());

  sss = ss[3];
  checkBuild(sss, ValueType::Bool, 1);
  ASSERT_FALSE(sss.getBool());

  ss = s[1];
  checkBuild(ss, ValueType::Double, 9);
  ASSERT_EQ(-42.23, ss.getDouble());

  std::string const valueOut =
      "[[\"foo\",[\"bar\",\"baz\",null],true,false],-42.23]";
  checkDump(s, valueOut);
}

TEST(ParserTest, NestedArrayInvalid1) {
  std::string const value("[ [ ]");

  Parser parser;
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value), Exception::ParseError);
  ASSERT_EQ(4U, parser.errorPos());
}

TEST(ParserTest, NestedArrayInvalid2) {
  std::string const value("[ ] ]");

  Parser parser;
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value), Exception::ParseError);
  ASSERT_EQ(4U, parser.errorPos());
}

TEST(ParserTest, NestedArrayInvalid3) {
  std::string const value("[ [ \"foo\", [ \"bar\", \"baz\", null ] ]");

  Parser parser;
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value), Exception::ParseError);
  ASSERT_EQ(34U, parser.errorPos());
}

TEST(ParserTest, BrokenArray1) {
  std::string const value("[");

  Parser parser;
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value), Exception::ParseError);
  ASSERT_EQ(0U, parser.errorPos());
}

TEST(ParserTest, BrokenArray2) {
  std::string const value("[,");

  Parser parser;
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value), Exception::ParseError);
  ASSERT_EQ(1U, parser.errorPos());
}

TEST(ParserTest, BrokenArray3) {
  std::string const value("[1,");

  Parser parser;
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value), Exception::ParseError);
  ASSERT_EQ(2U, parser.errorPos());
}

TEST(ParserTest, ShortArrayMembers) {
  std::string value("[");
  for (std::size_t i = 0; i < 255; ++i) {
    if (i > 0) {
      value.push_back(',');
    }
    value.append(std::to_string(i));
  }
  value.push_back(']');

  Parser parser;
  ValueLength len = parser.parse(value);
  ASSERT_EQ(1ULL, len);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());
  ASSERT_EQ(7ULL, s.head());
  checkBuild(s, ValueType::Array, 1019);
  ASSERT_EQ(255ULL, s.length());

  for (std::size_t i = 0; i < 255; ++i) {
    Slice ss = s[i];
    if (i <= 9) {
      checkBuild(ss, ValueType::SmallInt, 1);
    } else {
      checkBuild(ss, ValueType::UInt, 2);
    }
    ASSERT_EQ(i, ss.getUInt());
  }
}

TEST(ParserTest, LongArrayFewMembers) {
  std::string single("0123456789abcdef");
  single.append(single);
  single.append(single);
  single.append(single);
  single.append(single);
  single.append(single);
  single.append(single);  // 1024 bytes

  std::string value("[");
  for (std::size_t i = 0; i < 65; ++i) {
    if (i > 0) {
      value.push_back(',');
    }
    value.push_back('"');
    value.append(single);
    value.push_back('"');
  }
  value.push_back(']');

  Parser parser;
  ValueLength len = parser.parse(value);
  ASSERT_EQ(1ULL, len);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());
  ASSERT_EQ(4ULL, s.head());
  checkBuild(s, ValueType::Array, 67154);
  ASSERT_EQ(65ULL, s.length());

  for (std::size_t i = 0; i < 65; ++i) {
    Slice ss = s[i];
    checkBuild(ss, ValueType::String, 1033);
    ValueLength len;
    char const* s = ss.getString(len);
    ASSERT_EQ(1024ULL, len);
    ASSERT_EQ(0, strncmp(s, single.c_str(), len));
  }
}

TEST(ParserTest, LongArrayManyMembers) {
  std::string value("[");
  for (std::size_t i = 0; i < 256; ++i) {
    if (i > 0) {
      value.push_back(',');
    }
    value.append(std::to_string(i));
  }
  value.push_back(']');

  Parser parser;
  ValueLength len = parser.parse(value);
  ASSERT_EQ(1ULL, len);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());
  ASSERT_EQ(7ULL, s.head());
  checkBuild(s, ValueType::Array, 1023);
  ASSERT_EQ(256ULL, s.length());

  for (std::size_t i = 0; i < 256; ++i) {
    Slice ss = s[i];
    if (i <= 9) {
      checkBuild(ss, ValueType::SmallInt, 1);
    } else {
      checkBuild(ss, ValueType::UInt, 2);
    }
    ASSERT_EQ(i, ss.getUInt());
  }
}

TEST(ParserTest, EmptyObject) {
  std::string const value("{}");

  Parser parser;
  ValueLength len = parser.parse(value);
  ASSERT_EQ(1ULL, len);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());
  checkBuild(s, ValueType::Object, 1);
  ASSERT_EQ(0ULL, s.length());

  checkDump(s, value);
}

TEST(ParserTest, BrokenObject1) {
  std::string const value("{");

  Parser parser;
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value), Exception::ParseError);
  ASSERT_EQ(0U, parser.errorPos());
}

TEST(ParserTest, BrokenObject2) {
  std::string const value("{,");

  Parser parser;
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value), Exception::ParseError);
  ASSERT_EQ(0U, parser.errorPos());
}

TEST(ParserTest, BrokenObject3) {
  std::string const value("{1,");

  Parser parser;
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value), Exception::ParseError);
  ASSERT_EQ(0U, parser.errorPos());
}

TEST(ParserTest, BrokenObject4) {
  std::string const value("{\"foo");

  Parser parser;
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value), Exception::ParseError);
  ASSERT_EQ(4U, parser.errorPos());
}

TEST(ParserTest, BrokenObject5) {
  std::string const value("{\"foo\"");

  Parser parser;
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value), Exception::ParseError);
  ASSERT_EQ(5U, parser.errorPos());
}

TEST(ParserTest, BrokenObject6) {
  std::string const value("{\"foo\":");

  Parser parser;
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value), Exception::ParseError);
  ASSERT_EQ(6U, parser.errorPos());
}

TEST(ParserTest, BrokenObject7) {
  std::string const value("{\"foo\":\"foo");

  Parser parser;
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value), Exception::ParseError);
  ASSERT_EQ(10U, parser.errorPos());
}

TEST(ParserTest, BrokenObject8) {
  std::string const value("{\"foo\":\"foo\", ");

  Parser parser;
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value), Exception::ParseError);
  ASSERT_EQ(13U, parser.errorPos());
}

TEST(ParserTest, BrokenObject9) {
  std::string const value("{\"foo\":\"foo\", }");

  Parser parser;
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value), Exception::ParseError);
  ASSERT_EQ(13U, parser.errorPos());
}

TEST(ParserTest, BrokenObject10) {
  std::string const value("{\"foo\" }");

  Parser parser;
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value), Exception::ParseError);
  ASSERT_EQ(6U, parser.errorPos());
}

TEST(ParserTest, BrokenObject11) {
  std::string const value("{\"foo\" : [");

  Parser parser;
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value), Exception::ParseError);
  ASSERT_EQ(9U, parser.errorPos());
}

TEST(ParserTest, ObjectSimple1) {
  std::string const value("{ \"foo\" : 1}");

  Parser parser;
  ValueLength len = parser.parse(value);
  ASSERT_EQ(1ULL, len);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());
  checkBuild(s, ValueType::Object, 8);
  ASSERT_EQ(1ULL, s.length());

  Slice ss = s.keyAt(0);
  checkBuild(ss, ValueType::String, 4);

  std::string correct = "foo";
  ASSERT_EQ(correct, ss.copyString());
  ss = s.valueAt(0);
  checkBuild(ss, ValueType::SmallInt, 1);
  ASSERT_EQ(1, ss.getSmallInt());

  std::string valueOut = "{\"foo\":1}";
  checkDump(s, valueOut);
}

TEST(ParserTest, ObjectSimple2) {
  std::string const value("{ \"foo\" : \"bar\", \"baz\":true}");

  Parser parser;
  ValueLength len = parser.parse(value);
  ASSERT_EQ(1ULL, len);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());
  checkBuild(s, ValueType::Object, 18);
  ASSERT_EQ(2ULL, s.length());

  Slice ss = s.keyAt(0);
  checkBuild(ss, ValueType::String, 4);
  std::string correct = "baz";
  ASSERT_EQ(correct, ss.copyString());
  ss = s.valueAt(0);
  checkBuild(ss, ValueType::Bool, 1);
  ASSERT_TRUE(ss.getBool());

  ss = s.keyAt(1);
  checkBuild(ss, ValueType::String, 4);
  correct = "foo";
  ASSERT_EQ(correct, ss.copyString());
  ss = s.valueAt(1);
  checkBuild(ss, ValueType::String, 4);
  correct = "bar";
  ASSERT_EQ(correct, ss.copyString());

  std::string valueOut = "{\"baz\":true,\"foo\":\"bar\"}";
  checkDump(s, valueOut);
}

TEST(ParserTest, ObjectDenseNotation) {
  std::string const value("{\"a\":\"b\",\"c\":\"d\"}");

  Parser parser;
  ValueLength len = parser.parse(value);
  ASSERT_EQ(1ULL, len);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());
  checkBuild(s, ValueType::Object, 13);
  ASSERT_EQ(2ULL, s.length());

  Slice ss = s.keyAt(0);
  checkBuild(ss, ValueType::String, 2);
  std::string correct = "a";
  ASSERT_EQ(correct, ss.copyString());
  ss = s.valueAt(0);
  checkBuild(ss, ValueType::String, 2);
  correct = "b";
  ASSERT_EQ(correct, ss.copyString());

  ss = s.keyAt(1);
  checkBuild(ss, ValueType::String, 2);
  correct = "c";
  ASSERT_EQ(correct, ss.copyString());
  ss = s.valueAt(1);
  checkBuild(ss, ValueType::String, 2);
  correct = "d";
  ASSERT_EQ(correct, ss.copyString());

  checkDump(s, value);
}

TEST(ParserTest, ObjectReservedKeys) {
  std::string const value(
      "{ \"null\" : \"true\", \"false\":\"bar\", \"true\":\"foo\"}");

  Parser parser;
  ValueLength len = parser.parse(value);
  ASSERT_EQ(1ULL, len);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());
  checkBuild(s, ValueType::Object, 35);
  ASSERT_EQ(3ULL, s.length());

  Slice ss = s.keyAt(0);
  checkBuild(ss, ValueType::String, 6);
  std::string correct = "false";
  ASSERT_EQ(correct, ss.copyString());
  ss = s.valueAt(0);
  checkBuild(ss, ValueType::String, 4);
  correct = "bar";
  ASSERT_EQ(correct, ss.copyString());

  ss = s.keyAt(1);
  checkBuild(ss, ValueType::String, 5);
  correct = "null";
  ASSERT_EQ(correct, ss.copyString());
  ss = s.valueAt(1);
  checkBuild(ss, ValueType::String, 5);
  correct = "true";
  ASSERT_EQ(correct, ss.copyString());

  ss = s.keyAt(2);
  checkBuild(ss, ValueType::String, 5);
  correct = "true";
  ASSERT_EQ(correct, ss.copyString());
  ss = s.valueAt(2);
  checkBuild(ss, ValueType::String, 4);
  correct = "foo";
  ASSERT_EQ(correct, ss.copyString());

  std::string const valueOut =
      "{\"false\":\"bar\",\"null\":\"true\",\"true\":\"foo\"}";
  checkDump(s, valueOut);
}

TEST(ParserTest, ObjectMixed) {
  std::string const value(
      "{\"foo\":null,\"bar\":true,\"baz\":13.53,\"qux\":[1],\"quz\":{}}");

  Parser parser;
  ValueLength len = parser.parse(value);
  ASSERT_EQ(1ULL, len);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());
  checkBuild(s, ValueType::Object, 43);
  ASSERT_EQ(5ULL, s.length());

  Slice ss = s.keyAt(0);
  checkBuild(ss, ValueType::String, 4);
  std::string correct = "bar";
  ASSERT_EQ(correct, ss.copyString());
  ss = s.valueAt(0);
  checkBuild(ss, ValueType::Bool, 1);
  ASSERT_TRUE(ss.getBool());

  ss = s.keyAt(1);
  checkBuild(ss, ValueType::String, 4);
  correct = "baz";
  ASSERT_EQ(correct, ss.copyString());
  ss = s.valueAt(1);
  checkBuild(ss, ValueType::Double, 9);
  ASSERT_EQ(13.53, ss.getDouble());

  ss = s.keyAt(2);
  checkBuild(ss, ValueType::String, 4);
  correct = "foo";
  ASSERT_EQ(correct, ss.copyString());
  ss = s.valueAt(2);
  checkBuild(ss, ValueType::Null, 1);

  ss = s.keyAt(3);
  checkBuild(ss, ValueType::String, 4);
  correct = "qux";
  ASSERT_EQ(correct, ss.copyString());
  ss = s.valueAt(3);
  checkBuild(ss, ValueType::Array, 3);

  Slice sss = ss[0];
  checkBuild(sss, ValueType::SmallInt, 1);
  ASSERT_EQ(1ULL, sss.getUInt());

  ss = s.keyAt(4);
  checkBuild(ss, ValueType::String, 4);
  correct = "quz";
  ASSERT_EQ(correct, ss.copyString());
  ss = s.valueAt(4);
  checkBuild(ss, ValueType::Object, 1);
  ASSERT_EQ(0ULL, ss.length());

  std::string const valueOut(
      "{\"bar\":true,\"baz\":13.53,\"foo\":null,\"qux\":[1],\"quz\":{}}");
  checkDump(s, valueOut);
}

TEST(ParserTest, ObjectInvalidQuotes) {
  std::string const value("{'foo':'bar' }");

  Parser parser;
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value), Exception::ParseError);
}

TEST(ParserTest, ObjectMissingQuotes) {
  std::string const value("{foo:\"bar\" }");

  Parser parser;
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value), Exception::ParseError);
}

TEST(ParserTest, ShortObjectMembers) {
  std::string value("{");
  for (std::size_t i = 0; i < 255; ++i) {
    if (i > 0) {
      value.push_back(',');
    }
    value.append("\"test");
    if (i < 100) {
      value.push_back('0');
      if (i < 10) {
        value.push_back('0');
      }
    }
    value.append(std::to_string(i));
    value.append("\":");
    value.append(std::to_string(i));
  }
  value.push_back('}');

  Parser parser;
  ValueLength len = parser.parse(value);
  ASSERT_EQ(1ULL, len);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());
  ASSERT_EQ(0xcULL, s.head());
  checkBuild(s, ValueType::Object, 3059);
  ASSERT_EQ(255ULL, s.length());

  for (std::size_t i = 0; i < 255; ++i) {
    Slice sk = s.keyAt(i);
    ValueLength len;
    char const* str = sk.getString(len);
    std::string key("test");
    if (i < 100) {
      key.push_back('0');
      if (i < 10) {
        key.push_back('0');
      }
    }
    key.append(std::to_string(i));

    ASSERT_EQ(key.size(), len);
    ASSERT_EQ(0, strncmp(str, key.c_str(), len));
    Slice sv = s.valueAt(i);
    if (i <= 9) {
      checkBuild(sv, ValueType::SmallInt, 1);
    } else {
      checkBuild(sv, ValueType::UInt, 2);
    }
    ASSERT_EQ(i, sv.getUInt());
  }
}

TEST(ParserTest, LongObjectFewMembers) {
  std::string single("0123456789abcdef");
  single.append(single);
  single.append(single);
  single.append(single);
  single.append(single);
  single.append(single);
  single.append(single);  // 1024 bytes

  std::string value("{");
  for (std::size_t i = 0; i < 64; ++i) {
    if (i > 0) {
      value.push_back(',');
    }
    value.append("\"test");
    if (i < 100) {
      value.push_back('0');
      if (i < 10) {
        value.push_back('0');
      }
    }
    value.append(std::to_string(i));
    value.append("\":\"");
    value.append(single);
    value.push_back('\"');
  }
  value.push_back('}');

  Parser parser;
  ValueLength len = parser.parse(value);
  ASSERT_EQ(1ULL, len);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());
  ASSERT_EQ(0x0dULL, s.head());  // object with offset size 4
  checkBuild(s, ValueType::Object, 66889);
  ASSERT_EQ(64ULL, s.length());

  for (std::size_t i = 0; i < 64; ++i) {
    Slice sk = s.keyAt(i);
    ValueLength len;
    char const* str = sk.getString(len);
    std::string key("test");
    if (i < 100) {
      key.push_back('0');
      if (i < 10) {
        key.push_back('0');
      }
    }
    key.append(std::to_string(i));

    ASSERT_EQ(key.size(), len);
    ASSERT_EQ(0, strncmp(str, key.c_str(), len));
    Slice sv = s.valueAt(i);
    str = sv.getString(len);
    ASSERT_EQ(1024ULL, len);
    ASSERT_EQ(0, strncmp(str, single.c_str(), len));
  }
}

TEST(ParserTest, LongObjectManyMembers) {
  std::string value("{");
  for (std::size_t i = 0; i < 256; ++i) {
    if (i > 0) {
      value.push_back(',');
    }
    value.append("\"test");
    if (i < 100) {
      value.push_back('0');
      if (i < 10) {
        value.push_back('0');
      }
    }
    value.append(std::to_string(i));
    value.append("\":");
    value.append(std::to_string(i));
  }
  value.push_back('}');

  Parser parser;
  ValueLength len = parser.parse(value);
  ASSERT_EQ(1ULL, len);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());
  ASSERT_EQ(0x0cULL, s.head());  // long object
  checkBuild(s, ValueType::Object, 3071);
  ASSERT_EQ(256ULL, s.length());

  for (std::size_t i = 0; i < 256; ++i) {
    Slice sk = s.keyAt(i);
    ValueLength len;
    char const* str = sk.getString(len);
    std::string key("test");
    if (i < 100) {
      key.push_back('0');
      if (i < 10) {
        key.push_back('0');
      }
    }
    key.append(std::to_string(i));

    ASSERT_EQ(key.size(), len);
    ASSERT_EQ(0, strncmp(str, key.c_str(), len));
    Slice sv = s.valueAt(i);
    if (i <= 9) {
      checkBuild(sv, ValueType::SmallInt, 1);
    } else {
      checkBuild(sv, ValueType::UInt, 2);
    }
    ASSERT_EQ(i, sv.getUInt());
  }
}

TEST(ParserTest, Utf8Bom) {
  std::string const value("\xef\xbb\xbf{\"foo\":1}");

  Parser parser;
  ValueLength len = parser.parse(value);
  ASSERT_EQ(1ULL, len);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());
  checkBuild(s, ValueType::Object, 8);
  ASSERT_EQ(1ULL, s.length());

  Slice ss = s.keyAt(0);
  checkBuild(ss, ValueType::String, 4);
  std::string correct = "foo";
  ASSERT_EQ(correct, ss.copyString());
  ss = s.valueAt(0);
  checkBuild(ss, ValueType::SmallInt, 1);
  ASSERT_EQ(1ULL, ss.getUInt());

  std::string valueOut = "{\"foo\":1}";
  checkDump(s, valueOut);
}

TEST(ParserTest, Utf8BomBroken) {
  std::string const value("\xef\xbb");

  Parser parser;
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value), Exception::ParseError);
}

TEST(ParserTest, DuplicateAttributesAllowed) {
  std::string const value("{\"foo\":1,\"foo\":2}");

  Parser parser;
  parser.parse(value);
  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  Slice v = s.get("foo");
  ASSERT_TRUE(v.isNumber());
  ASSERT_EQ(1ULL, v.getUInt());
}

TEST(ParserTest, DuplicateAttributesDisallowed) {
  Options options;
  options.checkAttributeUniqueness = true;

  std::string const value("{\"foo\":1,\"foo\":2}");

  Parser parser(&options);
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value),
                              Exception::DuplicateAttributeName);
}

TEST(ParserTest, DuplicateAttributesDisallowedUnsortedInput) {
  Options options;
  options.checkAttributeUniqueness = true;

  std::string const value("{\"foo\":1,\"bar\":3,\"foo\":2}");

  Parser parser(&options);
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value),
                              Exception::DuplicateAttributeName);
}

TEST(ParserTest, DuplicateSubAttributesAllowed) {
  Options options;
  options.checkAttributeUniqueness = true;

  std::string const value(
      "{\"foo\":{\"bar\":1},\"baz\":{\"bar\":2},\"bar\":{\"foo\":23,\"baz\":9}"
      "}");

  Parser parser(&options);
  parser.parse(value);
  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());
  Slice v = s.get(std::vector<std::string>({"foo", "bar"}));
  ASSERT_TRUE(v.isNumber());
  ASSERT_EQ(1ULL, v.getUInt());
}

TEST(ParserTest, DuplicateSubAttributesDisallowed) {
  Options options;
  options.checkAttributeUniqueness = true;

  std::string const value(
      "{\"roo\":{\"bar\":1,\"abc\":true,\"def\":7,\"abc\":2}}");

  Parser parser(&options);
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value),
                              Exception::DuplicateAttributeName);
}

TEST(ParserTest, DuplicateAttributesSortedObjects) {
  Options options;
  options.buildUnindexedObjects = false;
  options.checkAttributeUniqueness = true;

  for (std::size_t i = 1; i < 20; ++i) {
    std::string value;
    value.push_back('{');
    for (std::size_t j = 0; j < i; ++j) {
      if (j != 0) {
        value.push_back(',');
      }
      value.push_back('"');
      value.append("test");
      value.append(std::to_string(j));
      value.append("\":true");
    }
    // now push a duplicate
    value.append(",\"test0\":false");
    value.push_back('}');
  
    Parser parser(&options);
    ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value),
                                Exception::DuplicateAttributeName);
  }
}

TEST(ParserTest, NoDuplicateAttributesSortedObjects) {
  Options options;
  options.buildUnindexedObjects = false;
  options.checkAttributeUniqueness = true;

  for (std::size_t i = 1; i < 20; ++i) {
    std::string value;
    value.push_back('{');
    for (std::size_t j = 0; j < i; ++j) {
      if (j != 0) {
        value.push_back(',');
      }
      value.push_back('"');
      value.append("test");
      value.append(std::to_string(j));
      value.append("\":true");
    }
    value.push_back('}');
  
    Parser parser(&options);
    ASSERT_TRUE(parser.parse(value) > 0);
  }
}

TEST(ParserTest, DuplicateAttributesUnsortedObjects) {
  Options options;
  options.buildUnindexedObjects = true;
  options.checkAttributeUniqueness = true;

  for (std::size_t i = 1; i < 20; ++i) {
    std::string value;
    value.push_back('{');
    for (std::size_t j = 0; j < i; ++j) {
      if (j != 0) {
        value.push_back(',');
      }
      value.push_back('"');
      value.append("test");
      value.append(std::to_string(j));
      value.append("\":true");
    }
    // now push a duplicate
    value.append(",\"test0\":false");
    value.push_back('}');
  
    Parser parser(&options);
    ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value),
                                Exception::DuplicateAttributeName);
  }
}

TEST(ParserTest, NoDuplicateAttributesUnsortedObjects) {
  Options options;
  options.buildUnindexedObjects = true;
  options.checkAttributeUniqueness = true;

  for (std::size_t i = 1; i < 20; ++i) {
    std::string value;
    value.push_back('{');
    for (std::size_t j = 0; j < i; ++j) {
      if (j != 0) {
        value.push_back(',');
      }
      value.push_back('"');
      value.append("test");
      value.append(std::to_string(j));
      value.append("\":true");
    }
    value.push_back('}');
  
    Parser parser(&options);
    ASSERT_TRUE(parser.parse(value) > 0);
  }
}

TEST(ParserTest, FromJsonString) {
  std::string const value("{\"foo\":1,\"bar\":2,\"baz\":3}");

  Options options;
  std::shared_ptr<Builder> b = Parser::fromJson(value, &options);

  Slice s(b->start());
  ASSERT_TRUE(s.hasKey("foo"));
  ASSERT_TRUE(s.hasKey("bar"));
  ASSERT_TRUE(s.hasKey("baz"));
  ASSERT_FALSE(s.hasKey("qux"));
}

TEST(ParserTest, FromJsonChar) {
  char const* value = "{\"foo\":1,\"bar\":2,\"baz\":3}";

  Options options;
  std::shared_ptr<Builder> b = Parser::fromJson(value, strlen(value), &options);

  Slice s(b->start());
  ASSERT_TRUE(s.hasKey("foo"));
  ASSERT_TRUE(s.hasKey("bar"));
  ASSERT_TRUE(s.hasKey("baz"));
  ASSERT_FALSE(s.hasKey("qux"));
}

TEST(ParserTest, FromJsonUInt8) {
  char const* value = "{\"foo\":1,\"bar\":2,\"baz\":3}";

  Options options;
  std::shared_ptr<Builder> b = Parser::fromJson(reinterpret_cast<uint8_t const*>(value), strlen(value), &options);

  Slice s(b->start());
  ASSERT_TRUE(s.hasKey("foo"));
  ASSERT_TRUE(s.hasKey("bar"));
  ASSERT_TRUE(s.hasKey("baz"));
  ASSERT_FALSE(s.hasKey("qux"));
}

TEST(ParserTest, KeepTopLevelOpenFalse) {
  std::string const value("{\"foo\":1,\"bar\":2,\"baz\":3}");

  Options options;
  options.keepTopLevelOpen = false;
  std::shared_ptr<Builder> b = Parser::fromJson(value, &options);
  ASSERT_TRUE(b->isClosed());

  Slice s(b->start());
  ASSERT_TRUE(s.hasKey("foo"));
  ASSERT_TRUE(s.hasKey("bar"));
  ASSERT_TRUE(s.hasKey("baz"));
  ASSERT_FALSE(s.hasKey("qux"));
}

TEST(ParserTest, KeepTopLevelOpenTrue) {
  std::string const value("{\"foo\":1,\"bar\":2,\"baz\":3}");

  Options options;
  options.keepTopLevelOpen = true;
  std::shared_ptr<Builder> b = Parser::fromJson(value, &options);
  ASSERT_FALSE(b->isClosed());

  ASSERT_VELOCYPACK_EXCEPTION(b->start(), Exception::BuilderNotSealed);
  b->close();
  ASSERT_TRUE(b->isClosed());

  Slice s(b->start());
  ASSERT_TRUE(s.hasKey("foo"));
  ASSERT_TRUE(s.hasKey("bar"));
  ASSERT_TRUE(s.hasKey("baz"));
  ASSERT_FALSE(s.hasKey("qux"));
}

TEST(ParserTest, UseNonSSEStringCopy) {
  // modify global function pointer!
  enableBuiltinStringFunctions();

  std::string const value(
      "\"der\\thund\\nging\\rin\\fden\\\\wald\\\"und\\b\\nden'fux\"");

  Parser parser;
  parser.parse(value);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->slice());

  ASSERT_EQ("der\thund\nging\rin\fden\\wald\"und\b\nden'fux", s.copyString());
}

TEST(ParserTest, UseNonSSEUtf8CheckValidString) {
  Options options;
  options.validateUtf8Strings = true;

  // modify global function pointer!
  enableBuiltinStringFunctions();

  std::string const value("\"the quick brown fox jumped over the lazy dog\"");

  Parser parser(&options);
  parser.parse(value);
  Slice s(parser.builder().slice());

  std::string parsed = s.copyString();
  ASSERT_EQ(value.substr(1, value.size() - 2), parsed);  // strip quotes
}

TEST(ParserTest, UseNonSSEUtf8CheckValidStringEscaped) {
  Options options;
  options.validateUtf8Strings = true;

  // modify global function pointer!
  enableBuiltinStringFunctions();

  std::string const value(
      "\"the quick brown\\tfox\\r\\njumped \\\"over\\\" the lazy dog\"");

  Parser parser(&options);
  parser.parse(value);
  Slice s(parser.builder().slice());

  std::string parsed = s.copyString();
  ASSERT_EQ("the quick brown\tfox\r\njumped \"over\" the lazy dog", parsed);
}

TEST(ParserTest, UseNonSSEUtf8CheckInvalidUtf8) {
  Options options;
  options.validateUtf8Strings = true;

  // modify global function pointer!
  enableBuiltinStringFunctions();

  std::string value;
  value.push_back('"');
  for (std::size_t i = 0; i < 100; ++i) {
    value.push_back(static_cast<unsigned char>(0x80));
  }
  value.push_back('"');

  Parser parser(&options);
  ASSERT_VELOCYPACK_EXCEPTION(parser.parse(value),
                              Exception::InvalidUtf8Sequence);
  ASSERT_EQ(1U, parser.errorPos());
  options.validateUtf8Strings = false;
  ASSERT_EQ(1ULL, parser.parse(value));
}

TEST(ParserTest, UseNonSSEWhitespaceCheck) {
  enableBuiltinStringFunctions();

  // modify global function pointer!
  std::string const value("\"foo                 bar\"");
  std::string const all(
      "                                                                        "
      "          " +
      value + "                            ");

  Parser parser;
  parser.parse(all);
  std::shared_ptr<Builder> builder = parser.steal();

  Slice s(builder->slice());

  ASSERT_EQ(value.substr(1, value.size() - 2), s.copyString());
}

TEST(ParserTest, ClearBuilderOption) {
  Options options;
  options.clearBuilderBeforeParse = false;
  Parser parser(&options);
  parser.parse(std::string("0"));
  parser.parse(std::string("1"));
  std::shared_ptr<Builder> builder = parser.steal();

  ASSERT_EQ(builder->buffer()->size(), 2UL);
  uint8_t* p = builder->start();
  ASSERT_EQ(p[0], 0x30);
  ASSERT_EQ(p[1], 0x31);
}

TEST(ParserTest, UseBuilderOnStack) {
  Builder builder;
  {
    Parser parser(builder);
    Builder const& innerBuilder(parser.builder());

    ASSERT_EQ(&builder, &innerBuilder);
    parser.parse("17");
    ASSERT_EQ(innerBuilder.size(), 2UL);
    ASSERT_EQ(builder.size(), 2UL);
  }
  ASSERT_EQ(builder.size(), 2UL);
}

TEST(ParserTest, UseBuilderOnStackForArrayValue) {
  Builder builder;
  builder.openArray();
  {
    Options opt;
    opt.clearBuilderBeforeParse = false;
    Parser parser(builder, &opt);
    Builder const& innerBuilder(parser.builder());

    ASSERT_EQ(&builder, &innerBuilder);
    parser.parse("17");
  }
  builder.close();
  ASSERT_EQ(4UL, builder.size());
}

TEST(ParserTest, UseBuilderOnStackOptionsNullPtr) {
  Builder builder;
  Parser* parser = nullptr;
  ASSERT_VELOCYPACK_EXCEPTION(parser = new Parser(builder, nullptr),
                              Exception::InternalError);
  if (parser == nullptr) {
    parser = new Parser(builder);
  }
  parser->parse("[17]");
  ASSERT_EQ(4UL, builder.size());
  delete parser;
}

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
