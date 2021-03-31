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

#include <string>

#include "tests-common.h"

TEST(IteratorTest, IterateNonArray1) {
  std::string const value("null");
  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  ASSERT_VELOCYPACK_EXCEPTION(ArrayIterator(s), Exception::InvalidValueType);
}

TEST(IteratorTest, IterateNonArray2) {
  std::string const value("true");
  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  ASSERT_VELOCYPACK_EXCEPTION(ArrayIterator(s), Exception::InvalidValueType);
}

TEST(IteratorTest, IterateNonArray3) {
  std::string const value("1");
  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  ASSERT_VELOCYPACK_EXCEPTION(ArrayIterator(s), Exception::InvalidValueType);
}

TEST(IteratorTest, IterateNonArray4) {
  std::string const value("\"abc\"");
  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  ASSERT_VELOCYPACK_EXCEPTION(ArrayIterator(s), Exception::InvalidValueType);
}

TEST(IteratorTest, IterateNonArray5) {
  std::string const value("{}");
  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  ASSERT_VELOCYPACK_EXCEPTION(ArrayIterator(s), Exception::InvalidValueType);
}

TEST(IteratorTest, IterateArrayEmpty) {
  std::string const value("[]");

  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  ArrayIterator it(s);
  ASSERT_EQ(0U, it.size());
  ASSERT_FALSE(it.valid());

  ASSERT_VELOCYPACK_EXCEPTION(it.value(), Exception::IndexOutOfBounds);

  it.next();
  ASSERT_FALSE(it.valid());

  ASSERT_VELOCYPACK_EXCEPTION((*it), Exception::IndexOutOfBounds);
}

TEST(IteratorTest, IterateArrayEmptySpecial) {
  ArrayIterator it(ArrayIterator::Empty{});
  ASSERT_EQ(0U, it.size());
  ASSERT_FALSE(it.valid());

  ASSERT_VELOCYPACK_EXCEPTION(it.value(), Exception::IndexOutOfBounds);

  it.next();
  ASSERT_FALSE(it.valid());

  ASSERT_VELOCYPACK_EXCEPTION((*it), Exception::IndexOutOfBounds);
}

TEST(IteratorTest, IterateArray) {
  std::string const value("[1,2,3,4,null,true,\"foo\",\"bar\"]");

  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  ArrayIterator it(s);
  ASSERT_EQ(8U, it.size());

  ASSERT_TRUE(it.valid());
  Slice current = it.value();
  ASSERT_TRUE(current.isNumber());
  ASSERT_EQ(1UL, current.getUInt());

  it.next();

  ASSERT_TRUE(it.valid());
  current = it.value();
  ASSERT_TRUE(current.isNumber());
  ASSERT_EQ(2UL, current.getUInt());

  it.next();

  ASSERT_TRUE(it.valid());
  current = it.value();
  ASSERT_TRUE(current.isNumber());
  ASSERT_EQ(3UL, current.getUInt());

  it.next();

  ASSERT_TRUE(it.valid());
  current = it.value();
  ASSERT_TRUE(current.isNumber());
  ASSERT_EQ(4UL, current.getUInt());

  it.next();

  ASSERT_TRUE(it.valid());
  current = it.value();
  ASSERT_TRUE(current.isNull());

  it.next();

  ASSERT_TRUE(it.valid());
  current = it.value();
  ASSERT_TRUE(current.isBool());
  ASSERT_TRUE(current.getBool());

  it.next();

  ASSERT_TRUE(it.valid());
  current = it.value();
  ASSERT_TRUE(current.isString());
  ASSERT_EQ("foo", current.copyString());

  it.next();

  ASSERT_TRUE(it.valid());
  current = it.value();
  ASSERT_TRUE(current.isString());
  ASSERT_EQ("bar", current.copyString());

  it.next();
  ASSERT_FALSE(it.valid());

  ASSERT_VELOCYPACK_EXCEPTION(it.value(), Exception::IndexOutOfBounds);
}

TEST(IteratorTest, IterateArrayForward) {
  std::string const value("[1,2,3,4,null,true,\"foo\",\"bar\"]");

  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  ArrayIterator it(s);
  ASSERT_EQ(8U, it.size());

  ASSERT_TRUE(it.valid());
  Slice current = it.value();
  ASSERT_TRUE(current.isNumber());
  ASSERT_EQ(1UL, current.getUInt());

  it.forward(1);

  ASSERT_TRUE(it.valid());
  current = it.value();
  ASSERT_TRUE(current.isNumber());
  ASSERT_EQ(2UL, current.getUInt());

  it.forward(1);

  ASSERT_TRUE(it.valid());
  current = it.value();
  ASSERT_TRUE(current.isNumber());
  ASSERT_EQ(3UL, current.getUInt());

  it.forward(1);

  ASSERT_TRUE(it.valid());
  current = it.value();
  ASSERT_TRUE(current.isNumber());
  ASSERT_EQ(4UL, current.getUInt());

  it.forward(2);

  ASSERT_TRUE(it.valid());
  current = it.value();
  ASSERT_TRUE(current.isBool());
  ASSERT_TRUE(current.getBool());

  it.forward(2);

  ASSERT_TRUE(it.valid());
  current = it.value();
  ASSERT_TRUE(current.isString());
  ASSERT_EQ("bar", current.copyString());

  it.forward(1);

  ASSERT_FALSE(it.valid());
  it.next();
  ASSERT_FALSE(it.valid());

  ASSERT_VELOCYPACK_EXCEPTION(it.value(), Exception::IndexOutOfBounds);
}

TEST(IteratorTest, IterateCompactArrayForward) {
  std::string const value("[1,2,3,4,null,true,\"foo\",\"bar\"]");

  Options options;
  options.buildUnindexedArrays = true;

  Parser parser(&options);
  parser.parse(value);
  Slice s(parser.start());

  ArrayIterator it(s);
  ASSERT_EQ(8U, it.size());

  ASSERT_TRUE(it.valid());
  Slice current = it.value();
  ASSERT_TRUE(current.isNumber());
  ASSERT_EQ(1UL, current.getUInt());

  it.forward(1);

  ASSERT_TRUE(it.valid());
  current = it.value();
  ASSERT_TRUE(current.isNumber());
  ASSERT_EQ(2UL, current.getUInt());

  it.forward(1);

  ASSERT_TRUE(it.valid());
  current = it.value();
  ASSERT_TRUE(current.isNumber());
  ASSERT_EQ(3UL, current.getUInt());

  it.forward(1);

  ASSERT_TRUE(it.valid());
  current = it.value();
  ASSERT_TRUE(current.isNumber());
  ASSERT_EQ(4UL, current.getUInt());

  it.forward(2);

  ASSERT_TRUE(it.valid());
  current = it.value();
  ASSERT_TRUE(current.isBool());
  ASSERT_TRUE(current.getBool());

  it.forward(2);

  ASSERT_TRUE(it.valid());
  current = it.value();
  ASSERT_TRUE(current.isString());
  ASSERT_EQ("bar", current.copyString());

  it.forward(1);

  ASSERT_FALSE(it.valid());
  it.next();
  ASSERT_FALSE(it.valid());

  ASSERT_VELOCYPACK_EXCEPTION(it.value(), Exception::IndexOutOfBounds);
}

TEST(IteratorTest, IterateSubArray) {
  std::string const value("[[1,2,3],[\"foo\",\"bar\"]]");

  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  ArrayIterator it(s);
  ASSERT_EQ(2U, it.size());

  ASSERT_TRUE(it.valid());
  Slice current = it.value();
  ASSERT_TRUE(current.isArray());

  ArrayIterator it2(current);
  ASSERT_EQ(3U, it2.size());
  ASSERT_TRUE(it2.valid());
  Slice sub = it2.value();
  ASSERT_TRUE(sub.isNumber());
  ASSERT_EQ(1UL, sub.getUInt());

  it2.next();

  ASSERT_TRUE(it2.valid());
  sub = it2.value();
  ASSERT_TRUE(sub.isNumber());
  ASSERT_EQ(2UL, sub.getUInt());

  it2.next();

  ASSERT_TRUE(it2.valid());
  sub = it2.value();
  ASSERT_TRUE(sub.isNumber());
  ASSERT_EQ(3UL, sub.getUInt());

  it2.next();
  ASSERT_FALSE(it2.valid());
  ASSERT_VELOCYPACK_EXCEPTION(it2.value(), Exception::IndexOutOfBounds);

  it.next();

  ASSERT_TRUE(it.valid());
  current = it.value();
  ASSERT_TRUE(current.isArray());

  ArrayIterator it3(current);
  ASSERT_EQ(2U, it3.size());

  ASSERT_TRUE(it3.valid());
  sub = it3.value();
  ASSERT_TRUE(sub.isString());
  ASSERT_EQ("foo", sub.copyString());

  it3.next();

  ASSERT_TRUE(it3.valid());
  sub = it3.value();
  ASSERT_TRUE(sub.isString());
  ASSERT_EQ("bar", sub.copyString());

  it3.next();
  ASSERT_FALSE(it3.valid());
  ASSERT_VELOCYPACK_EXCEPTION(it3.value(), Exception::IndexOutOfBounds);

  it.next();
  ASSERT_FALSE(it.valid());
  ASSERT_VELOCYPACK_EXCEPTION(it.value(), Exception::IndexOutOfBounds);
}

TEST(IteratorTest, IterateArrayUnsorted) {
  std::string const value("[1,2,3,4,null,true,\"foo\",\"bar\"]");

  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  ArrayIterator it(s);
  ASSERT_EQ(8U, it.size());

  ASSERT_TRUE(it.valid());
  Slice current = it.value();
  ASSERT_TRUE(current.isNumber());
  ASSERT_EQ(1UL, current.getUInt());

  it.next();

  ASSERT_TRUE(it.valid());
  current = it.value();
  ASSERT_TRUE(current.isNumber());
  ASSERT_EQ(2UL, current.getUInt());

  it.next();

  ASSERT_TRUE(it.valid());
  current = it.value();
  ASSERT_TRUE(current.isNumber());
  ASSERT_EQ(3UL, current.getUInt());

  it.next();

  ASSERT_TRUE(it.valid());
  current = it.value();
  ASSERT_TRUE(current.isNumber());
  ASSERT_EQ(4UL, current.getUInt());

  it.next();

  ASSERT_TRUE(it.valid());
  current = it.value();
  ASSERT_TRUE(current.isNull());

  it.next();

  ASSERT_TRUE(it.valid());
  current = it.value();
  ASSERT_TRUE(current.isBool());
  ASSERT_TRUE(current.getBool());

  it.next();

  ASSERT_TRUE(it.valid());
  current = it.value();
  ASSERT_TRUE(current.isString());
  ASSERT_EQ("foo", current.copyString());

  it.next();

  ASSERT_TRUE(it.valid());
  current = it.value();
  ASSERT_TRUE(current.isString());
  ASSERT_EQ("bar", current.copyString());

  it.next();
  ASSERT_FALSE(it.valid());

  ASSERT_VELOCYPACK_EXCEPTION(it.value(), Exception::IndexOutOfBounds);
}

TEST(IteratorTest, IterateNonObject1) {
  std::string const value("null");
  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  ASSERT_VELOCYPACK_EXCEPTION(ObjectIterator(s), Exception::InvalidValueType);
}

TEST(IteratorTest, IterateNonObject2) {
  std::string const value("true");
  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  ASSERT_VELOCYPACK_EXCEPTION(ObjectIterator(s), Exception::InvalidValueType);
}

TEST(IteratorTest, IterateNonObject3) {
  std::string const value("1");
  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  ASSERT_VELOCYPACK_EXCEPTION(ObjectIterator(s), Exception::InvalidValueType);
}

TEST(IteratorTest, IterateNonObject4) {
  std::string const value("\"abc\"");
  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  ASSERT_VELOCYPACK_EXCEPTION(ObjectIterator(s), Exception::InvalidValueType);
}

TEST(IteratorTest, IterateNonObject5) {
  std::string const value("[]");
  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  ASSERT_VELOCYPACK_EXCEPTION(ObjectIterator(s), Exception::InvalidValueType);
}

TEST(IteratorTest, IterateObjectEmpty) {
  std::string const value("{}");

  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  ObjectIterator it(s);
  ASSERT_FALSE(it.valid());

  ASSERT_VELOCYPACK_EXCEPTION(it.key(), Exception::IndexOutOfBounds);
  ASSERT_VELOCYPACK_EXCEPTION(it.value(), Exception::IndexOutOfBounds);

  it.next();
  ASSERT_FALSE(it.valid());
}

TEST(IteratorTest, IterateObject) {
  std::string const value(
      "{\"a\":1,\"b\":2,\"c\":3,\"d\":4,\"e\":null,\"f\":true,\"g\":\"foo\","
      "\"h\":\"bar\"}");

  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  ObjectIterator it(s);

  ASSERT_TRUE(it.valid());
  Slice key = it.key();
  Slice current = it.value();
  ASSERT_EQ("a", key.copyString());
  ASSERT_TRUE(current.isNumber());
  ASSERT_EQ(1UL, current.getUInt());

  it.next();
  ASSERT_TRUE(it.valid());

  key = it.key();
  current = it.value();
  ASSERT_EQ("b", key.copyString());
  ASSERT_TRUE(current.isNumber());
  ASSERT_EQ(2UL, current.getUInt());

  it.next();
  ASSERT_TRUE(it.valid());

  key = it.key();
  current = it.value();
  ASSERT_EQ("c", key.copyString());
  ASSERT_TRUE(current.isNumber());
  ASSERT_EQ(3UL, current.getUInt());

  it.next();
  ASSERT_TRUE(it.valid());

  key = it.key();
  current = it.value();
  ASSERT_EQ("d", key.copyString());
  ASSERT_TRUE(current.isNumber());
  ASSERT_EQ(4UL, current.getUInt());

  it.next();
  ASSERT_TRUE(it.valid());

  key = it.key();
  current = it.value();
  ASSERT_EQ("e", key.copyString());
  ASSERT_TRUE(current.isNull());

  it.next();
  ASSERT_TRUE(it.valid());

  key = it.key();
  current = it.value();
  ASSERT_EQ("f", key.copyString());
  ASSERT_TRUE(current.isBool());
  ASSERT_TRUE(current.getBool());

  it.next();
  ASSERT_TRUE(it.valid());

  key = it.key();
  current = it.value();
  ASSERT_EQ("g", key.copyString());
  ASSERT_TRUE(current.isString());
  ASSERT_EQ("foo", current.copyString());

  it.next();
  ASSERT_TRUE(it.valid());

  key = it.key();
  current = it.value();
  ASSERT_EQ("h", key.copyString());
  ASSERT_TRUE(current.isString());
  ASSERT_EQ("bar", current.copyString());

  it.next();
  ASSERT_FALSE(it.valid());

  ASSERT_VELOCYPACK_EXCEPTION(it.key(), Exception::IndexOutOfBounds);
  ASSERT_VELOCYPACK_EXCEPTION(it.value(), Exception::IndexOutOfBounds);
}

TEST(IteratorTest, IterateObjectUnsorted) {
  Options options;

  std::string const value("{\"z\":1,\"y\":2,\"x\":3}");

  Parser parser(&options);
  parser.parse(value);
  Slice s(parser.start());

  ObjectIterator it(s, true);

  ASSERT_TRUE(it.valid());
  Slice key = it.key();
  Slice current = it.value();
  ASSERT_EQ("z", key.copyString());
  ASSERT_TRUE(current.isNumber());
  ASSERT_EQ(1UL, current.getUInt());

  it.next();

  ASSERT_TRUE(it.valid());
  key = it.key();
  current = it.value();
  ASSERT_EQ("y", key.copyString());
  ASSERT_TRUE(current.isNumber());
  ASSERT_EQ(2UL, current.getUInt());

  it.next();

  ASSERT_TRUE(it.valid());
  key = it.key();
  current = it.value();
  ASSERT_EQ("x", key.copyString());
  ASSERT_TRUE(current.isNumber());
  ASSERT_EQ(3UL, current.getUInt());

  it.next();
  ASSERT_FALSE(it.valid());
}

TEST(IteratorTest, IterateObjectCompact) {
  Options options;
  options.buildUnindexedObjects = true;

  std::string const value(
      "{\"a\":1,\"b\":2,\"c\":3,\"d\":4,\"e\":null,\"f\":true,\"g\":\"foo\","
      "\"h\":\"bar\"}");

  Parser parser(&options);
  parser.parse(value);
  Slice s(parser.start());

  ASSERT_EQ(0x14, s.head());

  ObjectIterator it(s);

  ASSERT_TRUE(it.valid());
  Slice key = it.key();
  Slice current = it.value();
  ASSERT_EQ("a", key.copyString());
  ASSERT_TRUE(current.isNumber());
  ASSERT_EQ(1UL, current.getUInt());

  it.next();
  ASSERT_TRUE(it.valid());

  key = it.key();
  current = it.value();
  ASSERT_EQ("b", key.copyString());
  ASSERT_TRUE(current.isNumber());
  ASSERT_EQ(2UL, current.getUInt());

  it.next();
  ASSERT_TRUE(it.valid());

  key = it.key();
  current = it.value();
  ASSERT_EQ("c", key.copyString());
  ASSERT_TRUE(current.isNumber());
  ASSERT_EQ(3UL, current.getUInt());

  it.next();
  ASSERT_TRUE(it.valid());

  key = it.key();
  current = it.value();
  ASSERT_EQ("d", key.copyString());
  ASSERT_TRUE(current.isNumber());
  ASSERT_EQ(4UL, current.getUInt());

  it.next();
  ASSERT_TRUE(it.valid());

  key = it.key();
  current = it.value();
  ASSERT_EQ("e", key.copyString());
  ASSERT_TRUE(current.isNull());

  it.next();
  ASSERT_TRUE(it.valid());

  key = it.key();
  current = it.value();
  ASSERT_EQ("f", key.copyString());
  ASSERT_TRUE(current.isBool());
  ASSERT_TRUE(current.getBool());

  it.next();
  ASSERT_TRUE(it.valid());

  key = it.key();
  current = it.value();
  ASSERT_EQ("g", key.copyString());
  ASSERT_TRUE(current.isString());
  ASSERT_EQ("foo", current.copyString());

  it.next();
  ASSERT_TRUE(it.valid());

  key = it.key();
  current = it.value();
  ASSERT_EQ("h", key.copyString());
  ASSERT_TRUE(current.isString());
  ASSERT_EQ("bar", current.copyString());

  it.next();
  ASSERT_FALSE(it.valid());

  ASSERT_VELOCYPACK_EXCEPTION(it.key(), Exception::IndexOutOfBounds);
  ASSERT_VELOCYPACK_EXCEPTION(it.value(), Exception::IndexOutOfBounds);
}

TEST(IteratorTest, IterateObjectKeys) {
  std::string const value(
      "{\"1foo\":\"bar\",\"2baz\":\"quux\",\"3number\":1,\"4boolean\":true,"
      "\"5empty\":null}");

  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  std::size_t state = 0;
  ObjectIterator it(s);

  while (it.valid()) {
    Slice key(it.key());
    Slice value(it.value());

    switch (state++) {
      case 0:
        ASSERT_EQ("1foo", key.copyString());
        ASSERT_TRUE(value.isString());
        ASSERT_EQ("bar", value.copyString());
        break;
      case 1:
        ASSERT_EQ("2baz", key.copyString());
        ASSERT_TRUE(value.isString());
        ASSERT_EQ("quux", value.copyString());
        break;
      case 2:
        ASSERT_EQ("3number", key.copyString());
        ASSERT_TRUE(value.isNumber());
        ASSERT_EQ(1ULL, value.getUInt());
        break;
      case 3:
        ASSERT_EQ("4boolean", key.copyString());
        ASSERT_TRUE(value.isBoolean());
        ASSERT_TRUE(value.getBoolean());
        break;
      case 4:
        ASSERT_EQ("5empty", key.copyString());
        ASSERT_TRUE(value.isNull());
        break;
    }
    it.next();
  }

  ASSERT_EQ(5U, state);
}

TEST(IteratorTest, IterateObjectKeysCompact) {
  std::string const value(
      "{\"1foo\":\"bar\",\"2baz\":\"quux\",\"3number\":1,\"4boolean\":true,"
      "\"5empty\":null}");

  Options options;
  options.buildUnindexedObjects = true;

  Parser parser(&options);
  parser.parse(value);
  Slice s(parser.start());

  ASSERT_EQ(0x14, s.head());

  std::size_t state = 0;
  ObjectIterator it(s);

  while (it.valid()) {
    Slice key(it.key());
    Slice value(it.value());

    switch (state++) {
      case 0:
        ASSERT_EQ("1foo", key.copyString());
        ASSERT_TRUE(value.isString());
        ASSERT_EQ("bar", value.copyString());
        break;
      case 1:
        ASSERT_EQ("2baz", key.copyString());
        ASSERT_TRUE(value.isString());
        ASSERT_EQ("quux", value.copyString());
        break;
      case 2:
        ASSERT_EQ("3number", key.copyString());
        ASSERT_TRUE(value.isNumber());
        ASSERT_EQ(1ULL, value.getUInt());
        break;
      case 3:
        ASSERT_EQ("4boolean", key.copyString());
        ASSERT_TRUE(value.isBoolean());
        ASSERT_TRUE(value.getBoolean());
        break;
      case 4:
        ASSERT_EQ("5empty", key.copyString());
        ASSERT_TRUE(value.isNull());
        break;
    }
    it.next();
  }

  ASSERT_EQ(5U, state);
}

TEST(IteratorTest, IterateObjectValues) {
  std::string const value(
      "{\"1foo\":\"bar\",\"2baz\":\"quux\",\"3number\":1,\"4boolean\":true,"
      "\"5empty\":null}");

  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  std::vector<std::string> seenKeys;
  ObjectIterator it(s);

  while (it.valid()) {
    seenKeys.emplace_back(it.key().copyString());
    it.next();
  };

  ASSERT_EQ(5U, seenKeys.size());
  ASSERT_EQ("1foo", seenKeys[0]);
  ASSERT_EQ("2baz", seenKeys[1]);
  ASSERT_EQ("3number", seenKeys[2]);
  ASSERT_EQ("4boolean", seenKeys[3]);
  ASSERT_EQ("5empty", seenKeys[4]);
}

TEST(IteratorTest, IterateObjectValuesCompact) {
  std::string const value(
      "{\"1foo\":\"bar\",\"2baz\":\"quux\",\"3number\":1,\"4boolean\":true,"
      "\"5empty\":null}");

  Options options;
  options.buildUnindexedObjects = true;

  Parser parser(&options);
  parser.parse(value);
  Slice s(parser.start());

  ASSERT_EQ(0x14, s.head());

  std::vector<std::string> seenKeys;
  ObjectIterator it(s);

  while (it.valid()) {
    seenKeys.emplace_back(it.key().copyString());
    it.next();
  };

  ASSERT_EQ(5U, seenKeys.size());
  ASSERT_EQ("1foo", seenKeys[0]);
  ASSERT_EQ("2baz", seenKeys[1]);
  ASSERT_EQ("3number", seenKeys[2]);
  ASSERT_EQ("4boolean", seenKeys[3]);
  ASSERT_EQ("5empty", seenKeys[4]);
}

TEST(IteratorTest, EmptyArrayIteratorRangeBasedFor) {
  std::string const value("[]");

  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  std::size_t seen = 0;
  for (auto it : ArrayIterator(s)) {
    ASSERT_TRUE(false);
    ASSERT_FALSE(it.isNumber());  // only in here to please the compiler
  }
  ASSERT_EQ(0UL, seen);
}

TEST(IteratorTest, ArrayIteratorRangeBasedFor) {
  std::string const value("[1,2,3,4,5]");

  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  std::size_t seen = 0;
  for (auto it : ArrayIterator(s)) {
    ASSERT_TRUE(it.isNumber());
    ASSERT_EQ(seen + 1, it.getUInt());
    ++seen;
  }
  ASSERT_EQ(5UL, seen);
}

TEST(IteratorTest, ArrayIteratorRangeBasedForConst) {
  std::string const value("[1,2,3,4,5]");

  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  std::size_t seen = 0;
  for (auto const it : ArrayIterator(s)) {
    ASSERT_TRUE(it.isNumber());
    ASSERT_EQ(seen + 1, it.getUInt());
    ++seen;
  }
  ASSERT_EQ(5UL, seen);
}

TEST(IteratorTest, ArrayIteratorRangeBasedForConstRef) {
  std::string const value("[1,2,3,4,5]");

  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  std::size_t seen = 0;
  for (auto const& it : ArrayIterator(s)) {
    ASSERT_TRUE(it.isNumber());
    ASSERT_EQ(seen + 1, it.getUInt());
    ++seen;
  }
  ASSERT_EQ(5UL, seen);
}

TEST(IteratorTest, ArrayIteratorRangeBasedForCompact) {
  std::string const value("[1,2,3,4,5]");

  Options options;
  options.buildUnindexedArrays = true;

  Parser parser(&options);
  parser.parse(value);
  Slice s(parser.start());

  ASSERT_EQ(0x13, s.head());

  std::size_t seen = 0;
  for (auto it : ArrayIterator(s)) {
    ASSERT_TRUE(it.isNumber());
    ASSERT_EQ(seen + 1, it.getUInt());
    ++seen;
  }
  ASSERT_EQ(5UL, seen);
}

TEST(IteratorTest, ObjectIteratorRangeBasedForEmpty) {
  std::string const value("{}");

  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  std::size_t seen = 0;
  for (auto it : ObjectIterator(s)) {
    ASSERT_TRUE(false);
    ASSERT_FALSE(it.value.isNumber());  // only in here to please the compiler
  }
  ASSERT_EQ(0UL, seen);
}

TEST(IteratorTest, ObjectIteratorRangeBasedFor) {
  std::string const value("{\"1foo\":1,\"2bar\":2,\"3qux\":3}");

  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  std::size_t seen = 0;
  for (auto it : ObjectIterator(s)) {
    ASSERT_TRUE(it.key.isString());
    if (seen == 0) {
      ASSERT_EQ("1foo", it.key.copyString());
    } else if (seen == 1) {
      ASSERT_EQ("2bar", it.key.copyString());
    } else if (seen == 2) {
      ASSERT_EQ("3qux", it.key.copyString());
    }
    ASSERT_TRUE(it.value.isNumber());
    ASSERT_EQ(seen + 1, it.value.getUInt());
    ++seen;
  }
  ASSERT_EQ(3UL, seen);
}

TEST(IteratorTest, ObjectIteratorRangeBasedForConst) {
  std::string const value("{\"1foo\":1,\"2bar\":2,\"3qux\":3}");

  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  std::size_t seen = 0;
  for (auto const it : ObjectIterator(s)) {
    ASSERT_TRUE(it.key.isString());
    if (seen == 0) {
      ASSERT_EQ("1foo", it.key.copyString());
    } else if (seen == 1) {
      ASSERT_EQ("2bar", it.key.copyString());
    } else if (seen == 2) {
      ASSERT_EQ("3qux", it.key.copyString());
    }
    ASSERT_TRUE(it.value.isNumber());
    ASSERT_EQ(seen + 1, it.value.getUInt());
    ++seen;
  }
  ASSERT_EQ(3UL, seen);
}

TEST(IteratorTest, ObjectIteratorRangeBasedForConstRef) {
  std::string const value("{\"1foo\":1,\"2bar\":2,\"3qux\":3}");

  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  std::size_t seen = 0;
  for (auto const& it : ObjectIterator(s)) {
    ASSERT_TRUE(it.key.isString());
    if (seen == 0) {
      ASSERT_EQ("1foo", it.key.copyString());
    } else if (seen == 1) {
      ASSERT_EQ("2bar", it.key.copyString());
    } else if (seen == 2) {
      ASSERT_EQ("3qux", it.key.copyString());
    }
    ASSERT_TRUE(it.value.isNumber());
    ASSERT_EQ(seen + 1, it.value.getUInt());
    ++seen;
  }
  ASSERT_EQ(3UL, seen);
}

TEST(IteratorTest, ObjectIteratorRangeBasedForCompact) {
  std::string const value("{\"1foo\":1,\"2bar\":2,\"3qux\":3}");

  Options options;
  options.buildUnindexedObjects = true;

  Parser parser(&options);
  parser.parse(value);
  Slice s(parser.start());

  ASSERT_EQ(0x14, s.head());

  std::size_t seen = 0;
  for (auto it : ObjectIterator(s)) {
    ASSERT_TRUE(it.key.isString());
    if (seen == 0) {
      ASSERT_EQ("1foo", it.key.copyString());
    } else if (seen == 1) {
      ASSERT_EQ("2bar", it.key.copyString());
    } else if (seen == 2) {
      ASSERT_EQ("3qux", it.key.copyString());
    }
    ASSERT_TRUE(it.value.isNumber());
    ASSERT_EQ(seen + 1, it.value.getUInt());
    ++seen;
  }
  ASSERT_EQ(3UL, seen);
}

TEST(IteratorTest, ObjectIteratorTranslations) {
  std::unique_ptr<AttributeTranslator> translator(new AttributeTranslator);

  translator->add("foo", 1);
  translator->add("bar", 2);
  translator->add("baz", 3);
  translator->add("bark", 4);
  translator->add("bart", 0);
  translator->seal();

  AttributeTranslatorScope scope(translator.get());

  Options options;
  options.attributeTranslator = translator.get();

  std::string const value(
      "{\"foo\":1,\"bar\":2,\"qux\":3,\"baz\":4,\"bark\":5,\"bart\":6}");

  Parser parser(&options);
  parser.parse(value);
  Slice s(parser.start());

  ObjectIterator it(s);
  ASSERT_EQ(6UL, it.size());

  while (it.valid()) {
    switch (it.index()) {
      case 0:
        ASSERT_EQ("bar", it.key().copyString());
        break;
      case 1:
        ASSERT_EQ("bark", it.key().copyString());
        break;
      case 2:
        ASSERT_EQ("bart", it.key().copyString());
        break;
      case 3:
        ASSERT_EQ("baz", it.key().copyString());
        break;
      case 4:
        ASSERT_EQ("foo", it.key().copyString());
        break;
      case 5:
        ASSERT_EQ("qux", it.key().copyString());
        break;
    }
    it.next();
  }

  ASSERT_FALSE(it.valid());
  ASSERT_EQ(6UL, it.index());
}

TEST(IteratorTest, ArrayIteratorToStream) {
  std::string const value("[1,2,3,4,5]");
  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  ArrayIterator it(s);

  {
    std::ostringstream result;
    result << it;
    ASSERT_EQ("[ArrayIterator 0 / 5]", result.str());
  }

  it.next();
  {
    std::ostringstream result;
    result << it;
    ASSERT_EQ("[ArrayIterator 1 / 5]", result.str());
  }
  it.next();
  {
    std::ostringstream result;
    result << it;
    ASSERT_EQ("[ArrayIterator 2 / 5]", result.str());
  }
  it.next();
  {
    std::ostringstream result;
    result << it;
    ASSERT_EQ("[ArrayIterator 3 / 5]", result.str());
  }
  it.next();
  {
    std::ostringstream result;
    result << it;
    ASSERT_EQ("[ArrayIterator 4 / 5]", result.str());
  }
  it.next();
  {
    std::ostringstream result;
    result << it;
    ASSERT_EQ("[ArrayIterator 5 / 5]", result.str());
  }
  it.next();
  {
    std::ostringstream result;
    result << it;
    ASSERT_EQ("[ArrayIterator 6 / 5]", result.str());
  }
}

TEST(IteratorTest, ObjectIteratorToStream) {
  std::string const value("{\"foo\":1,\"bar\":2,\"baz\":3}");
  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  ObjectIterator it(s);

  {
    std::ostringstream result;
    result << it;
    ASSERT_EQ("[ObjectIterator 0 / 3]", result.str());
  }

  it.next();
  {
    std::ostringstream result;
    result << it;
    ASSERT_EQ("[ObjectIterator 1 / 3]", result.str());
  }
  it.next();
  {
    std::ostringstream result;
    result << it;
    ASSERT_EQ("[ObjectIterator 2 / 3]", result.str());
  }
  it.next();
  {
    std::ostringstream result;
    result << it;
    ASSERT_EQ("[ObjectIterator 3 / 3]", result.str());
  }
  it.next();
  {
    std::ostringstream result;
    result << it;
    ASSERT_EQ("[ObjectIterator 4 / 3]", result.str());
  }
}

TEST(IteratorTest, ArrayIteratorUnpackTuple) {
  Builder b;
  b.openArray();
  b.add(Value("some string"));
  b.add(Value(12));
  b.add(Value(false));
  b.add(Value("extracted as slice"));
  b.close();

  Slice s = b.slice();
  ArrayIterator iter(s);
  auto t = iter.unpackPrefixAsTuple<std::string, int, bool>();

  ASSERT_EQ(std::get<0>(t), "some string");
  ASSERT_EQ(std::get<1>(t), 12);
  ASSERT_EQ(std::get<2>(t), false);

  ASSERT_TRUE(iter.valid());
  ASSERT_TRUE(iter.value().isEqualString("extracted as slice"));
}

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
