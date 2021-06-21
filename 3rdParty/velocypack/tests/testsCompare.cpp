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

TEST(BinaryCompareTest, BasicTypes) {
  ASSERT_TRUE(BinaryCompare::equals(Slice::noneSlice(), Slice::noneSlice()));
  ASSERT_FALSE(BinaryCompare::equals(Slice::noneSlice(), Slice::nullSlice()));
  ASSERT_FALSE(BinaryCompare::equals(Slice::nullSlice(), Slice::noneSlice()));

  ASSERT_TRUE(BinaryCompare::equals(Slice::nullSlice(), Slice::nullSlice()));
  ASSERT_TRUE(BinaryCompare::equals(Slice::falseSlice(), Slice::falseSlice()));
  ASSERT_TRUE(BinaryCompare::equals(Slice::trueSlice(), Slice::trueSlice()));
  ASSERT_FALSE(BinaryCompare::equals(Slice::trueSlice(), Slice::falseSlice()));
  ASSERT_FALSE(BinaryCompare::equals(Slice::falseSlice(), Slice::trueSlice()));
 
  Builder b1;
  Builder b2;

  b1.add(Value(int64_t(21)));
  b2.add(Value(int64_t(21)));
  ASSERT_TRUE(b1.slice().isInt());
  ASSERT_TRUE(b2.slice().isInt());
  ASSERT_TRUE(BinaryCompare::equals(b1.slice(), b2.slice()));
  
  b2.clear();
  b2.add(Value(uint64_t(21)));
  ASSERT_TRUE(b1.slice().isInt());
  ASSERT_TRUE(b2.slice().isUInt());
  ASSERT_FALSE(BinaryCompare::equals(b1.slice(), b2.slice()));
  
  b1.clear();
  b2.clear();
  b1.add(Value(uint64_t(21)));
  b2.add(Value(uint64_t(21)));
  ASSERT_TRUE(b1.slice().isUInt());
  ASSERT_TRUE(b2.slice().isUInt());
  ASSERT_TRUE(BinaryCompare::equals(b1.slice(), b2.slice()));
}

TEST(NormalizedCompareTest, None) {
  ASSERT_TRUE(NormalizedCompare::equals(Slice::noneSlice(), Slice::noneSlice()));

  ASSERT_FALSE(NormalizedCompare::equals(Slice::noneSlice(), Parser::fromJson("null")->slice()));
}

TEST(NormalizedCompareTest, MinKey) {
  ASSERT_TRUE(NormalizedCompare::equals(Slice::minKeySlice(), Slice::minKeySlice()));

  ASSERT_FALSE(NormalizedCompare::equals(Slice::minKeySlice(), Slice::maxKeySlice()));
  ASSERT_FALSE(NormalizedCompare::equals(Slice::minKeySlice(), Parser::fromJson("null")->slice()));
}

TEST(NormalizedCompareTest, MaxKey) {
  ASSERT_TRUE(NormalizedCompare::equals(Slice::maxKeySlice(), Slice::maxKeySlice()));

  ASSERT_FALSE(NormalizedCompare::equals(Slice::maxKeySlice(), Slice::minKeySlice()));
  ASSERT_FALSE(NormalizedCompare::equals(Slice::maxKeySlice(), Parser::fromJson("null")->slice()));
}

TEST(NormalizedCompareTest, Null) {
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("null")->slice(), Parser::fromJson("null")->slice()));
  
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("null")->slice(), Parser::fromJson("false")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("null")->slice(), Parser::fromJson("true")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("null")->slice(), Parser::fromJson("0")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("null")->slice(), Parser::fromJson("1")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("null")->slice(), Parser::fromJson("-1")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("null")->slice(), Parser::fromJson("\"\"")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("null")->slice(), Parser::fromJson("\" \"")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("null")->slice(), Parser::fromJson("\"0\"")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("null")->slice(), Parser::fromJson("\"1\"")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("null")->slice(), Parser::fromJson("\"-1\"")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("null")->slice(), Parser::fromJson("\"null\"")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("null")->slice(), Parser::fromJson("[]")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("null")->slice(), Parser::fromJson("[null]")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("null")->slice(), Parser::fromJson("{}")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("null")->slice(), Parser::fromJson("{\"a\":1}")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("null")->slice(), Parser::fromJson("{\"b\":1}")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("null")->slice(), Parser::fromJson("{\"a\":null}")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("null")->slice(), Parser::fromJson("{\"null\":null}")->slice()));
}

TEST(NormalizedCompareTest, Bools) {
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("false")->slice(), Parser::fromJson("false")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("true")->slice(), Parser::fromJson("true")->slice()));
  
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("false")->slice(), Parser::fromJson("true")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("true")->slice(), Parser::fromJson("false")->slice()));
  
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("false")->slice(), Parser::fromJson("null")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("false")->slice(), Parser::fromJson("0")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("false")->slice(), Parser::fromJson("1")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("false")->slice(), Parser::fromJson("-1")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("false")->slice(), Parser::fromJson("\"\"")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("false")->slice(), Parser::fromJson("\" \"")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("false")->slice(), Parser::fromJson("\"0\"")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("false")->slice(), Parser::fromJson("\"1\"")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("false")->slice(), Parser::fromJson("\"-1\"")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("false")->slice(), Parser::fromJson("\"false\"")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("false")->slice(), Parser::fromJson("\"true\"")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("false")->slice(), Parser::fromJson("[]")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("false")->slice(), Parser::fromJson("[false]")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("false")->slice(), Parser::fromJson("[true]")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("false")->slice(), Parser::fromJson("[0]")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("false")->slice(), Parser::fromJson("[1]")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("false")->slice(), Parser::fromJson("[-1]")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("false")->slice(), Parser::fromJson("[\"\"]")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("false")->slice(), Parser::fromJson("[\"foobar\"]")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("false")->slice(), Parser::fromJson("[1, 2]")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("false")->slice(), Parser::fromJson("{}")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("false")->slice(), Parser::fromJson("{\"false\":false}")->slice()));
  
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("true")->slice(), Parser::fromJson("null")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("true")->slice(), Parser::fromJson("0")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("true")->slice(), Parser::fromJson("1")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("true")->slice(), Parser::fromJson("-1")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("true")->slice(), Parser::fromJson("\"\"")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("true")->slice(), Parser::fromJson("\" \"")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("true")->slice(), Parser::fromJson("\"0\"")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("true")->slice(), Parser::fromJson("\"1\"")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("true")->slice(), Parser::fromJson("\"-1\"")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("true")->slice(), Parser::fromJson("\"false\"")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("true")->slice(), Parser::fromJson("\"true\"")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("true")->slice(), Parser::fromJson("[]")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("true")->slice(), Parser::fromJson("[false]")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("true")->slice(), Parser::fromJson("[true]")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("true")->slice(), Parser::fromJson("[0]")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("true")->slice(), Parser::fromJson("[1]")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("true")->slice(), Parser::fromJson("[-1]")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("true")->slice(), Parser::fromJson("[\"\"]")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("true")->slice(), Parser::fromJson("[\"foobar\"]")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("true")->slice(), Parser::fromJson("[1, 2]")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("true")->slice(), Parser::fromJson("{}")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("true")->slice(), Parser::fromJson("{\"true\":true}")->slice()));
}

TEST(NormalizedCompareTest, Numbers) {
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("0")->slice(), Parser::fromJson("0")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("0")->slice(), Parser::fromJson("0.0")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("1")->slice(), Parser::fromJson("1")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("1")->slice(), Parser::fromJson("1.0")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("2")->slice(), Parser::fromJson("2")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("2")->slice(), Parser::fromJson("2.0")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("-1")->slice(), Parser::fromJson("-1")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("-1")->slice(), Parser::fromJson("-1.0")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("-10")->slice(), Parser::fromJson("-10")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("-10")->slice(), Parser::fromJson("-10.0")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("100")->slice(), Parser::fromJson("100")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("100")->slice(), Parser::fromJson("100.0")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("-100")->slice(), Parser::fromJson("-100")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("-100")->slice(), Parser::fromJson("-100.0")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("100000000000")->slice(), Parser::fromJson("100000000000")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("-100000000000")->slice(), Parser::fromJson("-100000000000")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("1234.56")->slice(), Parser::fromJson("1234.56")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("-1234.56")->slice(), Parser::fromJson("-1234.56")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("0.000001")->slice(), Parser::fromJson("0.000001")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("-0.000001")->slice(), Parser::fromJson("-0.000001")->slice()));
  
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("0")->slice(), Parser::fromJson("null")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("0")->slice(), Parser::fromJson("false")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("0")->slice(), Parser::fromJson("true")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("0")->slice(), Parser::fromJson("-1")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("0")->slice(), Parser::fromJson("1")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("0")->slice(), Parser::fromJson("0.01")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("0")->slice(), Parser::fromJson("-0.01")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("0")->slice(), Parser::fromJson("0.000001")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("0")->slice(), Parser::fromJson("-0.000001")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("0")->slice(), Parser::fromJson("\"0\"")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("0")->slice(), Parser::fromJson("\"1\"")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("0")->slice(), Parser::fromJson("\"-1\"")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("0")->slice(), Parser::fromJson("\"0.000001\"")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("0")->slice(), Parser::fromJson("\"-0.000001\"")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("0")->slice(), Parser::fromJson("[]")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("0")->slice(), Parser::fromJson("[0]")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("0")->slice(), Parser::fromJson("[0.00001]")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("0")->slice(), Parser::fromJson("{}")->slice()));
  
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("1")->slice(), Parser::fromJson("null")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("1")->slice(), Parser::fromJson("false")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("1")->slice(), Parser::fromJson("true")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("1")->slice(), Parser::fromJson("0")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("1")->slice(), Parser::fromJson("-1")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("1")->slice(), Parser::fromJson("0.0")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("1")->slice(), Parser::fromJson("-1.0")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("1")->slice(), Parser::fromJson("1.1")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("1")->slice(), Parser::fromJson("0.01")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("1")->slice(), Parser::fromJson("0.000001")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("1")->slice(), Parser::fromJson("-0.000001")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("1")->slice(), Parser::fromJson("\"0\"")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("1")->slice(), Parser::fromJson("\"1\"")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("1")->slice(), Parser::fromJson("\"-1\"")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("1")->slice(), Parser::fromJson("\"0.000001\"")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("1")->slice(), Parser::fromJson("\"-0.000001\"")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("1")->slice(), Parser::fromJson("[]")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("1")->slice(), Parser::fromJson("[0]")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("1")->slice(), Parser::fromJson("[1]")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("1")->slice(), Parser::fromJson("[-1]")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("1")->slice(), Parser::fromJson("[0.00001]")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("1")->slice(), Parser::fromJson("{}")->slice()));
  
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("-1")->slice(), Parser::fromJson("null")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("-1")->slice(), Parser::fromJson("false")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("-1")->slice(), Parser::fromJson("true")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("-1")->slice(), Parser::fromJson("0")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("-1")->slice(), Parser::fromJson("1")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("-1")->slice(), Parser::fromJson("0.0")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("-1")->slice(), Parser::fromJson("0.01")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("-1")->slice(), Parser::fromJson("-1.1")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("-1")->slice(), Parser::fromJson("0.000001")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("-1")->slice(), Parser::fromJson("-0.000001")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("-1")->slice(), Parser::fromJson("\"0\"")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("-1")->slice(), Parser::fromJson("\"1\"")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("-1")->slice(), Parser::fromJson("\"-1\"")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("-1")->slice(), Parser::fromJson("\"0.000001\"")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("-1")->slice(), Parser::fromJson("\"-0.000001\"")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("-1")->slice(), Parser::fromJson("[]")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("-1")->slice(), Parser::fromJson("[0]")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("-1")->slice(), Parser::fromJson("[1]")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("-1")->slice(), Parser::fromJson("[-1]")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("-1")->slice(), Parser::fromJson("[0.00001]")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("-1")->slice(), Parser::fromJson("{}")->slice()));
}

TEST(NormalizedCompareTest, Strings) {
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("\"\"")->slice(), Parser::fromJson("\"\"")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("\" \"")->slice(), Parser::fromJson("\" \"")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("\"  \"")->slice(), Parser::fromJson("\"  \"")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("\"0\"")->slice(), Parser::fromJson("\"0\"")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("\"1\"")->slice(), Parser::fromJson("\"1\"")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("\"10\"")->slice(), Parser::fromJson("\"10\"")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("\"10000\"")->slice(), Parser::fromJson("\"10000\"")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("\"a\"")->slice(), Parser::fromJson("\"a\"")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("\"A\"")->slice(), Parser::fromJson("\"A\"")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("\"foo\"")->slice(), Parser::fromJson("\"foo\"")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("\"the quick brown fox jumps over the lazy dog\"")->slice(), Parser::fromJson("\"the quick brown fox jumps over the lazy dog\"")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("\"the quick brown fox jumps over the lazy dog and the quick brown fox jumps over the lazy dog and the quick brown fox jumps over the lazy dog and the quick brown fox jumps over the lazy dog and\"")->slice(), Parser::fromJson("\"the quick brown fox jumps over the lazy dog and the quick brown fox jumps over the lazy dog and the quick brown fox jumps over the lazy dog and the quick brown fox jumps over the lazy dog and\"")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("\"\\t\"")->slice(), Parser::fromJson("\"\\t\"")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("\"\\n\"")->slice(), Parser::fromJson("\"\\n\"")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("\"abc\\ndef\"")->slice(), Parser::fromJson("\"abc\\ndef\"")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("\" the quick\\nbrown\\r\\nfox \"")->slice(), Parser::fromJson("\" the quick\\nbrown\\r\\nfox \"")->slice()));


  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("\"\"")->slice(), Parser::fromJson("\" \"")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("\"\"")->slice(), Parser::fromJson("\"  \"")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("\" \"")->slice(), Parser::fromJson("\"\"")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("\" \"")->slice(), Parser::fromJson("\"  \"")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("\"\\r\"")->slice(), Parser::fromJson("\"\"")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("\"\\r\"")->slice(), Parser::fromJson("\" \"")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("\"\\r\"")->slice(), Parser::fromJson("\"\\n\"")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("\"a\"")->slice(), Parser::fromJson("\"A\"")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("\"a\"")->slice(), Parser::fromJson("\"b\"")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("\"a\"")->slice(), Parser::fromJson("\"a \"")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("\"foo\"")->slice(), Parser::fromJson("\"bar\"")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("\"foo\"")->slice(), Parser::fromJson("\"FOO\"")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("\"the quick brown fox\"")->slice(), Parser::fromJson("\"the quick brown\"")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("\"the quick brown fox\"")->slice(), Parser::fromJson("\"the quick brown foxx\"")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("\"the quick brown fox\"")->slice(), Parser::fromJson("\"the quick brown Fox\"")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("\"the quick brown fox\"")->slice(), Parser::fromJson("\"the quick brown Fox \"")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("\"the quick brown fox\"")->slice(), Parser::fromJson("\"the quick brown FOX\"")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("\"the quick brown fox\"")->slice(), Parser::fromJson("\"the\\nquick\\nbrown\\nfox\"")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("\"the quick brown fox\"")->slice(), Parser::fromJson("\"the\\tquick\\tbrown\\tfox\"")->slice()));
}
  
TEST(NormalizedCompareTest, Arrays) {
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("[]")->slice(), Parser::fromJson("[]")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("[null]")->slice(), Parser::fromJson("[null]")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("[null, null]")->slice(), Parser::fromJson("[null, null]")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("[false]")->slice(), Parser::fromJson("[false]")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("[false, false]")->slice(), Parser::fromJson("[false, false]")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("[true]")->slice(), Parser::fromJson("[true]")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("[true, true]")->slice(), Parser::fromJson("[true, true]")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("[0]")->slice(), Parser::fromJson("[0]")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("[0, 0]")->slice(), Parser::fromJson("[0, 0]")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("[1]")->slice(), Parser::fromJson("[1]")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("[1, 1]")->slice(), Parser::fromJson("[1, 1]")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("[-1]")->slice(), Parser::fromJson("[-1]")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("[-1, -1]")->slice(), Parser::fromJson("[-1, -1]")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("[\"\"]")->slice(), Parser::fromJson("[\"\"]")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("[\"\", \"\"]")->slice(), Parser::fromJson("[\"\", \"\"]")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("[\"0\"]")->slice(), Parser::fromJson("[\"0\"]")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("[\"0\", \"0\"]")->slice(), Parser::fromJson("[\"0\", \"0\"]")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("[\"\", \"\"]")->slice(), Parser::fromJson("[\"\", \"\"]")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("[\"foo\", \"bar\"]")->slice(), Parser::fromJson("[\"foo\", \"bar\"]")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("[[]]")->slice(), Parser::fromJson("[[]]")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("[[], []]")->slice(), Parser::fromJson("[[], []]")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("[[1,2], [3,4]]")->slice(), Parser::fromJson("[[1,2], [3,4]]")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("[[1], [2,3], [3,4,5], [6,7,8,9]]")->slice(), Parser::fromJson("[[1],[2,3],[3,4,5],[6,7,8,9]]")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("[{}]")->slice(), Parser::fromJson("[{}]")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("[{}, {}]")->slice(), Parser::fromJson("[{}, {}]")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("[{\"a\":1}]")->slice(), Parser::fromJson("[{\"a\":1}]")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("[{\"a\":1}, {\"b\":2}]")->slice(), Parser::fromJson("[{\"a\":1},{\"b\":2}]")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("[{\"a\":1}, {\"b\":2}, {\"A\":\"foo\"}, {\"z\":false,\"y\":true}]")->slice(), Parser::fromJson("[{\"a\":1},{\"b\":2},{\"A\":\"foo\"},{\"z\":false,\"y\":true}]")->slice()));
  
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("[0,1,2,3,4,5,6,7,8,9]")->slice(), Parser::fromJson("[0,1,2,3,4,5,6,7,8,9]")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("[1,2,false,\"foo\",\"bar\",\"baz\",null,true,false,-10,[],\"the quick brown fox\"]")->slice(), Parser::fromJson("[1,2,false,\"foo\",\"bar\",\"baz\",null,true,false,-10,[],\"the quick brown fox\"]")->slice()));

  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("[]")->slice(), Parser::fromJson("null")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("[]")->slice(), Parser::fromJson("false")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("[]")->slice(), Parser::fromJson("true")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("[]")->slice(), Parser::fromJson("0")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("[]")->slice(), Parser::fromJson("1")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("[]")->slice(), Parser::fromJson("-1")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("[]")->slice(), Parser::fromJson("\"\"")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("[]")->slice(), Parser::fromJson("\"0\"")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("[]")->slice(), Parser::fromJson("\" \"")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("[]")->slice(), Parser::fromJson("\"[]\"")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("[]")->slice(), Parser::fromJson("\"foobar\"")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("[]")->slice(), Parser::fromJson("[0]")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("[]")->slice(), Parser::fromJson("[1]")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("[]")->slice(), Parser::fromJson("[-1]")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("[]")->slice(), Parser::fromJson("[1,2]")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("[]")->slice(), Parser::fromJson("[\"\"]")->slice()));
  
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("[]")->slice(), Parser::fromJson("{}")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("[]")->slice(), Parser::fromJson("{\"a\":1}")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("[]")->slice(), Parser::fromJson("{\"b\":1}")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("[]")->slice(), Parser::fromJson("{\"b\":\"\"}")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("[]")->slice(), Parser::fromJson("{\"b\":\"1\"}")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("[]")->slice(), Parser::fromJson("{\"b\":[]}")->slice()));
  
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("[1,2,3]")->slice(), Parser::fromJson("[1,2]")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("[1,2,3]")->slice(), Parser::fromJson("[1,2,4]")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("[1,2,3]")->slice(), Parser::fromJson("[1,2,3,4]")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("[1,2,3]")->slice(), Parser::fromJson("[1,2,3,3]")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("[1,2,3]")->slice(), Parser::fromJson("[1,1,2,3]")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("[1,2,3]")->slice(), Parser::fromJson("[1,3,2]")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("[1,2,3]")->slice(), Parser::fromJson("[3,2,1]")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("[1,2,3]")->slice(), Parser::fromJson("[2,1,3]")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("[1,2,3]")->slice(), Parser::fromJson("[2,3,1]")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("[1,2,3]")->slice(), Parser::fromJson("[1,1,1]")->slice()));
  
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("[true,false]")->slice(), Parser::fromJson("[true,true]")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("[true,false]")->slice(), Parser::fromJson("[false,true]")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("[true,false]")->slice(), Parser::fromJson("[true,true]")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("[true,false]")->slice(), Parser::fromJson("[true]")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("[true,false]")->slice(), Parser::fromJson("[true,false,false]")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("[true,false]")->slice(), Parser::fromJson("[true,false,-1]")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("[\"a\",\"b\",\"c\"]")->slice(), Parser::fromJson("[\"a\",\"b\",\"c\",\"d\"]")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("[\"a\",\"b\",\"c\"]")->slice(), Parser::fromJson("[\"a\",\"b\"]")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("[\"a\",\"b\",\"c\"]")->slice(), Parser::fromJson("[\"a\",\"b\",\"d\"]")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("[\"a\",\"b\",\"c\"]")->slice(), Parser::fromJson("[\"b\",\"b\",\"c\"]")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("[\"a\",\"b\",\"c\"]")->slice(), Parser::fromJson("[\"b\",\"c\"]")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("[\"a\",\"b\",\"c\"]")->slice(), Parser::fromJson("[\"b\",\"c\",\"d\"]")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("[\"a\",\"b\",\"c\"]")->slice(), Parser::fromJson("[\"b\",\"c\",\"a\"]")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("[\"a\",\"b\",\"c\"]")->slice(), Parser::fromJson("[\"c\",\"b\",\"a\"]")->slice()));
}

TEST(NormalizedCompareTest, Objects) {
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("{}")->slice(), Parser::fromJson("{}")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("{\"a\":1}")->slice(), Parser::fromJson("{\"a\":1}")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("{\"a\":1,\"A\":2}")->slice(), Parser::fromJson("{\"a\":1,\"A\":2}")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("{\"a\":1,\"A\":2,\"B\":\"C\",\"d\":\"f\"}")->slice(), Parser::fromJson("{\"a\":1,\"A\":2,\"B\":\"C\",\"d\":\"f\"}")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("{\"a\":1,\"b\":2}")->slice(), Parser::fromJson("{\"b\":2,\"a\":1}")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("{\"a\":1,\"b\":2,\"c\":3}")->slice(), Parser::fromJson("{\"b\":2,\"a\":1,\"c\":3}")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("{\"a\":1,\"b\":2,\"c\":3}")->slice(), Parser::fromJson("{\"c\":3,\"b\":2,\"a\":1}")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("{\"a\":1,\"b\":2,\"c\":3}")->slice(), Parser::fromJson("{\"a\":1,\"c\":3,\"b\":2}")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("{\"one\":{\"one-one\":1,\"one-two\":2,\"one-three\":3},\"two\":{\"two-one\":21,\"two-two\":22,\"two-three\":23},\"three\":\"three\"}")->slice(), Parser::fromJson("{\"one\":{\"one-one\":1,\"one-two\":2,\"one-three\":3},\"two\":{\"two-one\":21,\"two-two\":22,\"two-three\":23},\"three\":\"three\"}")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("{\"one\":{\"one-one\":1,\"one-two\":2,\"one-three\":3},\"two\":{\"two-one\":21,\"two-two\":22,\"two-three\":23},\"three\":\"three\"}")->slice(), Parser::fromJson("{\"one\":{\"one-two\":2,\"one-three\":3,\"one-one\":1},\"two\":{\"two-three\":23,\"two-two\":22,\"two-one\":21},\"three\":\"three\"}")->slice()));
  ASSERT_TRUE(NormalizedCompare::equals(Parser::fromJson("{\"one\":{\"one-one\":1,\"one-two\":2,\"one-three\":3},\"two\":{\"two-one\":21,\"two-two\":22,\"two-three\":23},\"three\":\"three\"}")->slice(), Parser::fromJson("{\"three\":\"three\",\"one\":{\"one-three\":3,\"one-one\":1,\"one-two\":2},\"two\":{\"two-two\":22,\"two-three\":23,\"two-one\":21}}")->slice()));

  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("{\"a\":1}")->slice(), Parser::fromJson("{\"A\":1}")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("{\"a\":1}")->slice(), Parser::fromJson("{\"b\":1}")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("{\"a\":1}")->slice(), Parser::fromJson("{\"a\":0}")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("{\"a\":1}")->slice(), Parser::fromJson("{\"a\":-1}")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("{\"a\":1,\"b\":2}")->slice(), Parser::fromJson("{\"a\":1}")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("{\"a\":1,\"b\":2}")->slice(), Parser::fromJson("{\"b\":2}")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("{\"a\":1,\"b\":2}")->slice(), Parser::fromJson("{\"a\":1,\"b\":2,\"c\":3}")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("{\"a\":1,\"b\":2}")->slice(), Parser::fromJson("{\"a\":1,\"b\":3}")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("{\"a\":1,\"b\":2}")->slice(), Parser::fromJson("{\"a\":2,\"b\":1}")->slice()));
  
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("{\"one\":{\"one-one\":1,\"one-two\":2,\"one-three\":3},\"two\":{\"two-one\":21,\"two-two\":22,\"two-three\":23},\"three\":\"three\"}")->slice(), Parser::fromJson("{\"one\":{\"one-one\":2,\"one-two\":2,\"one-three\":3},\"two\":{\"two-one\":21,\"two-two\":22,\"two-three\":23},\"three\":\"three\"}")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("{\"one\":{\"one-one\":1,\"one-two\":2,\"one-three\":3},\"two\":{\"two-one\":21,\"two-two\":22,\"two-three\":23},\"three\":\"three\"}")->slice(), Parser::fromJson("{\"one\":{\"one-one\":1,\"one-two\":2},\"two\":{\"two-one\":21,\"two-two\":22,\"two-three\":23},\"three\":\"three\"}")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("{\"one\":{\"one-one\":1,\"one-two\":2,\"one-three\":3},\"two\":{\"two-one\":21,\"two-two\":22,\"two-three\":23},\"three\":\"three\"}")->slice(), Parser::fromJson("{\"one\":{\"one-one\":1,\"one-two\":2,\"one-three\":3},\"two\":{\"two-one\":21,\"two-two\":22,\"two-three\":23}}")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("{\"one\":{\"one-one\":1,\"one-two\":2,\"one-three\":3},\"two\":{\"two-one\":21,\"two-two\":22,\"two-three\":23},\"three\":\"three\"}")->slice(), Parser::fromJson("{\"one\":{},\"two\":{\"two-one\":21,\"two-two\":22,\"two-three\":23},\"three\":\"three\"}")->slice()));
  ASSERT_FALSE(NormalizedCompare::equals(Parser::fromJson("{\"one\":{\"one-one\":1,\"one-two\":2,\"one-three\":3},\"two\":{\"two-one\":21,\"two-two\":22,\"two-three\":23},\"three\":\"three\"}")->slice(), Parser::fromJson("{\"one\":{\"one-one\":1,\"one-two\":2,\"one-three\":3},\"two\":{\"two-one\":21,\"two-two\":22,\"two-three\":23},\"three\":\"three\",\"four\":\"four\"}")->slice()));
}

TEST(NormalizedCompareTest, Custom) {
  Builder b;
  uint8_t* p = b.add(ValuePair(2ULL, ValueType::Custom));
  *p++ = 0xf0;
  *p++ = 0xaa;  

  ASSERT_VELOCYPACK_EXCEPTION(NormalizedCompare::equals(b.slice(), b.slice()), Exception::NotImplemented);
}

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
