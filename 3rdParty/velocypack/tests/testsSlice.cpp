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
#include <iostream>
#include <initializer_list>

#include "tests-common.h"

static unsigned char LocalBuffer[4096];

TEST(SliceTest, NoneFactory) {
  Slice s = Slice::noneSlice();
  ASSERT_TRUE(s.isNone());
}

TEST(SliceTest, NullFactory) {
  Slice s = Slice::nullSlice();
  ASSERT_TRUE(s.isNull());
}

TEST(SliceTest, ZeroFactory) {
  Slice s = Slice::zeroSlice();
  ASSERT_TRUE(s.isSmallInt());
  ASSERT_EQ(0UL, s.getUInt());
}

TEST(SliceTest, IllegalFactory) {
  Slice s = Slice::illegalSlice();
  ASSERT_TRUE(s.isIllegal());
}

TEST(SliceTest, FalseFactory) {
  Slice s = Slice::falseSlice();
  ASSERT_TRUE(s.isBoolean() && !s.getBoolean());
}

TEST(SliceTest, TrueFactory) {
  Slice s = Slice::trueSlice();
  ASSERT_TRUE(s.isBoolean() && s.getBoolean());
}

TEST(SliceTest, EmptyArrayFactory) {
  Slice s = Slice::emptyArraySlice();
  ASSERT_TRUE(s.isArray() && s.length() == 0);
}

TEST(SliceTest, EmptyObjectFactory) {
  Slice s = Slice::emptyObjectSlice();
  ASSERT_TRUE(s.isObject() && s.length() == 0);
}

TEST(SliceTest, MinKeyFactory) {
  Slice s = Slice::minKeySlice();
  ASSERT_TRUE(s.isMinKey());
}

TEST(SliceTest, MaxKeyFactory) {
  Slice s = Slice::maxKeySlice();
  ASSERT_TRUE(s.isMaxKey());
}

TEST(SliceTest, ResolveExternal) {
  ASSERT_TRUE(Slice::illegalSlice().isIllegal());
  ASSERT_TRUE(Slice::illegalSlice().resolveExternal().isIllegal());

  ASSERT_TRUE(Slice::noneSlice().isNone());
  ASSERT_TRUE(Slice::noneSlice().resolveExternal().isNone());

  ASSERT_TRUE(Slice::nullSlice().isNull());
  ASSERT_TRUE(Slice::nullSlice().resolveExternal().isNull());

  ASSERT_TRUE(Slice::falseSlice().isFalse());
  ASSERT_TRUE(Slice::falseSlice().resolveExternal().isFalse());

  ASSERT_TRUE(Slice::trueSlice().isTrue());
  ASSERT_TRUE(Slice::trueSlice().resolveExternal().isTrue());

  ASSERT_TRUE(Slice::zeroSlice().isSmallInt());
  ASSERT_TRUE(Slice::zeroSlice().resolveExternal().isSmallInt());

  ASSERT_TRUE(Slice::emptyArraySlice().isArray());
  ASSERT_TRUE(Slice::emptyArraySlice().resolveExternal().isArray());

  ASSERT_TRUE(Slice::emptyObjectSlice().isObject());
  ASSERT_TRUE(Slice::emptyObjectSlice().resolveExternal().isObject());

  ASSERT_TRUE(Slice::minKeySlice().isMinKey());
  ASSERT_TRUE(Slice::minKeySlice().resolveExternal().isMinKey());

  ASSERT_TRUE(Slice::maxKeySlice().isMaxKey());
  ASSERT_TRUE(Slice::maxKeySlice().resolveExternal().isMaxKey());

  Builder builder;
  builder.openArray();
  builder.add(Value(1));
  builder.close();

  LocalBuffer[0] = 0x1d;
  char const* p = builder.slice().startAs<char const>();
  memcpy(&LocalBuffer[1], &p, sizeof(char const*));

  ASSERT_TRUE(Slice(&LocalBuffer[0]).isExternal());
  ASSERT_FALSE(Slice(&LocalBuffer[0]).isArray());
  ASSERT_TRUE(Slice(&LocalBuffer[0]).resolveExternal().isArray());
  ASSERT_TRUE(Slice(&LocalBuffer[0]).resolveExternal().at(0).isNumber());

  Builder builder2;
  builder2.openObject();
  builder2.add("foo", Value(1));
  builder2.add("bar", Value("baz"));
  builder2.close();

  LocalBuffer[0] = 0x1d;
  p = builder2.slice().startAs<char const>();
  memcpy(&LocalBuffer[1], &p, sizeof(char const*));

  ASSERT_TRUE(Slice(&LocalBuffer[0]).isExternal());
  ASSERT_FALSE(Slice(&LocalBuffer[0]).isObject());
  ASSERT_TRUE(Slice(&LocalBuffer[0]).resolveExternal().isObject());
  ASSERT_TRUE(Slice(&LocalBuffer[0]).resolveExternal().get("foo").isNumber());
  ASSERT_EQ(1, Slice(&LocalBuffer[0]).resolveExternal().get("foo").getInt());
  ASSERT_TRUE(Slice(&LocalBuffer[0]).resolveExternal().get("bar").isString());
  ASSERT_EQ("baz", Slice(&LocalBuffer[0]).resolveExternal().get("bar").copyString());
}

TEST(SliceTest, GetResolveExternal) {
  Builder bExternal;
  bExternal.openObject();
  bExternal.add("foo", Value(1));
  bExternal.add("bar", Value(2));
  bExternal.add("baz", Value(3));
  bExternal.close();

  Slice bs = bExternal.slice();

  Builder builder;
  builder.openObject();
  builder.add("boo", Value(static_cast<void const*>(bs.start()), ValueType::External));
  builder.close();

  Slice s = builder.slice();

  ASSERT_FALSE(s.get(std::vector<std::string>{"boo"}).isExternal());
  ASSERT_FALSE(s.get(std::vector<std::string>{"boo"}, false).isExternal());
  ASSERT_FALSE(s.get(std::vector<std::string>{"boo"}, true).isExternal());
}

TEST(SliceTest, GetResolveExternalExternal) {
  Builder bExternal;
  bExternal.openObject();
  bExternal.add("foo", Value(1));
  bExternal.add("bar", Value(2));
  bExternal.add("baz", Value(3));
  bExternal.close();

  Builder builder;
  builder.openObject();
  builder.add("boo", Value(static_cast<void const*>(bExternal.slice().start()), ValueType::External));
  builder.close();

  Builder b;
  b.add(Value(static_cast<void const*>(builder.slice().start()), ValueType::External));

  Slice s = b.slice();

  ASSERT_FALSE(s.get(std::vector<std::string>{"boo"}, true).isExternal());
  ASSERT_TRUE(s.get(std::vector<std::string>{"boo"}, true).isObject());
}

TEST(SliceTest, GetEmptyPath) {
  std::string const value("{\"foo\":{\"bar\":{\"baz\":3,\"bark\":4},\"qux\":5},\"poo\":6}");

  Parser parser;
  parser.parse(value);
  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  {
    std::vector<std::string> lookup;
    ASSERT_VELOCYPACK_EXCEPTION(s.get(lookup), Exception::InvalidAttributePath);
    ASSERT_VELOCYPACK_EXCEPTION(s.get(lookup.begin(), lookup.end()), Exception::InvalidAttributePath);
  }

  {
    std::vector<StringRef> lookup;
    ASSERT_VELOCYPACK_EXCEPTION(s.get(lookup), Exception::InvalidAttributePath);
    ASSERT_VELOCYPACK_EXCEPTION(s.get(lookup.begin(), lookup.end()), Exception::InvalidAttributePath);
  }
}

TEST(SliceTest, GetOnNonObject) {
  Slice s = Slice::nullSlice();

  {
    std::vector<std::string> lookup{"foo"};
    ASSERT_VELOCYPACK_EXCEPTION(s.get(lookup), Exception::InvalidValueType);
    ASSERT_VELOCYPACK_EXCEPTION(s.get(lookup.begin(), lookup.end()), Exception::InvalidValueType);
  }

  {
    std::vector<StringRef> lookup{StringRef{"foo"}};
    ASSERT_VELOCYPACK_EXCEPTION(s.get(lookup), Exception::InvalidValueType);
    ASSERT_VELOCYPACK_EXCEPTION(s.get(lookup.begin(), lookup.end()), Exception::InvalidValueType);
  }
}

TEST(SliceTest, GetOnNestedObject) {
  std::string const value("{\"foo\":{\"bar\":{\"baz\":3,\"bark\":4},\"qux\":5},\"poo\":6}");

  Parser parser;
  parser.parse(value);
  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  auto runTest = [s](auto lookup) {
    lookup.emplace_back("foo");
    ASSERT_TRUE(s.get(lookup).isObject());
    ASSERT_TRUE(s.get(lookup.begin(), lookup.end()).isObject());

    lookup.emplace_back("boo"); // foo.boo does not exist
    ASSERT_TRUE(s.get(lookup).isNone());
    ASSERT_TRUE(s.get(lookup.begin(), lookup.end()).isNone());
    lookup.pop_back();

    lookup.emplace_back("bar"); // foo.bar
    ASSERT_TRUE(s.get(lookup).isObject());
    ASSERT_TRUE(s.get(lookup.begin(), lookup.end()).isObject());

    lookup.emplace_back("baz"); // foo.bar.baz
    ASSERT_TRUE(s.get(lookup).isNumber());
    ASSERT_TRUE(s.get(lookup.begin(), lookup.end()).isNumber());
    ASSERT_EQ(3, s.get(lookup).getInt());
    ASSERT_EQ(3, s.get(lookup.begin(), lookup.end()).getInt());

    lookup.emplace_back("bat"); // foo.bar.baz.bat
    ASSERT_TRUE(s.get(lookup).isNone());
    ASSERT_TRUE(s.get(lookup.begin(), lookup.end()).isNone());
    lookup.pop_back(); // foo.bar.baz
    lookup.pop_back(); // foo.bar

    lookup.emplace_back("bark"); // foo.bar.bark
    ASSERT_TRUE(s.get(lookup).isNumber());
    ASSERT_TRUE(s.get(lookup.begin(), lookup.end()).isNumber());
    ASSERT_EQ(4, s.get(lookup).getInt());
    ASSERT_EQ(4, s.get(lookup.begin(), lookup.end()).getInt());

    lookup.emplace_back("bat"); // foo.bar.bark.bat
    ASSERT_TRUE(s.get(lookup).isNone());
    ASSERT_TRUE(s.get(lookup.begin(), lookup.end()).isNone());
    lookup.pop_back(); // foo.bar.bark
    lookup.pop_back(); // foo.bar

    lookup.emplace_back("borg"); // foo.bar.borg
    ASSERT_TRUE(s.get(lookup).isNone());
    ASSERT_TRUE(s.get(lookup.begin(), lookup.end()).isNone());
    lookup.pop_back(); // foo.bar
    lookup.pop_back(); // foo

    lookup.emplace_back("qux"); // foo.qux
    ASSERT_TRUE(s.get(lookup).isNumber());
    ASSERT_TRUE(s.get(lookup.begin(), lookup.end()).isNumber());
    ASSERT_EQ(5, s.get(lookup).getInt());
    ASSERT_EQ(5, s.get(lookup.begin(), lookup.end()).getInt());
    lookup.pop_back(); // foo

    lookup.emplace_back("poo"); // foo.poo
    ASSERT_TRUE(s.get(lookup).isNone());
    ASSERT_TRUE(s.get(lookup.begin(), lookup.end()).isNone());
    lookup.pop_back(); // foo
    lookup.pop_back(); // {}

    lookup.emplace_back("poo"); // poo
    ASSERT_TRUE(s.get(lookup).isNumber());
    ASSERT_TRUE(s.get(lookup.begin(), lookup.end()).isNumber());
    ASSERT_EQ(6, s.get(lookup).getInt());
    ASSERT_EQ(6, s.get(lookup.begin(), lookup.end()).getInt());
  };

  runTest(std::vector<std::string>());
  runTest(std::vector<StringRef>());

  {
    auto lookup = { "foo", "bar", "baz" };
    ASSERT_TRUE(s.get(lookup).isNumber());
    ASSERT_TRUE(s.get(lookup.begin(), lookup.end()).isNumber());
    ASSERT_EQ(3, s.get(lookup).getInt());
    ASSERT_EQ(3, s.get(lookup.begin(), lookup.end()).getInt());
  }

  {
    std::initializer_list<char const*> lookup = { "foo", "bar", "baz" };
    ASSERT_TRUE(s.get(lookup).isNumber());
    ASSERT_TRUE(s.get(lookup.begin(), lookup.end()).isNumber());
    ASSERT_EQ(3, s.get(lookup).getInt());
    ASSERT_EQ(3, s.get(lookup.begin(), lookup.end()).getInt());
  }
}

TEST(SliceTest, GetWithIterator) {
  std::string const value("{\"foo\":{\"bar\":{\"baz\":3,\"bark\":4},\"qux\":5},\"poo\":6}");

  Parser parser;
  parser.parse(value);
  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  {
    auto lookup = { "foo", "bar", "baz" };
    ASSERT_TRUE(s.get(lookup).isNumber());
    ASSERT_TRUE(s.get(lookup.begin(), lookup.end()).isNumber());
    ASSERT_EQ(3, s.get(lookup).getInt());
    ASSERT_EQ(3, s.get(lookup.begin(), lookup.end()).getInt());
  }

  {
    std::initializer_list<char const*> lookup = { "foo", "bar", "baz" };
    ASSERT_TRUE(s.get(lookup).isNumber());
    ASSERT_TRUE(s.get(lookup.begin(), lookup.end()).isNumber());
    ASSERT_EQ(3, s.get(lookup).getInt());
    ASSERT_EQ(3, s.get(lookup.begin(), lookup.end()).getInt());
  }

  {
    std::initializer_list<std::string> lookup = { "foo", "bar", "baz" };
    ASSERT_TRUE(s.get(lookup).isNumber());
    ASSERT_TRUE(s.get(lookup.begin(), lookup.end()).isNumber());
    ASSERT_EQ(3, s.get(lookup).getInt());
    ASSERT_EQ(3, s.get(lookup.begin(), lookup.end()).getInt());
  }

  {
    std::initializer_list<StringRef> lookup = {StringRef("foo"), StringRef("bar"), StringRef("baz") };
    ASSERT_TRUE(s.get(lookup).isNumber());
    ASSERT_TRUE(s.get(lookup.begin(), lookup.end()).isNumber());
    ASSERT_EQ(3, s.get(lookup).getInt());
    ASSERT_EQ(3, s.get(lookup.begin(), lookup.end()).getInt());
  }

  {
    std::initializer_list<HashedStringRef> lookup = {HashedStringRef("foo", 3), HashedStringRef("bar", 3), HashedStringRef("baz", 3) };
    ASSERT_TRUE(s.get(lookup).isNumber());
    ASSERT_TRUE(s.get(lookup.begin(), lookup.end()).isNumber());
    ASSERT_EQ(3, s.get(lookup).getInt());
    ASSERT_EQ(3, s.get(lookup.begin(), lookup.end()).getInt());
  }
}

TEST(SliceTest, SliceStart) {
  std::string const value("null");

  Parser parser;
  parser.parse(value);
  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  ASSERT_EQ(0x18UL, s.head());
  ASSERT_EQ(0x18UL, *s.start());
  ASSERT_EQ('\x18', *s.startAs<char>());
  ASSERT_EQ('\x18', *s.startAs<unsigned char>());
  ASSERT_EQ(0x18UL, *s.startAs<uint8_t>());

  ASSERT_EQ(s.start(), s.begin());
  ASSERT_EQ(s.start() + 1, s.end());
}

TEST(SliceTest, ToJsonNull) {
  std::string const value("null");

  Parser parser;
  parser.parse(value);
  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());
  ASSERT_EQ("null", s.toJson());

  std::string out;
  StringSink sink(&out);
  s.toJson(&sink);
  ASSERT_EQ("null", out);

  out.clear();
  ASSERT_EQ("null", s.toJson(out));
}

TEST(SliceTest, ToJsonFalse) {
  std::string const value("false");

  Parser parser;
  parser.parse(value);
  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());
  ASSERT_EQ("false", s.toJson());
  
  std::string out;
  StringSink sink(&out);
  s.toJson(&sink);
  ASSERT_EQ("false", out);
  
  out.clear();
  ASSERT_EQ("false", s.toJson(out));
}

TEST(SliceTest, ToJsonTrue) {
  std::string const value("true");

  Parser parser;
  parser.parse(value);
  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());
  ASSERT_EQ("true", s.toJson());
  
  std::string out;
  StringSink sink(&out);
  s.toJson(&sink);
  ASSERT_EQ("true", out);
  
  out.clear();
  ASSERT_EQ("true", s.toJson(out));
}

TEST(SliceTest, ToJsonNumber) {
  std::string const value("-12345");

  Parser parser;
  parser.parse(value);
  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());
  ASSERT_EQ("-12345", s.toJson());
  
  std::string out;
  StringSink sink(&out);
  s.toJson(&sink);
  ASSERT_EQ("-12345", out);
  
  out.clear();
  ASSERT_EQ("-12345", s.toJson(out));
}

TEST(SliceTest, ToJsonString) {
  std::string const value("\"foobarbaz\"");

  Parser parser;
  parser.parse(value);
  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());
  ASSERT_EQ("\"foobarbaz\"", s.toJson());
  
  std::string out;
  StringSink sink(&out);
  s.toJson(&sink);
  ASSERT_EQ("\"foobarbaz\"", out);
  
  out.clear();
  ASSERT_EQ("\"foobarbaz\"", s.toJson(out));
}

TEST(SliceTest, ToJsonArrayEmpty) {
  std::string const value("[]");

  Parser parser;
  parser.parse(value);
  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());
  ASSERT_EQ(0x01, s.head());
  ASSERT_EQ("[]", s.toJson());
  
  std::string out;
  StringSink sink(&out);
  s.toJson(&sink);
  ASSERT_EQ("[]", out);
  
  out.clear();
  ASSERT_EQ("[]", s.toJson(out));
}

TEST(SliceTest, ToJsonArray) {
  std::string const value("[1,2,3,4,5]");

  Parser parser;
  parser.parse(value);
  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());
  ASSERT_EQ("[1,2,3,4,5]", s.toJson());
  
  std::string out;
  StringSink sink(&out);
  s.toJson(&sink);
  ASSERT_EQ("[1,2,3,4,5]", out);
  
  out.clear();
  ASSERT_EQ("[1,2,3,4,5]", s.toJson(out));
}

TEST(SliceTest, ToJsonArrayCompact) {
  std::string const value("[1,2,3,4,5]");

  Options options;
  options.buildUnindexedArrays = true;

  Parser parser(&options);
  parser.parse(value);
  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());
  ASSERT_EQ(0x13, s.head());

  ASSERT_EQ("[1,2,3,4,5]", s.toJson());
  
  std::string out;
  StringSink sink(&out);
  s.toJson(&sink);
  ASSERT_EQ("[1,2,3,4,5]", out);
  
  out.clear();
  ASSERT_EQ("[1,2,3,4,5]", s.toJson(out));
}

TEST(SliceTest, ToJsonObjectEmpty) {
  std::string const value("{}");

  Options options;
  options.buildUnindexedObjects = true;

  Parser parser(&options);
  parser.parse(value);
  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());
  ASSERT_EQ(0x0a, s.head());

  ASSERT_EQ("{}", s.toJson());
  
  std::string out;
  StringSink sink(&out);
  s.toJson(&sink);
  ASSERT_EQ("{}", out);
  
  out.clear();
  ASSERT_EQ("{}", s.toJson(out));
}

TEST(SliceTest, ToJsonObject) {
  std::string const value("{\"a\":1,\"b\":2,\"c\":3,\"d\":4,\"e\":5}");

  Options options;
  options.buildUnindexedObjects = true;

  Parser parser(&options);
  parser.parse(value);
  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());
  ASSERT_EQ(0x14, s.head());

  ASSERT_EQ("{\"a\":1,\"b\":2,\"c\":3,\"d\":4,\"e\":5}", s.toJson());
  
  std::string out;
  StringSink sink(&out);
  s.toJson(&sink);
  ASSERT_EQ("{\"a\":1,\"b\":2,\"c\":3,\"d\":4,\"e\":5}", out);
  
  out.clear();
  ASSERT_EQ("{\"a\":1,\"b\":2,\"c\":3,\"d\":4,\"e\":5}", s.toJson(out));
}

TEST(SliceTest, ToJsonObjectCompact) {
  std::string const value("{\"a\":1,\"b\":2,\"c\":3,\"d\":4,\"e\":5}");

  Parser parser;
  parser.parse(value);
  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());
  ASSERT_EQ("{\"a\":1,\"b\":2,\"c\":3,\"d\":4,\"e\":5}", s.toJson());
  
  std::string out;
  StringSink sink(&out);
  s.toJson(&sink);
  ASSERT_EQ("{\"a\":1,\"b\":2,\"c\":3,\"d\":4,\"e\":5}", out);
  
  out.clear();
  ASSERT_EQ("{\"a\":1,\"b\":2,\"c\":3,\"d\":4,\"e\":5}", s.toJson(out));
}

TEST(SliceTest, InvalidGetters) {
  std::string const value("[null,true,1,\"foo\",[],{}]");

  Parser parser;
  parser.parse(value);
  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  ValueLength len;

  ASSERT_VELOCYPACK_EXCEPTION(s.at(0).getBool(), Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(0).getInt(), Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(0).getUInt(), Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(0).getSmallInt(),
                              Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(0).getDouble(), Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(0).copyString(),
                              Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(0).copyBinary(),
                              Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(0).getString(len),
                              Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(0).getBinary(len),
                              Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(0).getExternal(),
                              Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(0).getUTCDate(),
                              Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(0).length(), Exception::InvalidValueType);

  ASSERT_VELOCYPACK_EXCEPTION(s.at(1).getInt(), Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(1).getUInt(), Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(1).getSmallInt(),
                              Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(1).getDouble(), Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(1).copyString(),
                              Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(1).copyBinary(),
                              Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(1).getString(len),
                              Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(1).getBinary(len),
                              Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(1).getExternal(),
                              Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(1).getUTCDate(),
                              Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(1).length(), Exception::InvalidValueType);

  ASSERT_VELOCYPACK_EXCEPTION(s.at(2).getBool(), Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(2).copyString(),
                              Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(2).copyBinary(),
                              Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(2).getString(len),
                              Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(2).getBinary(len),
                              Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(2).getExternal(),
                              Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(2).getUTCDate(),
                              Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(2).length(), Exception::InvalidValueType);

  ASSERT_VELOCYPACK_EXCEPTION(s.at(3).getBool(), Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(3).getInt(), Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(3).getUInt(), Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(3).getSmallInt(),
                              Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(3).getDouble(), Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(3).getExternal(),
                              Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(3).getUTCDate(),
                              Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(3).getBinary(len),
                              Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(3).copyBinary(),
                              Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(3).length(), Exception::InvalidValueType);

  ASSERT_VELOCYPACK_EXCEPTION(s.at(4).getBool(), Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(4).getInt(), Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(4).getUInt(), Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(4).getSmallInt(),
                              Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(4).getDouble(), Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(4).getExternal(),
                              Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(4).getUTCDate(),
                              Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(4).copyBinary(),
                              Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(4).copyString(),
                              Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(2).getString(len),
                              Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(2).getBinary(len),
                              Exception::InvalidValueType);

  ASSERT_VELOCYPACK_EXCEPTION(s.at(5).getBool(), Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(5).getInt(), Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(5).getUInt(), Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(5).getSmallInt(),
                              Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(5).getDouble(), Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(5).getExternal(),
                              Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(5).getUTCDate(),
                              Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(5).getString(len),
                              Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(5).getBinary(len),
                              Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(5).copyBinary(),
                              Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(5).copyString(),
                              Exception::InvalidValueType);
}

TEST(SliceTest, LengthNull) {
  std::string const value("null");

  Parser parser;
  parser.parse(value);
  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  ASSERT_VELOCYPACK_EXCEPTION(s.length(), Exception::InvalidValueType);
}

TEST(SliceTest, LengthTrue) {
  std::string const value("true");

  Parser parser;
  parser.parse(value);
  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  ASSERT_VELOCYPACK_EXCEPTION(s.length(), Exception::InvalidValueType);
}

TEST(SliceTest, LengthArrayEmpty) {
  std::string const value("[]");

  Parser parser;
  parser.parse(value);
  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  ASSERT_EQ(0UL, s.length());
}

TEST(SliceTest, LengthArray) {
  std::string const value("[1,2,3,4,5,6,7,8,\"foo\",\"bar\"]");

  Parser parser;
  parser.parse(value);
  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  ASSERT_EQ(10UL, s.length());
}

TEST(SliceTest, LengthArrayCompact) {
  std::string const value("[1,2,3,4,5,6,7,8,\"foo\",\"bar\"]");

  Options options;
  options.buildUnindexedArrays = true;

  Parser parser(&options);
  parser.parse(value);
  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  ASSERT_EQ(0x13, s.head());
  ASSERT_EQ(10UL, s.length());
}

TEST(SliceTest, LengthObjectEmpty) {
  std::string const value("{}");

  Parser parser;
  parser.parse(value);
  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  ASSERT_EQ(0UL, s.length());
}

TEST(SliceTest, LengthObject) {
  std::string const value(
      "{\"a\":1,\"b\":2,\"c\":3,\"d\":4,\"e\":5,\"f\":6,\"g\":7,\"h\":8,\"i\":"
      "\"foo\",\"j\":\"bar\"}");

  Parser parser;
  parser.parse(value);
  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  ASSERT_EQ(10UL, s.length());
}

TEST(SliceTest, LengthObjectCompact) {
  std::string const value(
      "{\"a\":1,\"b\":2,\"c\":3,\"d\":4,\"e\":5,\"f\":6,\"g\":7,\"h\":8,\"i\":"
      "\"foo\",\"j\":\"bar\"}");

  Options options;
  options.buildUnindexedObjects = true;

  Parser parser(&options);
  parser.parse(value);
  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  ASSERT_EQ(0x14, s.head());
  ASSERT_EQ(10UL, s.length());
}

TEST(SliceTest, Null) {
  LocalBuffer[0] = 0x18;

  Slice slice(reinterpret_cast<uint8_t const*>(&LocalBuffer[0]));

  ASSERT_EQ(ValueType::Null, slice.type());
  ASSERT_TRUE(slice.isNull());
  ASSERT_EQ(1ULL, slice.byteSize());
}

TEST(SliceTest, False) {
  LocalBuffer[0] = 0x19;

  Slice slice(reinterpret_cast<uint8_t const*>(&LocalBuffer[0]));

  ASSERT_EQ(ValueType::Bool, slice.type());
  ASSERT_TRUE(slice.isBool());
  ASSERT_TRUE(slice.isFalse());
  ASSERT_FALSE(slice.isTrue());
  ASSERT_EQ(1ULL, slice.byteSize());
  ASSERT_FALSE(slice.getBool());
}

TEST(SliceTest, True) {
  LocalBuffer[0] = 0x1a;

  Slice slice(reinterpret_cast<uint8_t const*>(&LocalBuffer[0]));

  ASSERT_EQ(ValueType::Bool, slice.type());
  ASSERT_TRUE(slice.isBool());
  ASSERT_FALSE(slice.isFalse());
  ASSERT_TRUE(slice.isTrue());
  ASSERT_EQ(1ULL, slice.byteSize());
  ASSERT_TRUE(slice.getBool());
}

TEST(SliceTest, MinKey) {
  LocalBuffer[0] = 0x1e;

  Slice slice(reinterpret_cast<uint8_t const*>(&LocalBuffer[0]));

  ASSERT_EQ(ValueType::MinKey, slice.type());
  ASSERT_TRUE(slice.isMinKey());
  ASSERT_EQ(1ULL, slice.byteSize());
}

TEST(SliceTest, MaxKey) {
  LocalBuffer[0] = 0x1f;

  Slice slice(reinterpret_cast<uint8_t const*>(&LocalBuffer[0]));

  ASSERT_EQ(ValueType::MaxKey, slice.type());
  ASSERT_TRUE(slice.isMaxKey());
  ASSERT_EQ(1ULL, slice.byteSize());
}

TEST(SliceTest, Double) {
  LocalBuffer[0] = 0x1b;

  double value = 23.5;
  dumpDouble(value, reinterpret_cast<uint8_t*>(LocalBuffer) + 1);

  Slice slice(reinterpret_cast<uint8_t const*>(&LocalBuffer[0]));

  ASSERT_EQ(ValueType::Double, slice.type());
  ASSERT_TRUE(slice.isDouble());
  ASSERT_EQ(9ULL, slice.byteSize());
  ASSERT_DOUBLE_EQ(value, slice.getDouble());
}

TEST(SliceTest, DoubleNegative) {
  LocalBuffer[0] = 0x1b;

  double value = -999.91355;
  dumpDouble(value, reinterpret_cast<uint8_t*>(LocalBuffer) + 1);

  Slice slice(reinterpret_cast<uint8_t const*>(&LocalBuffer[0]));

  ASSERT_EQ(ValueType::Double, slice.type());
  ASSERT_TRUE(slice.isDouble());
  ASSERT_EQ(9ULL, slice.byteSize());
  ASSERT_DOUBLE_EQ(value, slice.getDouble());
}

TEST(SliceTest, SmallInt) {
  int64_t expected[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, -6, -5, -4, -3, -2, -1};

  for (int i = 0; i < 16; ++i) {
    LocalBuffer[0] = 0x30 + i;

    Slice slice(reinterpret_cast<uint8_t const*>(&LocalBuffer[0]));
    ASSERT_EQ(ValueType::SmallInt, slice.type());
    ASSERT_TRUE(slice.isSmallInt());
    ASSERT_EQ(1ULL, slice.byteSize());

    ASSERT_EQ(expected[i], slice.getSmallInt());
    ASSERT_EQ(expected[i], slice.getInt());
  }
}

TEST(SliceTest, Int1) {
  LocalBuffer[0] = 0x20;
  uint8_t value = 0x33;
  memcpy(&LocalBuffer[1], (void*)&value, sizeof(value));

  Slice slice(reinterpret_cast<uint8_t const*>(&LocalBuffer[0]));

  ASSERT_EQ(ValueType::Int, slice.type());
  ASSERT_TRUE(slice.isInt());
  ASSERT_EQ(2ULL, slice.byteSize());

  ASSERT_EQ(value, slice.getInt());
  ASSERT_EQ(value, slice.getSmallInt());
}

TEST(SliceTest, Int2) {
  LocalBuffer[0] = 0x21;
  uint8_t* p = (uint8_t*)&LocalBuffer[1];
  *p++ = 0x23;
  *p++ = 0x42;

  Slice slice(reinterpret_cast<uint8_t const*>(&LocalBuffer[0]));

  ASSERT_EQ(ValueType::Int, slice.type());
  ASSERT_TRUE(slice.isInt());
  ASSERT_EQ(3ULL, slice.byteSize());
  ASSERT_EQ(0x4223LL, slice.getInt());
  ASSERT_EQ(0x4223LL, slice.getSmallInt());
}

TEST(SliceTest, Int3) {
  LocalBuffer[0] = 0x22;
  uint8_t* p = (uint8_t*)&LocalBuffer[1];
  *p++ = 0x23;
  *p++ = 0x42;
  *p++ = 0x66;

  Slice slice(reinterpret_cast<uint8_t const*>(&LocalBuffer[0]));

  ASSERT_EQ(ValueType::Int, slice.type());
  ASSERT_TRUE(slice.isInt());
  ASSERT_EQ(4ULL, slice.byteSize());
  ASSERT_EQ(0x664223LL, slice.getInt());
  ASSERT_EQ(0x664223LL, slice.getSmallInt());
}

TEST(SliceTest, Int4) {
  LocalBuffer[0] = 0x23;
  uint8_t* p = (uint8_t*)&LocalBuffer[1];
  *p++ = 0x23;
  *p++ = 0x42;
  *p++ = 0x66;
  *p++ = 0x7c;

  Slice slice(reinterpret_cast<uint8_t const*>(&LocalBuffer[0]));

  ASSERT_EQ(ValueType::Int, slice.type());
  ASSERT_TRUE(slice.isInt());
  ASSERT_EQ(5ULL, slice.byteSize());
  ASSERT_EQ(0x7c664223LL, slice.getInt());
  ASSERT_EQ(0x7c664223LL, slice.getSmallInt());
}

TEST(SliceTest, Int5) {
  LocalBuffer[0] = 0x24;
  uint8_t* p = (uint8_t*)&LocalBuffer[1];
  *p++ = 0x23;
  *p++ = 0x42;
  *p++ = 0x66;
  *p++ = 0xac;
  *p++ = 0x6f;

  Slice slice(reinterpret_cast<uint8_t const*>(&LocalBuffer[0]));

  ASSERT_EQ(ValueType::Int, slice.type());
  ASSERT_TRUE(slice.isInt());
  ASSERT_EQ(6ULL, slice.byteSize());
  ASSERT_EQ(0x6fac664223LL, slice.getInt());
  ASSERT_EQ(0x6fac664223LL, slice.getSmallInt());
}

TEST(SliceTest, Int6) {
  LocalBuffer[0] = 0x25;
  uint8_t* p = (uint8_t*)&LocalBuffer[1];
  *p++ = 0x23;
  *p++ = 0x42;
  *p++ = 0x66;
  *p++ = 0xac;
  *p++ = 0xff;
  *p++ = 0x3f;

  Slice slice(reinterpret_cast<uint8_t const*>(&LocalBuffer[0]));

  ASSERT_EQ(ValueType::Int, slice.type());
  ASSERT_TRUE(slice.isInt());
  ASSERT_EQ(7ULL, slice.byteSize());
  ASSERT_EQ(0x3fffac664223LL, slice.getInt());
  ASSERT_EQ(0x3fffac664223LL, slice.getSmallInt());
}

TEST(SliceTest, Int7) {
  LocalBuffer[0] = 0x26;
  uint8_t* p = (uint8_t*)&LocalBuffer[1];
  *p++ = 0x23;
  *p++ = 0x42;
  *p++ = 0x66;
  *p++ = 0xac;
  *p++ = 0xff;
  *p++ = 0x3f;
  *p++ = 0x5a;

  Slice slice(reinterpret_cast<uint8_t const*>(&LocalBuffer[0]));

  ASSERT_EQ(ValueType::Int, slice.type());
  ASSERT_TRUE(slice.isInt());
  ASSERT_EQ(8ULL, slice.byteSize());
  ASSERT_EQ(0x5a3fffac664223LL, slice.getInt());
  ASSERT_EQ(0x5a3fffac664223LL, slice.getSmallInt());
}

TEST(SliceTest, Int8) {
  LocalBuffer[0] = 0x27;
  uint8_t* p = (uint8_t*)&LocalBuffer[1];
  *p++ = 0x23;
  *p++ = 0x42;
  *p++ = 0x66;
  *p++ = 0xac;
  *p++ = 0xff;
  *p++ = 0x3f;
  *p++ = 0xfa;
  *p++ = 0x6f;

  Slice slice(reinterpret_cast<uint8_t const*>(&LocalBuffer[0]));

  ASSERT_EQ(ValueType::Int, slice.type());
  ASSERT_TRUE(slice.isInt());
  ASSERT_EQ(9ULL, slice.byteSize());
  ASSERT_EQ(0x6ffa3fffac664223LL, slice.getInt());
  ASSERT_EQ(0x6ffa3fffac664223LL, slice.getSmallInt());
}

TEST(SliceTest, IntMax) {
  Builder b;
  b.add(Value(INT64_MAX));

  Slice slice(b.slice());

  ASSERT_EQ(ValueType::Int, slice.type());
  ASSERT_TRUE(slice.isInt());
  ASSERT_EQ(9ULL, slice.byteSize());
  ASSERT_EQ(INT64_MAX, slice.getInt());
}

TEST(SliceTest, NegInt1) {
  LocalBuffer[0] = 0x20;
  uint8_t value = 0xa3;
  memcpy(&LocalBuffer[1], (void*)&value, sizeof(value));

  Slice slice(reinterpret_cast<uint8_t const*>(&LocalBuffer[0]));

  ASSERT_EQ(ValueType::Int, slice.type());
  ASSERT_TRUE(slice.isInt());
  ASSERT_EQ(2ULL, slice.byteSize());

  ASSERT_EQ(static_cast<int64_t>(0xffffffffffffffa3ULL), slice.getInt());
}

TEST(SliceTest, NegInt2) {
  LocalBuffer[0] = 0x21;
  uint8_t* p = (uint8_t*)&LocalBuffer[1];
  *p++ = 0x23;
  *p++ = 0xe2;

  Slice slice(reinterpret_cast<uint8_t const*>(&LocalBuffer[0]));

  ASSERT_EQ(ValueType::Int, slice.type());
  ASSERT_TRUE(slice.isInt());
  ASSERT_EQ(3ULL, slice.byteSize());
  ASSERT_EQ(static_cast<int64_t>(0xffffffffffffe223ULL), slice.getInt());
}

TEST(SliceTest, NegInt3) {
  LocalBuffer[0] = 0x22;
  uint8_t* p = (uint8_t*)&LocalBuffer[1];
  *p++ = 0x23;
  *p++ = 0x42;
  *p++ = 0xd6;

  Slice slice(reinterpret_cast<uint8_t const*>(&LocalBuffer[0]));

  ASSERT_EQ(ValueType::Int, slice.type());
  ASSERT_TRUE(slice.isInt());
  ASSERT_EQ(4ULL, slice.byteSize());
  ASSERT_EQ(static_cast<int64_t>(0xffffffffffd64223ULL), slice.getInt());
}

TEST(SliceTest, NegInt4) {
  LocalBuffer[0] = 0x23;
  uint8_t* p = (uint8_t*)&LocalBuffer[1];
  *p++ = 0x23;
  *p++ = 0x42;
  *p++ = 0x66;
  *p++ = 0xac;

  Slice slice(reinterpret_cast<uint8_t const*>(&LocalBuffer[0]));

  ASSERT_EQ(ValueType::Int, slice.type());
  ASSERT_TRUE(slice.isInt());
  ASSERT_EQ(5ULL, slice.byteSize());
  ASSERT_EQ(static_cast<int64_t>(0xffffffffac664223ULL), slice.getInt());
}

TEST(SliceTest, NegInt5) {
  LocalBuffer[0] = 0x24;
  uint8_t* p = (uint8_t*)&LocalBuffer[1];
  *p++ = 0x23;
  *p++ = 0x42;
  *p++ = 0x66;
  *p++ = 0xac;
  *p++ = 0xff;

  Slice slice(reinterpret_cast<uint8_t const*>(&LocalBuffer[0]));

  ASSERT_EQ(ValueType::Int, slice.type());
  ASSERT_TRUE(slice.isInt());
  ASSERT_EQ(6ULL, slice.byteSize());
  ASSERT_EQ(static_cast<int64_t>(0xffffffffac664223ULL), slice.getInt());
}

TEST(SliceTest, NegInt6) {
  LocalBuffer[0] = 0x25;
  uint8_t* p = (uint8_t*)&LocalBuffer[1];
  *p++ = 0x23;
  *p++ = 0x42;
  *p++ = 0x66;
  *p++ = 0xac;
  *p++ = 0xff;
  *p++ = 0xef;

  Slice slice(reinterpret_cast<uint8_t const*>(&LocalBuffer[0]));

  ASSERT_EQ(ValueType::Int, slice.type());
  ASSERT_TRUE(slice.isInt());
  ASSERT_EQ(7ULL, slice.byteSize());
  ASSERT_EQ(static_cast<int64_t>(0xffffefffac664223ULL), slice.getInt());
}

TEST(SliceTest, NegInt7) {
  LocalBuffer[0] = 0x26;
  uint8_t* p = (uint8_t*)&LocalBuffer[1];
  *p++ = 0x23;
  *p++ = 0x42;
  *p++ = 0x66;
  *p++ = 0xac;
  *p++ = 0xff;
  *p++ = 0xef;
  *p++ = 0xfa;

  Slice slice(reinterpret_cast<uint8_t const*>(&LocalBuffer[0]));

  ASSERT_EQ(ValueType::Int, slice.type());
  ASSERT_TRUE(slice.isInt());
  ASSERT_EQ(8ULL, slice.byteSize());
  ASSERT_EQ(static_cast<int64_t>(0xfffaefffac664223ULL), slice.getInt());
}

TEST(SliceTest, NegInt8) {
  LocalBuffer[0] = 0x27;
  uint8_t* p = (uint8_t*)&LocalBuffer[1];
  *p++ = 0x23;
  *p++ = 0x42;
  *p++ = 0x66;
  *p++ = 0xac;
  *p++ = 0xff;
  *p++ = 0xef;
  *p++ = 0xfa;
  *p++ = 0x8e;

  Slice slice(reinterpret_cast<uint8_t const*>(&LocalBuffer[0]));

  ASSERT_EQ(ValueType::Int, slice.type());
  ASSERT_TRUE(slice.isInt());
  ASSERT_EQ(9ULL, slice.byteSize());
  ASSERT_EQ(static_cast<int64_t>(0x8efaefffac664223ULL), slice.getInt());
}

TEST(SliceTest, IntMin) {
  Builder b;
  b.add(Value(INT64_MIN));

  Slice slice(b.slice());

  ASSERT_EQ(ValueType::Int, slice.type());
  ASSERT_TRUE(slice.isInt());
  ASSERT_EQ(9ULL, slice.byteSize());
  ASSERT_EQ(INT64_MIN, slice.getInt());
  ASSERT_VELOCYPACK_EXCEPTION(slice.getUInt(), Exception::NumberOutOfRange);
}

TEST(SliceTest, UInt1) {
  LocalBuffer[0] = 0x28;
  uint8_t value = 0x33;
  memcpy(&LocalBuffer[1], (void*)&value, sizeof(value));

  Slice slice(reinterpret_cast<uint8_t const*>(&LocalBuffer[0]));

  ASSERT_EQ(ValueType::UInt, slice.type());
  ASSERT_TRUE(slice.isUInt());
  ASSERT_EQ(2ULL, slice.byteSize());
  ASSERT_EQ(value, slice.getUInt());
}

TEST(SliceTest, UInt2) {
  LocalBuffer[0] = 0x29;
  uint8_t* p = (uint8_t*)&LocalBuffer[1];
  *p++ = 0x23;
  *p++ = 0x42;

  Slice slice(reinterpret_cast<uint8_t const*>(&LocalBuffer[0]));

  ASSERT_EQ(ValueType::UInt, slice.type());
  ASSERT_TRUE(slice.isUInt());
  ASSERT_EQ(3ULL, slice.byteSize());
  ASSERT_EQ(0x4223ULL, slice.getUInt());
}

TEST(SliceTest, UInt3) {
  LocalBuffer[0] = 0x2a;
  uint8_t* p = (uint8_t*)&LocalBuffer[1];
  *p++ = 0x23;
  *p++ = 0x42;
  *p++ = 0x66;

  Slice slice(reinterpret_cast<uint8_t const*>(&LocalBuffer[0]));

  ASSERT_EQ(ValueType::UInt, slice.type());
  ASSERT_TRUE(slice.isUInt());
  ASSERT_EQ(4ULL, slice.byteSize());
  ASSERT_EQ(0x664223ULL, slice.getUInt());
}

TEST(SliceTest, UInt4) {
  LocalBuffer[0] = 0x2b;
  uint8_t* p = (uint8_t*)&LocalBuffer[1];
  *p++ = 0x23;
  *p++ = 0x42;
  *p++ = 0x66;
  *p++ = 0xac;

  Slice slice(reinterpret_cast<uint8_t const*>(&LocalBuffer[0]));

  ASSERT_EQ(ValueType::UInt, slice.type());
  ASSERT_TRUE(slice.isUInt());
  ASSERT_EQ(5ULL, slice.byteSize());
  ASSERT_EQ(0xac664223ULL, slice.getUInt());
}

TEST(SliceTest, UInt5) {
  LocalBuffer[0] = 0x2c;
  uint8_t* p = (uint8_t*)&LocalBuffer[1];
  *p++ = 0x23;
  *p++ = 0x42;
  *p++ = 0x66;
  *p++ = 0xac;
  *p++ = 0xff;

  Slice slice(reinterpret_cast<uint8_t const*>(&LocalBuffer[0]));

  ASSERT_EQ(ValueType::UInt, slice.type());
  ASSERT_TRUE(slice.isUInt());
  ASSERT_EQ(6ULL, slice.byteSize());
  ASSERT_EQ(0xffac664223ULL, slice.getUInt());
}

TEST(SliceTest, UInt6) {
  LocalBuffer[0] = 0x2d;
  uint8_t* p = (uint8_t*)&LocalBuffer[1];
  *p++ = 0x23;
  *p++ = 0x42;
  *p++ = 0x66;
  *p++ = 0xac;
  *p++ = 0xff;
  *p++ = 0xee;

  Slice slice(reinterpret_cast<uint8_t const*>(&LocalBuffer[0]));

  ASSERT_EQ(ValueType::UInt, slice.type());
  ASSERT_TRUE(slice.isUInt());
  ASSERT_EQ(7ULL, slice.byteSize());
  ASSERT_EQ(0xeeffac664223ULL, slice.getUInt());
}

TEST(SliceTest, UInt7) {
  LocalBuffer[0] = 0x2e;
  uint8_t* p = (uint8_t*)&LocalBuffer[1];
  *p++ = 0x23;
  *p++ = 0x42;
  *p++ = 0x66;
  *p++ = 0xac;
  *p++ = 0xff;
  *p++ = 0xee;
  *p++ = 0x59;

  Slice slice(reinterpret_cast<uint8_t const*>(&LocalBuffer[0]));

  ASSERT_EQ(ValueType::UInt, slice.type());
  ASSERT_TRUE(slice.isUInt());
  ASSERT_EQ(8ULL, slice.byteSize());
  ASSERT_EQ(0x59eeffac664223ULL, slice.getUInt());
}

TEST(SliceTest, UInt8) {
  LocalBuffer[0] = 0x2f;
  uint8_t* p = (uint8_t*)&LocalBuffer[1];
  *p++ = 0x23;
  *p++ = 0x42;
  *p++ = 0x66;
  *p++ = 0xac;
  *p++ = 0xff;
  *p++ = 0xee;
  *p++ = 0x59;
  *p++ = 0xab;

  Slice slice(reinterpret_cast<uint8_t const*>(&LocalBuffer[0]));

  ASSERT_EQ(ValueType::UInt, slice.type());
  ASSERT_TRUE(slice.isUInt());
  ASSERT_EQ(9ULL, slice.byteSize());
  ASSERT_EQ(0xab59eeffac664223ULL, slice.getUInt());
}

TEST(SliceTest, UIntMax) {
  Builder b;
  b.add(Value(UINT64_MAX));

  Slice slice(b.slice());

  ASSERT_EQ(ValueType::UInt, slice.type());
  ASSERT_TRUE(slice.isUInt());
  ASSERT_EQ(9ULL, slice.byteSize());
  ASSERT_EQ(UINT64_MAX, slice.getUInt());
  ASSERT_VELOCYPACK_EXCEPTION(slice.getInt(), Exception::NumberOutOfRange);
}

TEST(SliceTest, ArrayEmpty) {
  LocalBuffer[0] = 0x01;

  Slice slice(reinterpret_cast<uint8_t const*>(&LocalBuffer[0]));

  ASSERT_EQ(ValueType::Array, slice.type());
  ASSERT_TRUE(slice.isArray());
  ASSERT_TRUE(slice.isEmptyArray());
  ASSERT_EQ(1ULL, slice.byteSize());
  ASSERT_EQ(0ULL, slice.length());
}

TEST(SliceTest, ObjectEmpty) {
  LocalBuffer[0] = 0x0a;

  Slice slice(reinterpret_cast<uint8_t const*>(&LocalBuffer[0]));

  ASSERT_EQ(ValueType::Object, slice.type());
  ASSERT_TRUE(slice.isObject());
  ASSERT_TRUE(slice.isEmptyObject());
  ASSERT_EQ(1ULL, slice.byteSize());
  ASSERT_EQ(0ULL, slice.length());
}

TEST(SliceTest, StringNoString) {
  Slice slice;

  ASSERT_FALSE(slice.isString());
  ValueLength length;
  ASSERT_VELOCYPACK_EXCEPTION(slice.getString(length),
                              Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(slice.getStringLength(),
                              Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(slice.copyString(), Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(slice.stringRef(), Exception::InvalidValueType);
}

TEST(SliceTest, StringEmpty) {
  LocalBuffer[0] = 0x40;

  Slice slice(reinterpret_cast<uint8_t const*>(&LocalBuffer[0]));

  ASSERT_EQ(ValueType::String, slice.type());
  ASSERT_TRUE(slice.isString());
  ASSERT_EQ(1ULL, slice.byteSize());
  ValueLength len;
  char const* s = slice.getString(len);
  ASSERT_EQ(0ULL, len);
  ASSERT_EQ(0, strncmp(s, "", len));

  ASSERT_EQ(0U, slice.getStringLength());
  ASSERT_EQ("", slice.copyString());
  ASSERT_TRUE(StringRef().equals(slice.stringRef()));
}

TEST(SliceTest, StringLengths) {
  Builder builder;

  for (std::size_t i = 0; i < 255; ++i) {
    builder.clear();

    std::string temp;
    for (std::size_t j = 0; j < i; ++j) {
      temp.push_back('x');
    }

    builder.add(Value(temp));

    Slice slice = builder.slice();

    ASSERT_TRUE(slice.isString());
    ASSERT_EQ(ValueType::String, slice.type());

    ASSERT_EQ(i, slice.getStringLength());

    if (i <= 126) {
      ASSERT_EQ(i + 1, slice.byteSize());
    } else {
      ASSERT_EQ(i + 9, slice.byteSize());
    }
  }
}

TEST(SliceTest, String1) {
  LocalBuffer[0] = 0x40 + static_cast<char>(strlen("foobar"));

  Slice slice(reinterpret_cast<uint8_t const*>(&LocalBuffer[0]));
  uint8_t* p = (uint8_t*)&LocalBuffer[1];
  *p++ = (uint8_t)'f';
  *p++ = (uint8_t)'o';
  *p++ = (uint8_t)'o';
  *p++ = (uint8_t)'b';
  *p++ = (uint8_t)'a';
  *p++ = (uint8_t)'r';

  ASSERT_EQ(ValueType::String, slice.type());
  ASSERT_TRUE(slice.isString());
  ASSERT_EQ(7ULL, slice.byteSize());
  ValueLength len;
  char const* s = slice.getString(len);
  ASSERT_EQ(6ULL, len);
  ASSERT_EQ(0, strncmp(s, "foobar", len));

  ASSERT_EQ(strlen("foobar"), slice.getStringLength());
  ASSERT_EQ("foobar", slice.copyString());
  ASSERT_TRUE(StringRef("foobar").equals(slice.stringRef()));
}

TEST(SliceTest, String2) {
  LocalBuffer[0] = 0x48;

  Slice slice(reinterpret_cast<uint8_t const*>(&LocalBuffer[0]));
  uint8_t* p = (uint8_t*)&LocalBuffer[1];
  *p++ = (uint8_t)'1';
  *p++ = (uint8_t)'2';
  *p++ = (uint8_t)'3';
  *p++ = (uint8_t)'f';
  *p++ = (uint8_t)'\r';
  *p++ = (uint8_t)'\t';
  *p++ = (uint8_t)'\n';
  *p++ = (uint8_t)'x';

  ASSERT_EQ(ValueType::String, slice.type());
  ASSERT_TRUE(slice.isString());
  ASSERT_EQ(9ULL, slice.byteSize());
  ValueLength len;
  char const* s = slice.getString(len);
  ASSERT_EQ(8ULL, len);
  ASSERT_EQ(0, strncmp(s, "123f\r\t\nx", len));
  ASSERT_EQ(8U, slice.getStringLength());

  ASSERT_EQ("123f\r\t\nx", slice.copyString());
  ASSERT_TRUE(StringRef("123f\r\t\nx").equals(slice.stringRef()));
}

TEST(SliceTest, StringNullBytes) {
  LocalBuffer[0] = 0x48;

  Slice slice(reinterpret_cast<uint8_t const*>(&LocalBuffer[0]));
  uint8_t* p = (uint8_t*)&LocalBuffer[1];
  *p++ = (uint8_t)'\0';
  *p++ = (uint8_t)'1';
  *p++ = (uint8_t)'2';
  *p++ = (uint8_t)'\0';
  *p++ = (uint8_t)'3';
  *p++ = (uint8_t)'4';
  *p++ = (uint8_t)'\0';
  *p++ = (uint8_t)'x';

  ASSERT_EQ(ValueType::String, slice.type());
  ASSERT_TRUE(slice.isString());
  ASSERT_EQ(9ULL, slice.byteSize());
  ValueLength len;
  slice.getString(len);
  ASSERT_EQ(8ULL, len);
  ASSERT_EQ(8U, slice.getStringLength());

  {
    std::string s(slice.copyString());
    ASSERT_EQ(8ULL, s.size());
    ASSERT_EQ('\0', s[0]);
    ASSERT_EQ('1', s[1]);
    ASSERT_EQ('2', s[2]);
    ASSERT_EQ('\0', s[3]);
    ASSERT_EQ('3', s[4]);
    ASSERT_EQ('4', s[5]);
    ASSERT_EQ('\0', s[6]);
    ASSERT_EQ('x', s[7]);
  }

  {
    StringRef s(slice.stringRef());
    ASSERT_EQ(8ULL, s.size());
    ASSERT_EQ('\0', s[0]);
    ASSERT_EQ('1', s[1]);
    ASSERT_EQ('2', s[2]);
    ASSERT_EQ('\0', s[3]);
    ASSERT_EQ('3', s[4]);
    ASSERT_EQ('4', s[5]);
    ASSERT_EQ('\0', s[6]);
    ASSERT_EQ('x', s[7]);
  }
}

TEST(SliceTest, StringLong) {
  LocalBuffer[0] = 0xbf;

  Slice slice(reinterpret_cast<uint8_t const*>(&LocalBuffer[0]));
  uint8_t* p = (uint8_t*)&LocalBuffer[1];
  // length
  *p++ = (uint8_t)6;
  *p++ = (uint8_t)0;
  *p++ = (uint8_t)0;
  *p++ = (uint8_t)0;
  *p++ = (uint8_t)0;
  *p++ = (uint8_t)0;
  *p++ = (uint8_t)0;
  *p++ = (uint8_t)0;

  *p++ = (uint8_t)'f';
  *p++ = (uint8_t)'o';
  *p++ = (uint8_t)'o';
  *p++ = (uint8_t)'b';
  *p++ = (uint8_t)'a';
  *p++ = (uint8_t)'r';

  ASSERT_EQ(ValueType::String, slice.type());
  ASSERT_TRUE(slice.isString());
  ASSERT_EQ(15ULL, slice.byteSize());
  ValueLength len;
  char const* s = slice.getString(len);
  ASSERT_EQ(6ULL, len);
  ASSERT_EQ(0, strncmp(s, "foobar", len));
  ASSERT_EQ(6U, slice.getStringLength());

  ASSERT_EQ("foobar", slice.copyString());
  ASSERT_TRUE(StringRef("foobar").equals(slice.stringRef()));
}

TEST(SliceTest, BinaryEmpty) {
  uint8_t buf[] = {0xc0, 0x00};
  Slice slice(&buf[0]);

  ASSERT_TRUE(slice.isBinary());
  ValueLength len;
  slice.getBinary(len);
  ASSERT_EQ(0ULL, len);
  ASSERT_EQ(0U, slice.getBinaryLength());
  auto result = slice.copyBinary();
  ASSERT_EQ(0UL, result.size());
}

TEST(SliceTest, BinarySomeValue) {
  uint8_t buf[] = {0xc0, 0x05, 0xfe, 0xfd, 0xfc, 0xfb, 0xfa};
  Slice slice(&buf[0]);

  ASSERT_TRUE(slice.isBinary());
  ValueLength len;
  uint8_t const* s = slice.getBinary(len);
  ASSERT_EQ(5ULL, len);
  ASSERT_EQ(0, memcmp(s, &buf[2], len));
  ASSERT_EQ(5U, slice.getBinaryLength());

  auto result = slice.copyBinary();
  ASSERT_EQ(5UL, result.size());
  ASSERT_EQ(0xfe, result[0]);
  ASSERT_EQ(0xfd, result[1]);
  ASSERT_EQ(0xfc, result[2]);
  ASSERT_EQ(0xfb, result[3]);
  ASSERT_EQ(0xfa, result[4]);
}

TEST(SliceTest, BinaryWithNullBytes) {
  uint8_t buf[] = {0xc0, 0x05, 0x01, 0x02, 0x00, 0x03, 0x00};
  Slice slice(&buf[0]);

  ASSERT_TRUE(slice.isBinary());
  ValueLength len;
  uint8_t const* s = slice.getBinary(len);
  ASSERT_EQ(5ULL, len);
  ASSERT_EQ(0, memcmp(s, &buf[2], len));
  ASSERT_EQ(5U, slice.getBinaryLength());

  auto result = slice.copyBinary();
  ASSERT_EQ(5UL, result.size());
  ASSERT_EQ(0x01, result[0]);
  ASSERT_EQ(0x02, result[1]);
  ASSERT_EQ(0x00, result[2]);
  ASSERT_EQ(0x03, result[3]);
  ASSERT_EQ(0x00, result[4]);
}

TEST(SliceTest, BinaryNonBinary) {
  Slice slice;

  ValueLength len;
  ASSERT_VELOCYPACK_EXCEPTION(slice.getBinary(len),
                              Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(slice.getBinaryLength(),
                              Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(slice.copyBinary(), Exception::InvalidValueType);
}

TEST(SliceTest, ArrayCases1) {
  uint8_t buf[] = {0x02, 0x05, 0x31, 0x32, 0x33};
  Slice s(buf);
  ASSERT_TRUE(s.isArray());
  ASSERT_FALSE(s.isEmptyArray());
  ASSERT_EQ(3ULL, s.length());
  ASSERT_EQ(sizeof(buf), s.byteSize());
  Slice ss = s[0];
  ASSERT_TRUE(ss.isSmallInt());
  ASSERT_EQ(1LL, ss.getInt());
}

TEST(SliceTest, ArrayCases2) {
  uint8_t buf[] = {0x02, 0x06, 0x00, 0x31, 0x32, 0x33};
  Slice s(buf);
  ASSERT_TRUE(s.isArray());
  ASSERT_FALSE(s.isEmptyArray());
  ASSERT_EQ(3ULL, s.length());
  ASSERT_EQ(sizeof(buf), s.byteSize());
  Slice ss = s[0];
  ASSERT_TRUE(ss.isSmallInt());
  ASSERT_EQ(1LL, ss.getInt());
}

TEST(SliceTest, ArrayCases3) {
  uint8_t buf[] = {0x02, 0x08, 0x00, 0x00, 0x00, 0x31, 0x32, 0x33};
  Slice s(buf);
  ASSERT_TRUE(s.isArray());
  ASSERT_FALSE(s.isEmptyArray());
  ASSERT_EQ(3ULL, s.length());
  ASSERT_EQ(sizeof(buf), s.byteSize());
  Slice ss = s[0];
  ASSERT_TRUE(ss.isSmallInt());
  ASSERT_EQ(1LL, ss.getInt());
}

TEST(SliceTest, ArrayCases4) {
  uint8_t buf[] = {0x02, 0x0c, 0x00, 0x00, 0x00, 0x00,
                   0x00, 0x00, 0x00, 0x31, 0x32, 0x33};
  Slice s(buf);
  ASSERT_TRUE(s.isArray());
  ASSERT_FALSE(s.isEmptyArray());
  ASSERT_EQ(3ULL, s.length());
  ASSERT_EQ(sizeof(buf), s.byteSize());
  Slice ss = s[0];
  ASSERT_TRUE(ss.isSmallInt());
  ASSERT_EQ(1LL, ss.getInt());
}

TEST(SliceTest, ArrayCases5) {
  uint8_t buf[] = {0x03, 0x06, 0x00, 0x31, 0x32, 0x33};
  Slice s(buf);
  ASSERT_TRUE(s.isArray());
  ASSERT_FALSE(s.isEmptyArray());
  ASSERT_EQ(3ULL, s.length());
  ASSERT_EQ(sizeof(buf), s.byteSize());
  Slice ss = s[0];
  ASSERT_TRUE(ss.isSmallInt());
  ASSERT_EQ(1LL, ss.getInt());
}

TEST(SliceTest, ArrayCases6) {
  uint8_t buf[] = {0x03, 0x08, 0x00, 0x00, 0x00, 0x31, 0x32, 0x33};
  Slice s(buf);
  ASSERT_TRUE(s.isArray());
  ASSERT_FALSE(s.isEmptyArray());
  ASSERT_EQ(3ULL, s.length());
  ASSERT_EQ(sizeof(buf), s.byteSize());
  Slice ss = s[0];
  ASSERT_TRUE(ss.isSmallInt());
  ASSERT_EQ(1LL, ss.getInt());
}

TEST(SliceTest, ArrayCases7) {
  uint8_t buf[] = {0x03, 0x0c, 0x00, 0x00, 0x00, 0x00,
                   0x00, 0x00, 0x00, 0x31, 0x32, 0x33};
  Slice s(buf);
  ASSERT_TRUE(s.isArray());
  ASSERT_FALSE(s.isEmptyArray());
  ASSERT_EQ(3ULL, s.length());
  ASSERT_EQ(sizeof(buf), s.byteSize());
  Slice ss = s[0];
  ASSERT_TRUE(ss.isSmallInt());
  ASSERT_EQ(1LL, ss.getInt());
}

TEST(SliceTest, ArrayCases8) {
  uint8_t buf[] = {0x04, 0x08, 0x00, 0x00, 0x00, 0x31, 0x32, 0x33};
  Slice s(buf);
  ASSERT_TRUE(s.isArray());
  ASSERT_FALSE(s.isEmptyArray());
  ASSERT_EQ(3ULL, s.length());
  ASSERT_EQ(sizeof(buf), s.byteSize());
  Slice ss = s[0];
  ASSERT_TRUE(ss.isSmallInt());
  ASSERT_EQ(1LL, ss.getInt());
}

TEST(SliceTest, ArrayCases9) {
  uint8_t buf[] = {0x04, 0x0c, 0x00, 0x00, 0x00, 0x00,
                   0x00, 0x00, 0x00, 0x31, 0x32, 0x33};
  Slice s(buf);
  ASSERT_TRUE(s.isArray());
  ASSERT_FALSE(s.isEmptyArray());
  ASSERT_EQ(3ULL, s.length());
  ASSERT_EQ(sizeof(buf), s.byteSize());
  Slice ss = s[0];
  ASSERT_TRUE(ss.isSmallInt());
  ASSERT_EQ(1LL, ss.getInt());
}

TEST(SliceTest, ArrayCases10) {
  uint8_t buf[] = {0x05, 0x0c, 0x00, 0x00, 0x00, 0x00,
                   0x00, 0x00, 0x00, 0x31, 0x32, 0x33};
  Slice s(buf);
  ASSERT_TRUE(s.isArray());
  ASSERT_FALSE(s.isEmptyArray());
  ASSERT_EQ(3ULL, s.length());
  ASSERT_EQ(sizeof(buf), s.byteSize());
  Slice ss = s[0];
  ASSERT_TRUE(ss.isSmallInt());
  ASSERT_EQ(1LL, ss.getInt());
}

TEST(SliceTest, ArrayCases11) {
  uint8_t buf[] = {0x06, 0x09, 0x03, 0x31, 0x32, 0x33, 0x03, 0x04, 0x05};
  Slice s(buf);
  ASSERT_TRUE(s.isArray());
  ASSERT_FALSE(s.isEmptyArray());
  ASSERT_EQ(3ULL, s.length());
  ASSERT_EQ(sizeof(buf), s.byteSize());
  Slice ss = s[0];
  ASSERT_TRUE(ss.isSmallInt());
  ASSERT_EQ(1LL, ss.getInt());
}

TEST(SliceTest, ArrayCases12) {
  uint8_t buf[] = {0x06, 0x0b, 0x03, 0x00, 0x00, 0x31,
                   0x32, 0x33, 0x05, 0x06, 0x07};
  Slice s(buf);
  ASSERT_TRUE(s.isArray());
  ASSERT_FALSE(s.isEmptyArray());
  ASSERT_EQ(3ULL, s.length());
  ASSERT_EQ(sizeof(buf), s.byteSize());
  Slice ss = s[0];
  ASSERT_TRUE(ss.isSmallInt());
  ASSERT_EQ(1LL, ss.getInt());
}

TEST(SliceTest, ArrayCases13) {
  uint8_t buf[] = {0x06, 0x0f, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
                   0x00, 0x31, 0x32, 0x33, 0x09, 0x0a, 0x0b};
  Slice s(buf);
  ASSERT_TRUE(s.isArray());
  ASSERT_FALSE(s.isEmptyArray());
  ASSERT_EQ(3ULL, s.length());
  ASSERT_EQ(sizeof(buf), s.byteSize());
  Slice ss = s[0];
  ASSERT_TRUE(ss.isSmallInt());
  ASSERT_EQ(1LL, ss.getInt());
}

TEST(SliceTest, ArrayCases14) {
  uint8_t buf[] = {0x07, 0x0e, 0x00, 0x03, 0x00, 0x31, 0x32,
                   0x33, 0x05, 0x00, 0x06, 0x00, 0x07, 0x00};
  Slice s(buf);
  ASSERT_TRUE(s.isArray());
  ASSERT_FALSE(s.isEmptyArray());
  ASSERT_EQ(3ULL, s.length());
  ASSERT_EQ(sizeof(buf), s.byteSize());
  Slice ss = s[0];
  ASSERT_TRUE(ss.isSmallInt());
  ASSERT_EQ(1LL, ss.getInt());
}

TEST(SliceTest, ArrayCases15) {
  uint8_t buf[] = {0x07, 0x12, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
                   0x31, 0x32, 0x33, 0x09, 0x00, 0x0a, 0x00, 0x0b, 0x00};
  Slice s(buf);
  ASSERT_TRUE(s.isArray());
  ASSERT_FALSE(s.isEmptyArray());
  ASSERT_EQ(3ULL, s.length());
  ASSERT_EQ(sizeof(buf), s.byteSize());
  Slice ss = s[0];
  ASSERT_TRUE(ss.isSmallInt());
  ASSERT_EQ(1LL, ss.getInt());
}

TEST(SliceTest, ArrayCases16) {
  uint8_t buf[] = {0x08, 0x18, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00,
                   0x00, 0x31, 0x32, 0x33, 0x09, 0x00, 0x00, 0x00,
                   0x0a, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00};
  Slice s(buf);
  ASSERT_TRUE(s.isArray());
  ASSERT_FALSE(s.isEmptyArray());
  ASSERT_EQ(3ULL, s.length());
  ASSERT_EQ(sizeof(buf), s.byteSize());
  Slice ss = s[0];
  ASSERT_TRUE(ss.isSmallInt());
  ASSERT_EQ(1LL, ss.getInt());
}

TEST(SliceTest, ArrayCases17) {
  uint8_t buf[] = {0x09, 0x2c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                   0x31, 0x32, 0x33, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00,
                   0x00, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                   0x00, 0x0b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                   0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  Slice s(buf);
  ASSERT_TRUE(s.isArray());
  ASSERT_FALSE(s.isEmptyArray());
  ASSERT_EQ(3ULL, s.length());
  ASSERT_EQ(sizeof(buf), s.byteSize());
  Slice ss = s[0];
  ASSERT_TRUE(ss.isSmallInt());
  ASSERT_EQ(1LL, ss.getInt());
}

TEST(SliceTest, ArrayCasesCompact) {
  uint8_t buf[] = {0x13, 0x08, 0x30, 0x31, 0x32, 0x33, 0x34, 0x05};

  Slice s(buf);
  ASSERT_TRUE(s.isArray());
  ASSERT_FALSE(s.isEmptyArray());
  ASSERT_EQ(5ULL, s.length());
  ASSERT_EQ(sizeof(buf), s.byteSize());
  Slice ss = s[0];
  ASSERT_TRUE(ss.isSmallInt());
  ASSERT_EQ(0LL, ss.getInt());
  ss = s[1];
  ASSERT_TRUE(ss.isSmallInt());
  ASSERT_EQ(1LL, ss.getInt());
  ss = s[4];
  ASSERT_TRUE(ss.isSmallInt());
  ASSERT_EQ(4LL, ss.getInt());
}

TEST(SliceTest, ObjectCases1) {
  uint8_t buf[] = {0x0b, 0x00, 0x03, 0x41, 0x61, 0x31, 0x41, 0x62,
                   0x32, 0x41, 0x63, 0x33, 0x03, 0x06, 0x09};
  buf[1] = sizeof(buf);  // set the bytelength
  Slice s(buf);
  ASSERT_TRUE(s.isObject());
  ASSERT_FALSE(s.isEmptyObject());
  ASSERT_EQ(3ULL, s.length());
  ASSERT_EQ(sizeof(buf), s.byteSize());
  Slice ss = s["a"];
  ASSERT_TRUE(ss.isSmallInt());
  ASSERT_EQ(1LL, ss.getInt());
}

TEST(SliceTest, ObjectCases2) {
  uint8_t buf[] = {0x0b, 0x00, 0x03, 0x00, 0x00, 0x41, 0x61, 0x31, 0x41,
                   0x62, 0x32, 0x41, 0x63, 0x33, 0x05, 0x08, 0x0b};
  buf[1] = sizeof(buf);  // set the bytelength
  Slice s(buf);
  ASSERT_TRUE(s.isObject());
  ASSERT_FALSE(s.isEmptyObject());
  ASSERT_EQ(3ULL, s.length());
  ASSERT_EQ(sizeof(buf), s.byteSize());
  Slice ss = s["a"];
  ASSERT_TRUE(ss.isSmallInt());
  ASSERT_EQ(1LL, ss.getInt());
}

TEST(SliceTest, ObjectCases3) {
  uint8_t buf[] = {0x0b, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00,
                   0x00, 0x00, 0x41, 0x61, 0x31, 0x41, 0x62,
                   0x32, 0x41, 0x63, 0x33, 0x09, 0x0c, 0x0f};
  buf[1] = sizeof(buf);  // set the bytelength
  Slice s(buf);
  ASSERT_TRUE(s.isObject());
  ASSERT_FALSE(s.isEmptyObject());
  ASSERT_EQ(3ULL, s.length());
  ASSERT_EQ(sizeof(buf), s.byteSize());
  Slice ss = s["a"];
  ASSERT_TRUE(ss.isSmallInt());
  ASSERT_EQ(1LL, ss.getInt());
}

TEST(SliceTest, ObjectCases7) {
  uint8_t buf[] = {0x0c, 0x00, 0x00, 0x03, 0x00, 0x41, 0x61, 0x31, 0x41, 0x62,
                   0x32, 0x41, 0x63, 0x33, 0x05, 0x00, 0x08, 0x00, 0x0b, 0x00};
  buf[1] = sizeof(buf);  // set the bytelength
  Slice s(buf);
  ASSERT_TRUE(s.isObject());
  ASSERT_FALSE(s.isEmptyObject());
  ASSERT_EQ(3ULL, s.length());
  ASSERT_EQ(sizeof(buf), s.byteSize());
  Slice ss = s["a"];
  ASSERT_TRUE(ss.isSmallInt());
  ASSERT_EQ(1LL, ss.getInt());
}

TEST(SliceTest, ObjectCases8) {
  uint8_t buf[] = {0x0c, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00,
                   0x00, 0x41, 0x61, 0x31, 0x41, 0x62, 0x32, 0x41,
                   0x63, 0x33, 0x09, 0x00, 0x0c, 0x00, 0x0f, 0x00};
  buf[1] = sizeof(buf);  // set the bytelength
  Slice s(buf);
  ASSERT_TRUE(s.isObject());
  ASSERT_FALSE(s.isEmptyObject());
  ASSERT_EQ(3ULL, s.length());
  ASSERT_EQ(sizeof(buf), s.byteSize());
  Slice ss = s["a"];
  ASSERT_TRUE(ss.isSmallInt());
  ASSERT_EQ(1LL, ss.getInt());
}

TEST(SliceTest, ObjectCases11) {
  uint8_t buf[] = {0x0d, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x41,
                   0x61, 0x31, 0x41, 0x62, 0x32, 0x41, 0x63, 0x33, 0x09, 0x00,
                   0x00, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00};
  buf[1] = sizeof(buf);  // set the bytelength
  Slice s(buf);
  ASSERT_TRUE(s.isObject());
  ASSERT_FALSE(s.isEmptyObject());
  ASSERT_EQ(3ULL, s.length());
  ASSERT_EQ(sizeof(buf), s.byteSize());
  Slice ss = s["a"];
  ASSERT_TRUE(ss.isSmallInt());
  ASSERT_EQ(1LL, ss.getInt());
}

TEST(SliceTest, ObjectCases13) {
  uint8_t buf[] = {0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x41,
                   0x61, 0x31, 0x41, 0x62, 0x32, 0x41, 0x63, 0x33, 0x09, 0x00,
                   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00,
                   0x00, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00,
                   0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  buf[1] = sizeof(buf);  // set the bytelength
  Slice s(buf);
  ASSERT_TRUE(s.isObject());
  ASSERT_FALSE(s.isEmptyObject());
  ASSERT_EQ(3ULL, s.length());
  ASSERT_EQ(sizeof(buf), s.byteSize());
  Slice ss = s["a"];
  ASSERT_TRUE(ss.isSmallInt());
  ASSERT_EQ(1LL, ss.getInt());
}

TEST(SliceTest, ObjectCompact) {
  uint8_t const buf[] = {0x14, 0x0f, 0x41, 0x61, 0x30, 0x41, 0x62, 0x31,
                         0x41, 0x63, 0x32, 0x41, 0x64, 0x33, 0x04};
  Slice s(buf);
  ASSERT_TRUE(s.isObject());
  ASSERT_FALSE(s.isEmptyObject());
  ASSERT_EQ(4ULL, s.length());
  ASSERT_EQ(sizeof(buf), s.byteSize());
  Slice ss = s["a"];
  ASSERT_TRUE(ss.isSmallInt());
  ASSERT_EQ(0LL, ss.getInt());
  ss = s["b"];
  ASSERT_TRUE(ss.isSmallInt());
  ASSERT_EQ(1LL, ss.getInt());
  ss = s["d"];
  ASSERT_TRUE(ss.isSmallInt());
  ASSERT_EQ(3LL, ss.getInt());
}

TEST(SliceTest, ToStringNull) {
  std::string const value("null");

  std::shared_ptr<Builder> b = Parser::fromJson(value);
  Slice s(b->start());

  ASSERT_EQ("null", s.toString());
}

TEST(SliceTest, ToStringArray) {
  std::string const value("[1,2,3,4,5]");

  std::shared_ptr<Builder> b = Parser::fromJson(value);
  Slice s(b->start());

  ASSERT_EQ("[\n  1,\n  2,\n  3,\n  4,\n  5\n]", s.toString());
}

TEST(SliceTest, ToStringArrayCompact) {
  Options options;
  options.buildUnindexedArrays = true;

  std::string const value("[1,2,3,4,5]");

  std::shared_ptr<Builder> b = Parser::fromJson(value, &options);
  Slice s(b->start());

  ASSERT_EQ(0x13, s.head());
  ASSERT_EQ("[\n  1,\n  2,\n  3,\n  4,\n  5\n]", s.toString());
}

TEST(SliceTest, ToStringArrayEmpty) {
  std::string const value("[]");

  std::shared_ptr<Builder> b = Parser::fromJson(value);
  Slice s(b->start());

  ASSERT_EQ("[\n]", s.toString());
}

TEST(SliceTest, ToStringObjectEmpty) {
  std::string const value("{ }");

  std::shared_ptr<Builder> b = Parser::fromJson(value);
  Slice s(b->start());

  ASSERT_EQ("{\n}", s.toString());
}

TEST(SliceTest, EqualToUniqueValues) {
  std::string const value("[1,2,3,4,null,true,\"foo\",\"bar\"]");

  Parser parser;
  parser.parse(value);

  std::unordered_set<Slice, NormalizedCompare::Hash, NormalizedCompare::Equal> values(11, NormalizedCompare::Hash(), NormalizedCompare::Equal());
  for (auto it : ArrayIterator(Slice(parser.start()))) {
    values.emplace(it);
  }

  ASSERT_EQ(8UL, values.size());
}

TEST(SliceTest, EqualToDuplicateValuesNumbers) {
  std::string const value("[1,2,3,4,1,2,3,4,5,9,1]");

  Parser parser;
  parser.parse(value);

  std::unordered_set<Slice, NormalizedCompare::Hash, NormalizedCompare::Equal> values(11, NormalizedCompare::Hash(), NormalizedCompare::Equal());
  for (auto it : ArrayIterator(Slice(parser.start()))) {
    values.emplace(it);
  }

  ASSERT_EQ(6UL, values.size());  // 1,2,3,4,5,9
}

TEST(SliceTest, EqualToBiggerNumbers) {
  std::string const value("[1024,1025,1031,1024,1029,1025]");

  Parser parser;
  parser.parse(value);

  std::unordered_set<Slice, NormalizedCompare::Hash, NormalizedCompare::Equal> values(11, NormalizedCompare::Hash(), NormalizedCompare::Equal());
  for (auto it : ArrayIterator(Slice(parser.start()))) {
    values.emplace(it);
  }

  ASSERT_EQ(4UL, values.size());  // 1024, 1025, 1029, 1031
}

TEST(SliceTest, EqualToDuplicateValuesStrings) {
  std::string const value(
      "[\"foo\",\"bar\",\"baz\",\"bart\",\"foo\",\"bark\",\"qux\",\"foo\"]");

  Parser parser;
  parser.parse(value);

  std::unordered_set<Slice, NormalizedCompare::Hash, NormalizedCompare::Equal> values(11, NormalizedCompare::Hash(), NormalizedCompare::Equal());
  for (auto it : ArrayIterator(Slice(parser.start()))) {
    values.emplace(it);
  }

  ASSERT_EQ(6UL, values.size());  // "foo", "bar", "baz", "bart", "bark", "qux"
}

TEST(SliceTest, EqualToNull) {
  std::shared_ptr<Builder> b1 = Parser::fromJson("null");
  Slice s1 = b1->slice();
  std::shared_ptr<Builder> b2 = Parser::fromJson("null");
  Slice s2 = b2->slice();

  ASSERT_TRUE(s1.binaryEquals(s2));
  ASSERT_TRUE(s2.binaryEquals(s1));
}

TEST(SliceTest, EqualToInt) {
  std::shared_ptr<Builder> b1 = Parser::fromJson("-128885355");
  Slice s1 = b1->slice();
  std::shared_ptr<Builder> b2 = Parser::fromJson("-128885355");
  Slice s2 = b2->slice();

  ASSERT_TRUE(s1.binaryEquals(s2));
  ASSERT_TRUE(s2.binaryEquals(s1));
}

TEST(SliceTest, EqualToUInt) {
  std::shared_ptr<Builder> b1 = Parser::fromJson("128885355");
  Slice s1 = b1->slice();
  std::shared_ptr<Builder> b2 = Parser::fromJson("128885355");
  Slice s2 = b2->slice();

  ASSERT_TRUE(s1.binaryEquals(s2));
  ASSERT_TRUE(s2.binaryEquals(s1));
}

TEST(SliceTest, EqualToDouble) {
  std::shared_ptr<Builder> b1 = Parser::fromJson("-128885355.353");
  Slice s1 = b1->slice();
  std::shared_ptr<Builder> b2 = Parser::fromJson("-128885355.353");
  Slice s2 = b2->slice();

  ASSERT_TRUE(s1.binaryEquals(s2));
  ASSERT_TRUE(s2.binaryEquals(s1));
}

TEST(SliceTest, EqualToString) {
  std::shared_ptr<Builder> b1 = Parser::fromJson("\"this is a test string\"");
  Slice s1 = b1->slice();
  std::shared_ptr<Builder> b2 = Parser::fromJson("\"this is a test string\"");
  Slice s2 = b2->slice();

  ASSERT_TRUE(s1.binaryEquals(s2));
  ASSERT_TRUE(s2.binaryEquals(s1));
}

TEST(SliceTest, EqualToDirectInvocation) {
  std::string const value("[1024,1025,1026,1027,1028]");

  Parser parser;
  parser.parse(value);

  int comparisons = 0;
  ArrayIterator it(Slice(parser.start()));
  while (it.valid()) {
    ArrayIterator it2(Slice(parser.start()));
    while (it2.valid()) {
      if (it.index() != it2.index()) {
        ASSERT_FALSE(it.value().binaryEquals(it2.value()));
        ASSERT_FALSE(it2.value().binaryEquals(it.value()));
        ++comparisons;
      }
      it2.next();
    }
    it.next();
  }
  ASSERT_EQ(20, comparisons);
}

TEST(SliceTest, EqualToDirectInvocationSmallInts) {
  std::string const value("[1,2,3,4,5]");

  Parser parser;
  parser.parse(value);

  int comparisons = 0;
  ArrayIterator it(Slice(parser.start()));
  while (it.valid()) {
    ArrayIterator it2(Slice(parser.start()));
    while (it2.valid()) {
      if (it.index() != it2.index()) {
        ASSERT_FALSE(it.value().binaryEquals(it2.value()));
        ASSERT_FALSE(it2.value().binaryEquals(it.value()));
        ++comparisons;
      }
      it2.next();
    }
    it.next();
  }
  ASSERT_EQ(20, comparisons);
}

TEST(SliceTest, EqualToDirectInvocationLongStrings) {
  std::shared_ptr<Builder> b1 = Parser::fromJson(
      "\"thisisalongstring.dddddddddddddddddddddddddddds......................."
      "....................................................."
      "longerthan127chars\"");
  std::shared_ptr<Builder> b2 = Parser::fromJson(
      "\"thisisalongstring.dddddddddddddddddddddddddddds.................eek!.."
      "........................................................."
      "longerthan127chars\"");

  ASSERT_TRUE(b1->slice().binaryEquals(b1->slice()));
  ASSERT_TRUE(b2->slice().binaryEquals(b2->slice()));
  ASSERT_FALSE(b1->slice().binaryEquals(b2->slice()));
  ASSERT_FALSE(b2->slice().binaryEquals(b1->slice()));
}

TEST(SliceTest, Hashing) {
  for (std::size_t i = 0; i < 256; ++i) {
    if (SliceStaticData::FixedTypeLengths[i] != 1) {
      // not a one-byte type
      continue;
    }
    Builder b;
    uint8_t val = static_cast<uint8_t>(i);
    b.add(Slice(&val));

    ASSERT_EQ(SliceStaticData::PrecalculatedHashesForDefaultSeed[i], b.slice().hash());
    ASSERT_EQ(SliceStaticData::PrecalculatedHashesForDefaultSeed[i], b.slice().hashSlow());
    ASSERT_EQ(SliceStaticData::PrecalculatedHashesForDefaultSeed[i], b.slice().hash(Slice::defaultSeed64));
    ASSERT_EQ(SliceStaticData::PrecalculatedHashesForDefaultSeed[i], b.slice().hashSlow(Slice::defaultSeed64));
  }
}

#ifdef VELOCYPACK_XXHASH

TEST(SliceTest, HashNull) {
  std::shared_ptr<Builder> b = Parser::fromJson("null");
  Slice s = b->slice();

  ASSERT_EQ(1903446559881298698ULL, s.hash());
  ASSERT_EQ(1903446559881298698ULL, s.normalizedHash());
}

TEST(SliceTest, HashDouble) {
  std::shared_ptr<Builder> b = Parser::fromJson("-345354.35532352");
  Slice s = b->slice();

  ASSERT_EQ(4457948945193834531ULL, s.hash());
  ASSERT_EQ(9737553273926297610ULL, s.normalizedHash());
}

TEST(SliceTest, HashString) {
  std::shared_ptr<Builder> b = Parser::fromJson("\"this is a test string\"");
  Slice s = b->slice();

  ASSERT_EQ(9531555437566788706ULL, s.hash());
  ASSERT_EQ(9531555437566788706ULL, s.normalizedHash());
}

TEST(SliceTest, HashStringEmpty) {
  std::shared_ptr<Builder> b = Parser::fromJson("\"\"");
  Slice s = b->slice();

  ASSERT_EQ(1450600894602296270ULL, s.hash());
  ASSERT_EQ(1450600894602296270ULL, s.normalizedHash());
}

TEST(SliceTest, HashStringShort) {
  std::shared_ptr<Builder> b = Parser::fromJson("\"123456\"");
  Slice s = b->slice();

  ASSERT_EQ(14855108345558666872ULL, s.hash());
  ASSERT_EQ(14855108345558666872ULL, s.normalizedHash());
  ASSERT_EQ(14855108345558666872ULL, s.hashString());
}

TEST(SliceTest, HashArray) {
  std::shared_ptr<Builder> b = Parser::fromJson("[1,2,3,4,5,6,7,8,9,10]");
  Slice s = b->slice();

  ASSERT_EQ(8308483934453544580ULL, s.hash());
}

TEST(SliceTest, HashObject) {
  std::shared_ptr<Builder> b = Parser::fromJson(
      "{\"one\":1,\"two\":2,\"three\":3,\"four\":4,\"five\":5,\"six\":6,"
      "\"seven\":7}");
  Slice s = b->slice();

  ASSERT_EQ(8873752126133306149ULL, s.hash());
}

TEST(SliceTest, NormalizedHashDouble) {
  Builder b1;
  b1.openArray();
  b1.add(Value(-1.0));
  b1.add(Value(0));
  b1.add(Value(1.0));
  b1.add(Value(2.0));
  b1.add(Value(3.0));
  b1.add(Value(42.0));
  b1.add(Value(-42.0));
  b1.add(Value(123456.0));
  b1.add(Value(-123456.0));
  b1.close();

  Builder b2;
  b2.openArray();
  b2.add(Value(-1));
  b2.add(Value(0));
  b2.add(Value(1));
  b2.add(Value(2));
  b2.add(Value(3));
  b2.add(Value(42));
  b2.add(Value(-42));
  b2.add(Value(123456));
  b2.add(Value(-123456));
  b2.close();

  // hash values differ, but normalized hash values shouldn't!
  ASSERT_EQ(7102974787002861403ULL, b1.slice().hash());
  ASSERT_EQ(14248638626330948552ULL, b2.slice().hash());

  ASSERT_EQ(13690116699997059692ULL, b1.slice().normalizedHash());
  ASSERT_EQ(13690116699997059692ULL, b2.slice().normalizedHash());
}

TEST(SliceTest, NormalizedHashArray) {
  Options options;

  options.buildUnindexedArrays = false;
  std::shared_ptr<Builder> b1 = Parser::fromJson("[1,2,3,4,5,6,7,8,9,10]", &options);
  Slice s1 = b1->slice();

  options.buildUnindexedArrays = true;
  std::shared_ptr<Builder> b2 = Parser::fromJson("[1,2,3,4,5,6,7,8,9,10]", &options);
  Slice s2 = b2->slice();

  // hash values differ, but normalized hash values shouldn't!
  ASSERT_EQ(8308483934453544580ULL, s1.hash());
  ASSERT_EQ(15333913616940129336ULL, s2.hash());

  ASSERT_EQ(1025874842406722974ULL, s1.normalizedHash());
  ASSERT_EQ(1025874842406722974ULL, s2.normalizedHash());
}

TEST(SliceTest, NormalizedHashArrayNested) {
  Options options;

  options.buildUnindexedArrays = false;
  std::shared_ptr<Builder> b1 = Parser::fromJson("[-4.0,1,2.0,-4345.0,4,5,6,7,8,9,10,[1,9,-42,45.0]]", &options);
  Slice s1 = b1->slice();

  options.buildUnindexedArrays = true;
  std::shared_ptr<Builder> b2 = Parser::fromJson("[-4.0,1,2.0,-4345.0,4,5,6,7,8,9,10,[1,9,-42,45.0]]", &options);
  Slice s2 = b2->slice();

  // hash values differ, but normalized hash values shouldn't!
  ASSERT_EQ(15793061464938738924ULL, s1.hash());
  ASSERT_EQ(2722569323545975071ULL, s2.hash());

  ASSERT_EQ(11101500731480543049ULL, s1.normalizedHash());
  ASSERT_EQ(11101500731480543049ULL, s2.normalizedHash());
}

TEST(SliceTest, NormalizedHashObjectOrder) {
  Options options;

  options.buildUnindexedObjects = false;
  std::shared_ptr<Builder> b1 = Parser::fromJson(
      "{\"one\":1,\"two\":2,\"three\":3,\"four\":4,\"five\":5,\"six\":6,"
      "\"seven\":7}", &options);
  Slice s1 = b1->slice();

  options.buildUnindexedObjects = false;
  std::shared_ptr<Builder> b2 = Parser::fromJson(
      "{\"seven\":7,\"six\":6,\"five\":5,\"four\":4,\"three\":3,\"two\":2,\"one\":1}", &options);
  Slice s2 = b2->slice();

  // hash values differ, but normalized hash values shouldn't!
  ASSERT_EQ(8873752126133306149ULL, s1.hash());
  ASSERT_EQ(9972002797221051811ULL, s2.hash());

  ASSERT_EQ(724735390467908482ULL, s1.normalizedHash());
  ASSERT_EQ(724735390467908482ULL, s2.normalizedHash());
}

#endif

#ifdef VELOCYPACK_WYHASH

TEST(SliceTest, HashNull) {
  std::shared_ptr<Builder> b = Parser::fromJson("null");
  Slice s = b->slice();

  ASSERT_EQ(15824746774140724939ULL, s.hash());
  ASSERT_EQ(15824746774140724939ULL, s.normalizedHash());
}

TEST(SliceTest, HashDouble) {
  std::shared_ptr<Builder> b = Parser::fromJson("-345354.35532352");
  Slice s = b->slice();

  ASSERT_EQ(6761808920526616855ULL, s.hash());
  ASSERT_EQ(3752886392484756466ULL, s.normalizedHash());
}

TEST(SliceTest, HashString) {
  std::shared_ptr<Builder> b = Parser::fromJson("\"this is a test string\"");
  Slice s = b->slice();

  ASSERT_EQ(6980608708952405024ULL, s.hash());
  ASSERT_EQ(6980608708952405024ULL, s.normalizedHash());
}

TEST(SliceTest, HashStringEmpty) {
  std::shared_ptr<Builder> b = Parser::fromJson("\"\"");
  Slice s = b->slice();

  ASSERT_EQ(320496339300456582ULL, s.hash());
  ASSERT_EQ(320496339300456582ULL, s.normalizedHash());
}

TEST(SliceTest, HashStringShort) {
  std::shared_ptr<Builder> b = Parser::fromJson("\"123456\"");
  Slice s = b->slice();

  ASSERT_EQ(8249707569299589838ULL, s.hash());
  ASSERT_EQ(8249707569299589838ULL, s.normalizedHash());
  ASSERT_EQ(8249707569299589838ULL, s.hashString());
}

TEST(SliceTest, HashArray) {
  std::shared_ptr<Builder> b = Parser::fromJson("[1,2,3,4,5,6,7,8,9,10]");
  Slice s = b->slice();

  ASSERT_EQ(18208530095070427143ULL, s.hash());
}

TEST(SliceTest, HashObject) {
  std::shared_ptr<Builder> b = Parser::fromJson(
      "{\"one\":1,\"two\":2,\"three\":3,\"four\":4,\"five\":5,\"six\":6,"
      "\"seven\":7}");
  Slice s = b->slice();

  ASSERT_EQ(6457404870531647212ULL, s.hash());
}

TEST(SliceTest, NormalizedHashDouble) {
  Builder b1;
  b1.openArray();
  b1.add(Value(-1.0));
  b1.add(Value(0));
  b1.add(Value(1.0));
  b1.add(Value(2.0));
  b1.add(Value(3.0));
  b1.add(Value(42.0));
  b1.add(Value(-42.0));
  b1.add(Value(123456.0));
  b1.add(Value(-123456.0));
  b1.close();

  Builder b2;
  b2.openArray();
  b2.add(Value(-1));
  b2.add(Value(0));
  b2.add(Value(1));
  b2.add(Value(2));
  b2.add(Value(3));
  b2.add(Value(42));
  b2.add(Value(-42));
  b2.add(Value(123456));
  b2.add(Value(-123456));
  b2.close();

  // hash values differ, but normalized hash values shouldn't!
  ASSERT_EQ(4818112773835168348ULL, b1.slice().hash());
  ASSERT_EQ(12966989194194709699ULL, b2.slice().hash());

  ASSERT_EQ(7665406973067858677ULL, b1.slice().normalizedHash());
  ASSERT_EQ(7665406973067858677ULL, b2.slice().normalizedHash());
}

TEST(SliceTest, NormalizedHashArray) {
  Options options;

  options.buildUnindexedArrays = false;
  std::shared_ptr<Builder> b1 = Parser::fromJson("[1,2,3,4,5,6,7,8,9,10]", &options);
  Slice s1 = b1->slice();

  options.buildUnindexedArrays = true;
  std::shared_ptr<Builder> b2 = Parser::fromJson("[1,2,3,4,5,6,7,8,9,10]", &options);
  Slice s2 = b2->slice();

  // hash values differ, but normalized hash values shouldn't!
  ASSERT_EQ(18208530095070427143ULL, s1.hash());
  ASSERT_EQ(4459115985522862126ULL, s2.hash());

  ASSERT_EQ(12687829189178745330ULL, s1.normalizedHash());
  ASSERT_EQ(12687829189178745330ULL, s2.normalizedHash());
}

TEST(SliceTest, NormalizedHashArrayNested) {
  Options options;

  options.buildUnindexedArrays = false;
  std::shared_ptr<Builder> b1 = Parser::fromJson("[-4.0,1,2.0,-4345.0,4,5,6,7,8,9,10,[1,9,-42,45.0]]", &options);
  Slice s1 = b1->slice();

  options.buildUnindexedArrays = true;
  std::shared_ptr<Builder> b2 = Parser::fromJson("[-4.0,1,2.0,-4345.0,4,5,6,7,8,9,10,[1,9,-42,45.0]]", &options);
  Slice s2 = b2->slice();

  // hash values differ, but normalized hash values shouldn't!
  ASSERT_EQ(481920679374299546ULL, s1.hash());
  ASSERT_EQ(13841768155551889409ULL, s2.hash());

  ASSERT_EQ(3081255268503050102ULL, s1.normalizedHash());
  ASSERT_EQ(3081255268503050102ULL, s2.normalizedHash());
}

TEST(SliceTest, NormalizedHashObjectOrder) {
  Options options;

  options.buildUnindexedObjects = false;
  std::shared_ptr<Builder> b1 = Parser::fromJson(
      "{\"one\":1,\"two\":2,\"three\":3,\"four\":4,\"five\":5,\"six\":6,"
      "\"seven\":7}", &options);
  Slice s1 = b1->slice();

  options.buildUnindexedObjects = false;
  std::shared_ptr<Builder> b2 = Parser::fromJson(
      "{\"seven\":7,\"six\":6,\"five\":5,\"four\":4,\"three\":3,\"two\":2,\"one\":1}", &options);
  Slice s2 = b2->slice();

  // hash values differ, but normalized hash values shouldn't!
  ASSERT_EQ(6457404870531647212ULL, s1.hash());
  ASSERT_EQ(11941263562352219035ULL, s2.hash());

  ASSERT_EQ(16039089241133611790ULL, s1.normalizedHash());
  ASSERT_EQ(16039089241133611790ULL, s2.normalizedHash());
}

#endif

#ifdef VELOCYPACK_FASTHASH

TEST(SliceTest, HashNull) {
  std::shared_ptr<Builder> b = Parser::fromJson("null");
  Slice s = b->slice();

  ASSERT_EQ(15292542490648858194ULL, s.hash());
  ASSERT_EQ(15292542490648858194ULL, s.normalizedHash());
}

TEST(SliceTest, HashDouble) {
  std::shared_ptr<Builder> b = Parser::fromJson("-345354.35532352");
  Slice s = b->slice();

  ASSERT_EQ(8711156443018077288ULL, s.hash());
  ASSERT_EQ(15147306223577264442ULL, s.normalizedHash());
}

TEST(SliceTest, HashString) {
  std::shared_ptr<Builder> b = Parser::fromJson("\"this is a test string\"");
  Slice s = b->slice();

  ASSERT_EQ(16298643255475496611ULL, s.hash());
  ASSERT_EQ(16298643255475496611ULL, s.normalizedHash());
  ASSERT_EQ(16298643255475496611ULL, s.hashString());
}

TEST(SliceTest, HashStringEmpty) {
  std::shared_ptr<Builder> b = Parser::fromJson("\"\"");
  Slice s = b->slice();

  ASSERT_EQ(5324680019219065241ULL, s.hash());
  ASSERT_EQ(5324680019219065241ULL, s.normalizedHash());
  ASSERT_EQ(5324680019219065241ULL, s.hashString());
}

TEST(SliceTest, HashStringShort) {
  std::shared_ptr<Builder> b = Parser::fromJson("\"123456\"");
  Slice s = b->slice();

  ASSERT_EQ(13345050106135537218ULL, s.hash());
  ASSERT_EQ(13345050106135537218ULL, s.normalizedHash());
  ASSERT_EQ(13345050106135537218ULL, s.hashString());
}

TEST(SliceTest, HashStringMedium) {
  std::shared_ptr<Builder> b = Parser::fromJson("\"123456foobar,this is a medium sized string\"");
  Slice s = b->slice();

  ASSERT_EQ(11452660398945112315ULL, s.hash());
  ASSERT_EQ(11452660398945112315ULL, s.normalizedHash());
  ASSERT_EQ(11452660398945112315ULL, s.hashString());
}

TEST(SliceTest, HashStringLong) {
  std::shared_ptr<Builder> b = Parser::fromJson("\"the quick brown fox jumped over the lazy dog, and it jumped and jumped "
      "and jumped and went on. But then, the String needed to get even longer "
      "and longer until the test finally worked.\"");
  Slice s = b->slice();

  ASSERT_EQ(14870584969143055038ULL, s.hash());
  ASSERT_EQ(14870584969143055038ULL, s.normalizedHash());
  ASSERT_EQ(14870584969143055038ULL, s.hashString());
}

TEST(SliceTest, HashArray) {
  std::shared_ptr<Builder> b = Parser::fromJson("[1,2,3,4,5,6,7,8,9,10]");
  Slice s = b->slice();

  ASSERT_EQ(1515761289406454211ULL, s.hash());
}

TEST(SliceTest, HashObject) {
  std::shared_ptr<Builder> b = Parser::fromJson(
      "{\"one\":1,\"two\":2,\"three\":3,\"four\":4,\"five\":5,\"six\":6,"
      "\"seven\":7}");
  Slice s = b->slice();

  ASSERT_EQ(6865527808070733846ULL, s.hash());
}

TEST(SliceTest, NormalizedHashDouble) {
  Builder b1;
  b1.openArray();
  b1.add(Value(-1.0));
  b1.add(Value(0));
  b1.add(Value(1.0));
  b1.add(Value(2.0));
  b1.add(Value(3.0));
  b1.add(Value(42.0));
  b1.add(Value(-42.0));
  b1.add(Value(123456.0));
  b1.add(Value(-123456.0));
  b1.close();

  Builder b2;
  b2.openArray();
  b2.add(Value(-1));
  b2.add(Value(0));
  b2.add(Value(1));
  b2.add(Value(2));
  b2.add(Value(3));
  b2.add(Value(42));
  b2.add(Value(-42));
  b2.add(Value(123456));
  b2.add(Value(-123456));
  b2.close();

  // hash values differ, but normalized hash values shouldn't!
  ASSERT_EQ(200376126201688693ULL, b1.slice().hash());
  ASSERT_EQ(3369550273364380220ULL, b2.slice().hash());

  ASSERT_EQ(65589186907022834ULL, b1.slice().normalizedHash());
  ASSERT_EQ(65589186907022834ULL, b2.slice().normalizedHash());
}

TEST(SliceTest, NormalizedHashArray) {
  Options options;

  options.buildUnindexedArrays = false;
  std::shared_ptr<Builder> b1 = Parser::fromJson("[1,2,3,4,5,6,7,8,9,10]", &options);
  Slice s1 = b1->slice();

  options.buildUnindexedArrays = true;
  std::shared_ptr<Builder> b2 = Parser::fromJson("[1,2,3,4,5,6,7,8,9,10]", &options);
  Slice s2 = b2->slice();

  // hash values differ, but normalized hash values shouldn't!
  ASSERT_EQ(1515761289406454211ULL, s1.hash());
  ASSERT_EQ(6179595527158943660ULL, s2.hash());

  ASSERT_EQ(13469007395921057835ULL, s1.normalizedHash());
  ASSERT_EQ(13469007395921057835ULL, s2.normalizedHash());
}

TEST(SliceTest, NormalizedHashArrayNested) {
  Options options;

  options.buildUnindexedArrays = false;
  std::shared_ptr<Builder> b1 = Parser::fromJson("[-4.0,1,2.0,-4345.0,4,5,6,7,8,9,10,[1,9,-42,45.0]]", &options);
  Slice s1 = b1->slice();

  options.buildUnindexedArrays = true;
  std::shared_ptr<Builder> b2 = Parser::fromJson("[-4.0,1,2.0,-4345.0,4,5,6,7,8,9,10,[1,9,-42,45.0]]", &options);
  Slice s2 = b2->slice();

  // hash values differ, but normalized hash values shouldn't!
  ASSERT_EQ(437707331568343016ULL, s1.hash());
  ASSERT_EQ(12530379609568313352ULL, s2.hash());

  ASSERT_EQ(16364328471445495391ULL, s1.normalizedHash());
  ASSERT_EQ(16364328471445495391ULL, s2.normalizedHash());
}

TEST(SliceTest, NormalizedHashObject) {
  Options options;

  options.buildUnindexedObjects = false;
  std::shared_ptr<Builder> b1 = Parser::fromJson(
      "{\"one\":1,\"two\":2,\"three\":3,\"four\":4,\"five\":5,\"six\":6,"
      "\"seven\":7}", &options);
  Slice s1 = b1->slice();

  options.buildUnindexedObjects = true;
  std::shared_ptr<Builder> b2 = Parser::fromJson(
      "{\"one\":1,\"two\":2,\"three\":3,\"four\":4,\"five\":5,\"six\":6,"
      "\"seven\":7}", &options);
  Slice s2 = b2->slice();

  // hash values differ, but normalized hash values shouldn't!
  ASSERT_EQ(6865527808070733846ULL, s1.hash());
  ASSERT_EQ(4048487509578424242ULL, s2.hash());

  ASSERT_EQ(18068466095586825298ULL, s1.normalizedHash());
  ASSERT_EQ(18068466095586825298ULL, s2.normalizedHash());
}

TEST(SliceTest, NormalizedHashObjectOrder) {
  Options options;

  options.buildUnindexedObjects = false;
  std::shared_ptr<Builder> b1 = Parser::fromJson(
      "{\"one\":1,\"two\":2,\"three\":3,\"four\":4,\"five\":5,\"six\":6,"
      "\"seven\":7}", &options);
  Slice s1 = b1->slice();

  options.buildUnindexedObjects = false;
  std::shared_ptr<Builder> b2 = Parser::fromJson(
      "{\"seven\":7,\"six\":6,\"five\":5,\"four\":4,\"three\":3,\"two\":2,\"one\":1}", &options);
  Slice s2 = b2->slice();

  // hash values differ, but normalized hash values shouldn't!
  ASSERT_EQ(6865527808070733846ULL, s1.hash());
  ASSERT_EQ(11084437118009261125ULL, s2.hash());

  ASSERT_EQ(18068466095586825298ULL, s1.normalizedHash());
  ASSERT_EQ(18068466095586825298ULL, s2.normalizedHash());
}

#endif

TEST(SliceTest, GetNumericValueIntNoLoss) {
  Builder b;
  b.add(Value(ValueType::Array));
  b.add(Value(1));
  b.add(Value(-1));
  b.add(Value(10));
  b.add(Value(-10));
  b.add(Value(INT64_MAX));
  b.add(Value(-3453.32));
  b.add(Value(2343323453.3232235));
  b.close();

  Slice s = Slice(b.start());

  ASSERT_EQ(1, s.at(0).getNumber<int64_t>());
  ASSERT_EQ(-1, s.at(1).getNumber<int64_t>());
  ASSERT_EQ(10, s.at(2).getNumber<int64_t>());
  ASSERT_EQ(-10, s.at(3).getNumber<int64_t>());
  ASSERT_EQ(INT64_MAX, s.at(4).getNumber<int64_t>());
  ASSERT_EQ(-3453, s.at(5).getNumber<int64_t>());
  ASSERT_EQ(2343323453, s.at(6).getNumber<int64_t>());

  ASSERT_EQ(1, s.at(0).getNumber<int16_t>());
  ASSERT_EQ(-1, s.at(1).getNumber<int16_t>());
  ASSERT_EQ(10, s.at(2).getNumber<int16_t>());
  ASSERT_EQ(-10, s.at(3).getNumber<int16_t>());
  ASSERT_VELOCYPACK_EXCEPTION(s.at(4).getNumber<int16_t>(),
                              Exception::NumberOutOfRange);
  ASSERT_EQ(-3453, s.at(5).getNumber<int16_t>());
  ASSERT_VELOCYPACK_EXCEPTION(s.at(6).getNumber<int16_t>(),
                              Exception::NumberOutOfRange);
}

TEST(SliceTest, GetNumericValueUIntNoLoss) {
  Builder b;
  b.add(Value(ValueType::Array));
  b.add(Value(1));
  b.add(Value(-1));
  b.add(Value(10));
  b.add(Value(-10));
  b.add(Value(INT64_MAX));
  b.add(Value(-3453.32));
  b.add(Value(2343323453.3232235));
  b.close();

  Slice s = Slice(b.start());

  ASSERT_EQ(1ULL, s.at(0).getNumber<uint64_t>());
  ASSERT_VELOCYPACK_EXCEPTION(s.at(1).getNumber<uint64_t>(),
                              Exception::NumberOutOfRange);
  ASSERT_EQ(10ULL, s.at(2).getNumber<uint64_t>());
  ASSERT_VELOCYPACK_EXCEPTION(s.at(3).getNumber<uint64_t>(),
                              Exception::NumberOutOfRange);
  ASSERT_EQ(static_cast<uint64_t>(INT64_MAX), s.at(4).getNumber<uint64_t>());
  ASSERT_VELOCYPACK_EXCEPTION(s.at(5).getNumber<uint64_t>(),
                              Exception::NumberOutOfRange);
  ASSERT_EQ(2343323453ULL, s.at(6).getNumber<uint64_t>());

  ASSERT_EQ(1ULL, s.at(0).getNumber<uint16_t>());
  ASSERT_VELOCYPACK_EXCEPTION(s.at(1).getNumber<uint16_t>(),
                              Exception::NumberOutOfRange);
  ASSERT_EQ(10ULL, s.at(2).getNumber<uint16_t>());
  ASSERT_VELOCYPACK_EXCEPTION(s.at(3).getNumber<uint16_t>(),
                              Exception::NumberOutOfRange);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(4).getNumber<uint16_t>(),
                              Exception::NumberOutOfRange);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(5).getNumber<uint16_t>(),
                              Exception::NumberOutOfRange);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(6).getNumber<uint16_t>(),
                              Exception::NumberOutOfRange);
}

TEST(SliceTest, GetNumericValueDoubleNoLoss) {
  Builder b;
  b.add(Value(ValueType::Array));
  b.add(Value(1));
  b.add(Value(-1));
  b.add(Value(10));
  b.add(Value(-10));
  b.add(Value(INT64_MAX));
  b.add(Value(-3453.32));
  b.add(Value(2343323453.3232235));
  b.add(Value(static_cast<uint64_t>(10000)));
  b.close();

  Slice s = Slice(b.start());

  ASSERT_DOUBLE_EQ(1., s.at(0).getNumber<double>());
  ASSERT_DOUBLE_EQ(-1., s.at(1).getNumber<double>());
  ASSERT_DOUBLE_EQ(10., s.at(2).getNumber<double>());
  ASSERT_DOUBLE_EQ(-10., s.at(3).getNumber<double>());
  ASSERT_DOUBLE_EQ(static_cast<double>(INT64_MAX), s.at(4).getNumber<double>());
  ASSERT_DOUBLE_EQ(-3453.32, s.at(5).getNumber<double>());
  ASSERT_DOUBLE_EQ(2343323453.3232235, s.at(6).getNumber<double>());
  ASSERT_DOUBLE_EQ(10000., s.at(7).getNumber<double>());
}

TEST(SliceTest, GetNumericValueWrongSource) {
  Builder b;
  b.add(Value(ValueType::Array));
  b.add(Value(ValueType::Null));
  b.add(Value(true));
  b.add(Value("foo"));
  b.add(Value("bar"));
  b.add(Value(ValueType::Array));
  b.close();
  b.add(Value(ValueType::Object));
  b.close();
  b.close();

  Slice s = Slice(b.start());

  ASSERT_VELOCYPACK_EXCEPTION(s.at(0).getNumber<int64_t>(),
                              Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(0).getNumber<uint64_t>(),
                              Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(0).getNumber<double>(),
                              Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(1).getNumber<int64_t>(),
                              Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(1).getNumber<uint64_t>(),
                              Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(1).getNumber<double>(),
                              Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(2).getNumber<int64_t>(),
                              Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(2).getNumber<uint64_t>(),
                              Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(2).getNumber<double>(),
                              Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(3).getNumber<int64_t>(),
                              Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(3).getNumber<uint64_t>(),
                              Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(3).getNumber<double>(),
                              Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(4).getNumber<int64_t>(),
                              Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(4).getNumber<uint64_t>(),
                              Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(4).getNumber<double>(),
                              Exception::InvalidValueType);
}

TEST(SliceTest, Translate) {
  std::unique_ptr<AttributeTranslator> translator(new AttributeTranslator);

  translator->add("foo", 1);
  translator->add("bar", 2);
  translator->add("baz", 3);
  translator->add("bark", 4);
  translator->add("mötör", 5);
  translator->seal();

  AttributeTranslatorScope scope(translator.get());

  Options options;
  Builder b(&options);
  options.attributeTranslator = translator.get();

  b.add(Value(ValueType::Object));
  b.add("foo", Value(true));
  b.add("bar", Value(false));
  b.add("baz", Value(1));
  b.add("bart", Value(2));
  b.add("bark", Value(42));
  b.add("mötör", Value(19));
  b.add("mötörhead", Value(20));
  b.close();

  Slice s = Slice(b.start());

  ASSERT_EQ(7UL, s.length());

  ASSERT_EQ("bar", Slice(s.start() + s.getNthOffset(0)).translate().copyString());
  ASSERT_EQ("bark", Slice(s.start() + s.getNthOffset(1)).translate().copyString());
  ASSERT_VELOCYPACK_EXCEPTION(Slice(s.start() + s.getNthOffset(2)).translate().copyString(), Exception::InvalidValueType);
  ASSERT_EQ("baz", Slice(s.start() + s.getNthOffset(3)).translate().copyString());
  ASSERT_EQ("foo", Slice(s.start() + s.getNthOffset(4)).translate().copyString());
  ASSERT_EQ("mötör", Slice(s.start() + s.getNthOffset(5)).translate().copyString());
  ASSERT_VELOCYPACK_EXCEPTION(Slice(s.start() + s.getNthOffset(6)).translate().copyString(), Exception::InvalidValueType);

  // try again w/o AttributeTranslator
  scope.revert();
  ASSERT_VELOCYPACK_EXCEPTION(Slice(s.start() + s.getNthOffset(0)).translate().copyString(), Exception::NeedAttributeTranslator);
}

TEST(SliceTest, TranslateSingleMember) {
  std::unique_ptr<AttributeTranslator> translator(new AttributeTranslator);

  translator->add("foo", 1);
  translator->seal();

  AttributeTranslatorScope scope(translator.get());

  Options options;
  Builder b(&options);
  options.attributeTranslator = translator.get();

  b.add(Value(ValueType::Object));
  b.add("foo", Value(true));
  b.close();

  Slice s = Slice(b.start());

  ASSERT_EQ(1UL, s.length());

  ASSERT_EQ("foo", Slice(s.start() + s.getNthOffset(0)).translate().copyString());

  // try again w/o AttributeTranslator
  scope.revert();
  ASSERT_VELOCYPACK_EXCEPTION(Slice(s.start() + s.getNthOffset(0)).translate().copyString(), Exception::NeedAttributeTranslator);
}

TEST(SliceTest, Translations) {
  std::unique_ptr<AttributeTranslator> translator(new AttributeTranslator);

  translator->add("foo", 1);
  translator->add("bar", 2);
  translator->add("baz", 3);
  translator->add("bark", 4);
  translator->add("mötör", 5);
  translator->add("quetzalcoatl", 6);
  translator->seal();

  AttributeTranslatorScope scope(translator.get());

  Options options;
  Builder b(&options);
  options.attributeTranslator = translator.get();

  b.add(Value(ValueType::Object));
  b.add("foo", Value(true));
  b.add("bar", Value(false));
  b.add("baz", Value(1));
  b.add("bart", Value(2));
  b.add("bark", Value(42));
  b.add("mötör", Value(19));
  b.add("mötörhead", Value(20));
  b.add("quetzal", Value(21));
  b.close();

  Slice s = Slice(b.start());

  ASSERT_EQ(8UL, s.length());
  ASSERT_TRUE(s.hasKey("foo"));
  ASSERT_TRUE(s.get("foo").getBoolean());
  ASSERT_TRUE(s.hasKey("bar"));
  ASSERT_FALSE(s.get("bar").getBoolean());
  ASSERT_TRUE(s.hasKey("baz"));
  ASSERT_EQ(1UL, s.get("baz").getUInt());
  ASSERT_TRUE(s.hasKey("bart"));
  ASSERT_EQ(2UL, s.get("bart").getUInt());
  ASSERT_TRUE(s.hasKey("bark"));
  ASSERT_EQ(42UL, s.get("bark").getUInt());
  ASSERT_TRUE(s.hasKey("mötör"));
  ASSERT_EQ(19UL, s.get("mötör").getUInt());
  ASSERT_TRUE(s.hasKey("mötörhead"));
  ASSERT_EQ(20UL, s.get("mötörhead").getUInt());
  ASSERT_TRUE(s.hasKey("quetzal"));
  ASSERT_EQ(21UL, s.get("quetzal").getUInt());

  ASSERT_EQ("bar", s.keyAt(0).copyString());
  ASSERT_EQ("bark", s.keyAt(1).copyString());
  ASSERT_EQ("bart", s.keyAt(2).copyString());
  ASSERT_EQ("baz", s.keyAt(3).copyString());
  ASSERT_EQ("foo", s.keyAt(4).copyString());
  ASSERT_EQ("mötör", s.keyAt(5).copyString());
  ASSERT_EQ("mötörhead", s.keyAt(6).copyString());
  ASSERT_EQ("quetzal", s.keyAt(7).copyString());
}

TEST(SliceTest, TranslationsSingleMemberObject) {
  std::unique_ptr<AttributeTranslator> translator(new AttributeTranslator);

  translator->add("foo", 1);
  translator->seal();

  AttributeTranslatorScope scope(translator.get());

  Options options;
  Builder b(&options);
  options.attributeTranslator = translator.get();

  b.add(Value(ValueType::Object));
  b.add("foo", Value(true));
  b.close();

  Slice s = Slice(b.start());

  ASSERT_EQ(1UL, s.length());
  ASSERT_TRUE(s.hasKey("foo"));
  ASSERT_TRUE(s.get("foo").getBoolean());

  ASSERT_FALSE(s.hasKey("bar"));
  ASSERT_TRUE(s.get("bar").isNone());
}

TEST(SliceTest, TranslationsSubObjects) {
  std::unique_ptr<AttributeTranslator> translator(new AttributeTranslator);

  translator->add("foo", 1);
  translator->add("bar", 2);
  translator->add("baz", 3);
  translator->add("bark", 4);
  translator->seal();

  AttributeTranslatorScope scope(translator.get());

  Options options;
  options.attributeTranslator = translator.get();

  Builder b(&options);
  b.add(Value(ValueType::Object));
  b.add("foo", Value(ValueType::Object));
  b.add("bar", Value(false));
  b.add("baz", Value(1));
  b.add("bark", Value(ValueType::Object));
  b.add("foo", Value(2));
  b.add("bar", Value(3));
  b.close();
  b.close();
  b.add("bar", Value(4));
  b.add("bark", Value(ValueType::Object));
  b.add("foo", Value(5));
  b.add("bar", Value(6));
  b.close();
  b.close();

  Slice s = Slice(b.start());

  ASSERT_EQ(3UL, s.length());
  ASSERT_TRUE(s.hasKey("foo"));
  ASSERT_TRUE(s.get("foo").isObject());
  ASSERT_FALSE(s.get(std::vector<std::string>({"foo", "bar"})).getBoolean());
  ASSERT_EQ(1UL, s.get(std::vector<std::string>({"foo", "baz"})).getUInt());
  ASSERT_TRUE(s.get(std::vector<std::string>({"foo", "bark"})).isObject());
  ASSERT_EQ(2UL,
            s.get(std::vector<std::string>({"foo", "bark", "foo"})).getUInt());
  ASSERT_EQ(3UL,
            s.get(std::vector<std::string>({"foo", "bark", "bar"})).getUInt());
  ASSERT_TRUE(s.hasKey("bar"));
  ASSERT_EQ(4UL, s.get("bar").getUInt());
  ASSERT_TRUE(s.hasKey("bark"));
  ASSERT_TRUE(s.get("bark").isObject());
  ASSERT_EQ(5UL, s.get(std::vector<std::string>({"bark", "foo"})).getUInt());
  ASSERT_EQ(6UL, s.get(std::vector<std::string>({"bark", "bar"})).getUInt());
  ASSERT_EQ("bar", s.keyAt(0).copyString());
  ASSERT_EQ("bark", s.keyAt(1).copyString());
  ASSERT_EQ("bar", s.valueAt(1).keyAt(0).copyString());
  ASSERT_EQ("foo", s.valueAt(1).keyAt(1).copyString());
  ASSERT_EQ("foo", s.keyAt(2).copyString());
  ASSERT_EQ("bar", s.valueAt(2).keyAt(0).copyString());
  ASSERT_EQ("bark", s.valueAt(2).keyAt(1).copyString());
  ASSERT_EQ("baz", s.valueAt(2).keyAt(2).copyString());
}

TEST(SliceTest, TranslatedObjectWithoutTranslator) {
  std::unique_ptr<AttributeTranslator> translator(new AttributeTranslator);

  translator->add("foo", 1);
  translator->add("bar", 2);
  translator->add("baz", 3);
  translator->seal();

  AttributeTranslatorScope scope(translator.get());

  Options options;
  Builder b(&options);
  options.attributeTranslator = translator.get();

  b.add(Value(ValueType::Object));
  b.add("mötör", Value(1));
  b.add("quetzal", Value(2));
  b.add("foo", Value(3));
  b.add("bar", Value(4));
  b.add("baz", Value(5));
  b.add("bart", Value(6));
  b.close();

  Slice s = Slice(b.start());

  scope.revert();

  ASSERT_EQ(6UL, s.length());
  ASSERT_VELOCYPACK_EXCEPTION(s.keyAt(0).copyString(),
                              Exception::NeedAttributeTranslator);
  ASSERT_EQ("bart", s.keyAt(1).copyString());
  ASSERT_VELOCYPACK_EXCEPTION(s.keyAt(2).copyString(),
                              Exception::NeedAttributeTranslator);
  ASSERT_VELOCYPACK_EXCEPTION(s.keyAt(3).copyString(),
                              Exception::NeedAttributeTranslator);
  ASSERT_EQ("mötör", s.keyAt(4).copyString());
  ASSERT_EQ("quetzal", s.keyAt(5).copyString());
}

TEST(SliceTest, TranslatedWithCompactNotation) {
  std::unique_ptr<AttributeTranslator> translator(new AttributeTranslator);

  translator->add("foo", 1);
  translator->add("bar", 2);
  translator->add("baz", 3);
  translator->add("bart", 4);
  translator->add("bark", 5);
  translator->seal();

  AttributeTranslatorScope scope(translator.get());

  Options options;
  Builder b(&options);
  options.buildUnindexedObjects = true;
  options.attributeTranslator = translator.get();

  b.add(Value(ValueType::Object));
  b.add("foo", Value(1));
  b.add("bar", Value(2));
  b.add("baz", Value(3));
  b.add("bark", Value(4));
  b.add("bart", Value(5));
  b.close();

  Slice s = Slice(b.start());
  ASSERT_EQ(0x14, s.head());

  ASSERT_EQ(5UL, s.length());
  ASSERT_EQ("foo", s.keyAt(0).copyString());
  ASSERT_EQ("bar", s.keyAt(1).copyString());
  ASSERT_EQ("baz", s.keyAt(2).copyString());
  ASSERT_EQ("bark", s.keyAt(3).copyString());
  ASSERT_EQ("bart", s.keyAt(4).copyString());
}

TEST(SliceTest, TranslatedInvalidKey) {
  std::unique_ptr<AttributeTranslator> translator(new AttributeTranslator);

  translator->add("foo", 1);
  translator->seal();

  AttributeTranslatorScope scope(translator.get());

  Options options;
  options.attributeTranslator = translator.get();

  // a compact object with a single member (key: 4, value: false)
  uint8_t const data[] = {0x14, 0x05, 0x34, 0x19, 0x01};

  Slice s = Slice(data);

  ASSERT_EQ(1UL, s.length());
  ASSERT_VELOCYPACK_EXCEPTION(s.keyAt(0).copyString(), Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(Collection::keys(s), Exception::InvalidValueType);
}

TEST(SliceTest, CustomTypeByteSize) {
  uint8_t example0[] = { 0xf0, 0x00 };
  {
    Slice s(example0);
    ASSERT_EQ(sizeof(example0), s.byteSize());
  }

  uint8_t example1[] = { 0xf1, 0x00, 0x00 };
  {
    Slice s(example1);
    ASSERT_EQ(sizeof(example1), s.byteSize());
  }

  uint8_t example2[] = { 0xf2, 0x00, 0x00, 0x00, 0x00 };
  {
    Slice s(example2);
    ASSERT_EQ(sizeof(example2), s.byteSize());
  }

  uint8_t example3[] = { 0xf3, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
  {
    Slice s(example3);
    ASSERT_EQ(sizeof(example3), s.byteSize());
  }

  uint8_t example4[] = { 0xf4, 0x03, 0x00, 0x00, 0x00 };
  {
    Slice s(example4);
    ASSERT_EQ(sizeof(example4), s.byteSize());
  }

  uint8_t example5[] = { 0xf5, 0x02, 0x00, 0x00 };
  {
    Slice s(example5);
    ASSERT_EQ(sizeof(example5), s.byteSize());
  }

  uint8_t example6[] = { 0xf6, 0x01, 0x00 };
  {
    Slice s(example6);
    ASSERT_EQ(sizeof(example6), s.byteSize());
  }

  uint8_t example7[] = { 0xf7, 0x01, 0x00, 0x00 };
  {
    Slice s(example7);
    ASSERT_EQ(sizeof(example7), s.byteSize());
  }

  uint8_t example8[] = { 0xf8, 0x02, 0x00, 0x00, 0x00 };
  {
    Slice s(example8);
    ASSERT_EQ(sizeof(example8), s.byteSize());
  }

  uint8_t example9[] = { 0xf9, 0x03, 0x00, 0x00, 0x00, 0x00 };
  {
    Slice s(example9);
    ASSERT_EQ(sizeof(example9), s.byteSize());
  }

  uint8_t example10[] = { 0xfa, 0x01, 0x00, 0x00, 0x00, 0x00 };
  {
    Slice s(example10);
    ASSERT_EQ(sizeof(example10), s.byteSize());
  }

  uint8_t example11[] = { 0xfb, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00 };
  {
    Slice s(example11);
    ASSERT_EQ(sizeof(example11), s.byteSize());
  }

  uint8_t example12[] = { 0xfc, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
  {
    Slice s(example12);
    ASSERT_EQ(sizeof(example12), s.byteSize());
  }

  uint8_t example13[] = { 0xfd, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
  {
    Slice s(example13);
    ASSERT_EQ(sizeof(example13), s.byteSize());
  }

  uint8_t example14[] = { 0xfe, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
  {
    Slice s(example14);
    ASSERT_EQ(sizeof(example14), s.byteSize());
  }

  uint8_t example15[] = { 0xff, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
  {
    Slice s(example15);
    ASSERT_EQ(sizeof(example15), s.byteSize());
  }
}

TEST(SliceTest, Reassign) {
  uint8_t buf1[] = { 0x19 };
  uint8_t buf2[] = { 0x1a };
  Slice s(buf1);
  ASSERT_TRUE(s.isBoolean());
  ASSERT_FALSE(s.getBool());
  s.set(buf2);
  ASSERT_TRUE(s.isBoolean());
  ASSERT_TRUE(s.getBool());
}

TEST(SliceTest, TranslateInObjectIterator) {
  std::unique_ptr<AttributeTranslator> translator(new AttributeTranslator);

  translator->add("_key", 1);
  translator->seal();

  AttributeTranslatorScope scope(translator.get());

  Builder b;
  b.openObject();
  b.add("_key", Value(12));
  b.close();

  Slice s = b.slice();
  for (auto p : ObjectIterator(s)) {
    Slice k = p.key;
    ASSERT_TRUE(k.isString());
  }
}

TEST(SliceTest, IsNumber) {
  Slice s;
  Builder b;

  // number 0
  b.clear();
  b.add(Value(int(0)));
  s = b.slice();

  ASSERT_TRUE(s.isNumber());
  ASSERT_TRUE(s.isNumber<int>());
  ASSERT_TRUE(s.isNumber<int64_t>());
  ASSERT_TRUE(s.isNumber<uint64_t>());
  ASSERT_TRUE(s.isNumber<double>());

  ASSERT_EQ(int(0), s.getNumber<int>());
  ASSERT_EQ(int64_t(0), s.getNumber<int64_t>());
  ASSERT_EQ(uint64_t(0), s.getNumber<uint64_t>());
  ASSERT_EQ(double(0.0), s.getNumber<double>());

  // positive int
  b.clear();
  b.add(Value(int(42)));
  s = b.slice();

  ASSERT_TRUE(s.isNumber());
  ASSERT_TRUE(s.isNumber<int>());
  ASSERT_TRUE(s.isNumber<int64_t>());
  ASSERT_TRUE(s.isNumber<uint64_t>());
  ASSERT_TRUE(s.isNumber<double>());

  ASSERT_EQ(int(42), s.getNumber<int>());
  ASSERT_EQ(int64_t(42), s.getNumber<int64_t>());
  ASSERT_EQ(uint64_t(42), s.getNumber<uint64_t>());
  ASSERT_EQ(double(42.0), s.getNumber<double>());


  // negative int
  b.clear();
  b.add(Value(int(-2)));
  s = b.slice();

  ASSERT_TRUE(s.isNumber());
  ASSERT_TRUE(s.isNumber<int>());
  ASSERT_TRUE(s.isNumber<int64_t>());
  ASSERT_FALSE(s.isNumber<uint64_t>());
  ASSERT_TRUE(s.isNumber<double>());

  ASSERT_EQ(int(-2), s.getNumber<int>());
  ASSERT_EQ(int64_t(-2), s.getNumber<int64_t>());
  ASSERT_VELOCYPACK_EXCEPTION(s.getNumber<uint64_t>(), Exception::NumberOutOfRange);
  ASSERT_EQ(double(-2.0), s.getNumber<double>());


  // positive big int
  b.clear();
  b.add(Value(int64_t(INT64_MAX)));
  s = b.slice();

  ASSERT_TRUE(s.isNumber());
  ASSERT_TRUE(s.isNumber<int64_t>());
  ASSERT_TRUE(s.isNumber<uint64_t>());
  ASSERT_TRUE(s.isNumber<double>());

  ASSERT_EQ(int64_t(INT64_MAX), s.getNumber<int64_t>());
  ASSERT_EQ(uint64_t(INT64_MAX), s.getNumber<uint64_t>());
  ASSERT_EQ(double(INT64_MAX), s.getNumber<double>());


  // negative big int
  b.clear();
  b.add(Value(int64_t(INT64_MIN)));
  s = b.slice();

  ASSERT_TRUE(s.isNumber());
  ASSERT_TRUE(s.isNumber<int64_t>());
  ASSERT_FALSE(s.isNumber<uint64_t>());
  ASSERT_TRUE(s.isNumber<double>());

  ASSERT_EQ(int64_t(INT64_MIN), s.getNumber<int64_t>());
  ASSERT_VELOCYPACK_EXCEPTION(s.getNumber<uint64_t>(), Exception::NumberOutOfRange);
  ASSERT_EQ(double(INT64_MIN), s.getNumber<double>());


  // positive big uint
  b.clear();
  b.add(Value(uint64_t(UINT64_MAX)));
  s = b.slice();

  ASSERT_TRUE(s.isNumber());
  ASSERT_FALSE(s.isNumber<int64_t>());
  ASSERT_TRUE(s.isNumber<uint64_t>());
  ASSERT_TRUE(s.isNumber<double>());

  ASSERT_VELOCYPACK_EXCEPTION(s.getNumber<int64_t>(), Exception::NumberOutOfRange);
  ASSERT_EQ(uint64_t(UINT64_MAX), s.getNumber<uint64_t>());
  ASSERT_EQ(double(UINT64_MAX), s.getNumber<double>());


  // negative double
  b.clear();
  b.add(Value(double(-1.25)));
  s = b.slice();

  ASSERT_TRUE(s.isNumber());
  ASSERT_TRUE(s.isNumber<int64_t>());
  ASSERT_VELOCYPACK_EXCEPTION(s.getNumber<uint64_t>(), Exception::NumberOutOfRange);
  ASSERT_TRUE(s.isNumber<double>());


  // positive double
  b.clear();
  b.add(Value(double(1.25)));
  s = b.slice();

  ASSERT_TRUE(s.isNumber());
  ASSERT_TRUE(s.isNumber<int64_t>());
  ASSERT_TRUE(s.isNumber<uint64_t>());
  ASSERT_TRUE(s.isNumber<double>());
}

TEST(SliceTest, ReadTag) {
  Builder b;
  b.addTagged(42, Value(5));

  Slice s = b.slice();
  ASSERT_TRUE(s.isTagged());
  ASSERT_EQ(s.getFirstTag(), 42);
  ASSERT_EQ(s.getTags().at(0), 42);
  ASSERT_EQ(s.getTags().size(), 1);
  ASSERT_TRUE(s.hasTag(42));
  ASSERT_FALSE(s.hasTag(49));

  ASSERT_EQ(s.value().getInt(), 5);
}

TEST(SliceTest, ReadTag8Bytes) {
  Builder b;
  b.addTagged(257, Value(5));

  Slice s = b.slice();
  ASSERT_TRUE(s.isTagged());
  ASSERT_EQ(s.getFirstTag(), 257);
  ASSERT_EQ(s.getTags().at(0), 257);
  ASSERT_EQ(s.getTags().size(), 1);
  ASSERT_TRUE(s.hasTag(257));
  ASSERT_FALSE(s.hasTag(49));

  ASSERT_EQ(s.value().getInt(), 5);
}

TEST(SliceTest, ReadTags) {
  Builder b;
  b.addTagged(42, Value(5));

  Builder bb;
  bb.addTagged(49, b.slice());

  Slice s = bb.slice();
  ASSERT_TRUE(s.isTagged());
  ASSERT_EQ(s.getFirstTag(), 49);
  ASSERT_EQ(s.getTags().size(), 2);
  ASSERT_EQ(s.getTags().at(0), 49);
  ASSERT_EQ(s.getTags().at(1), 42);
  ASSERT_TRUE(s.hasTag(42));
  ASSERT_TRUE(s.hasTag(49));
  ASSERT_FALSE(s.hasTag(50));

  ASSERT_EQ(s.value().getInt(), 5);
}

TEST(SliceTest, ReadTags8Bytes) {
  Builder b;
  b.addTagged(257, Value(5));

  Builder bb;
  bb.addTagged(65536, b.slice());

  Slice s = bb.slice();
  ASSERT_TRUE(s.isTagged());
  ASSERT_EQ(s.getFirstTag(), 65536);
  ASSERT_EQ(s.getTags().size(), 2);
  ASSERT_EQ(s.getTags().at(0), 65536);
  ASSERT_EQ(s.getTags().at(1), 257);
  ASSERT_TRUE(s.hasTag(257));
  ASSERT_TRUE(s.hasTag(65536));
  ASSERT_FALSE(s.hasTag(50));

  ASSERT_EQ(s.value().getInt(), 5);
}

TEST(SliceTest, UnpackTupleSlice) {
  Builder b;
  b.openArray();
  b.add(Value("some string"));
  b.add(Value(12));
  b.add(Value(false));
  b.add(Value("extracted as slice"));
  b.close();

  Slice s = b.slice();
  auto t = s.unpackTuple<std::string, int, bool, Slice>();

  ASSERT_EQ(std::get<0>(t), "some string");
  ASSERT_EQ(std::get<1>(t), 12);
  ASSERT_EQ(std::get<2>(t), false);
  ASSERT_TRUE(std::get<3>(t).isString());
}

TEST(SliceTest, UnpackTupleSliceInvalidSize) {
  Builder b;
  b.openArray();
  b.add(Value("some string"));
  b.add(Value(12));
  b.add(Value(false));
  b.close();

  Slice s = b.slice();
  ASSERT_VELOCYPACK_EXCEPTION((s.unpackTuple<std::string, int, bool, Slice>()), Exception::BadTupleSize)
}

TEST(SliceTest, ExtractTuple) {
  Builder b;
  b.openArray();
  b.add(Value("some string"));
  b.add(Value(12));
  b.add(Value(false));
  b.add(Value("extracted as slice"));
  b.close();

  Slice s = b.slice();
  auto t = s.extract<std::tuple<std::string, int, bool, Slice>>();

  ASSERT_EQ(std::get<0>(t), "some string");
  ASSERT_EQ(std::get<1>(t), 12);
  ASSERT_EQ(std::get<2>(t), false);
  ASSERT_TRUE(std::get<3>(t).isString());
}

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
