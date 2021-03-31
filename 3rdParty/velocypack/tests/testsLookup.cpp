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
#include <fstream>
#include <string>

#include "tests-common.h"

TEST(LookupTest, HasKeyShortObject) {
  std::string const value(
      "{\"foo\":null,\"bar\":true,\"baz\":13.53,\"qux\":[1],\"quz\":{}}");

  Parser parser;
  parser.parse(value);
  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  ASSERT_TRUE(s.hasKey("foo"));
  ASSERT_TRUE(s.hasKey("bar"));
  ASSERT_TRUE(s.hasKey("baz"));
  ASSERT_TRUE(s.hasKey("qux"));
  ASSERT_TRUE(s.hasKey("quz"));
  ASSERT_FALSE(s.hasKey("nada"));
  ASSERT_FALSE(s.hasKey("Foo"));
  ASSERT_FALSE(s.hasKey("food"));
  ASSERT_FALSE(s.hasKey("quxx"));
  ASSERT_FALSE(s.hasKey("q"));
  ASSERT_FALSE(s.hasKey(""));
}

TEST(LookupTest, HasKeyShortObjectCompact) {
  std::string const value(
      "{\"foo\":null,\"bar\":true,\"baz\":13.53,\"qux\":[1],\"quz\":{}}");

  Options options;
  options.buildUnindexedObjects = true;

  Parser parser(&options);

  parser.parse(value);
  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  ASSERT_EQ(0x14, s.head());

  ASSERT_TRUE(s.hasKey("foo"));
  ASSERT_TRUE(s.hasKey("bar"));
  ASSERT_TRUE(s.hasKey("baz"));
  ASSERT_TRUE(s.hasKey("qux"));
  ASSERT_TRUE(s.hasKey("quz"));
  ASSERT_FALSE(s.hasKey("nada"));
  ASSERT_FALSE(s.hasKey("Foo"));
  ASSERT_FALSE(s.hasKey("food"));
  ASSERT_FALSE(s.hasKey("quxx"));
  ASSERT_FALSE(s.hasKey("q"));
  ASSERT_FALSE(s.hasKey(""));
}

TEST(LookupTest, HasKeyLongObject) {
  std::string value("{");
  for (std::size_t i = 4; i < 1024; ++i) {
    if (i > 4) {
      value.append(",");
    }
    value.append("\"test");
    value.append(std::to_string(i));
    value.append("\":");
    value.append(std::to_string(i));
  }
  value.append("}");

  Parser parser;
  parser.parse(value);
  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  ASSERT_TRUE(s.hasKey("test4"));
  ASSERT_TRUE(s.hasKey("test10"));
  ASSERT_TRUE(s.hasKey("test42"));
  ASSERT_TRUE(s.hasKey("test100"));
  ASSERT_TRUE(s.hasKey("test932"));
  ASSERT_TRUE(s.hasKey("test1000"));
  ASSERT_TRUE(s.hasKey("test1023"));
  ASSERT_FALSE(s.hasKey("test0"));
  ASSERT_FALSE(s.hasKey("test1"));
  ASSERT_FALSE(s.hasKey("test2"));
  ASSERT_FALSE(s.hasKey("test3"));
  ASSERT_FALSE(s.hasKey("test1024"));
}

TEST(LookupTest, HasKeyLongObjectCompact) {
  std::string value("{");
  for (std::size_t i = 4; i < 1024; ++i) {
    if (i > 4) {
      value.append(",");
    }
    value.append("\"test");
    value.append(std::to_string(i));
    value.append("\":");
    value.append(std::to_string(i));
  }
  value.append("}");

  Options options;
  options.buildUnindexedObjects = true;

  Parser parser(&options);
  parser.parse(value);
  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  ASSERT_EQ(0x14, s.head());

  ASSERT_TRUE(s.hasKey("test4"));
  ASSERT_TRUE(s.hasKey("test10"));
  ASSERT_TRUE(s.hasKey("test42"));
  ASSERT_TRUE(s.hasKey("test100"));
  ASSERT_TRUE(s.hasKey("test932"));
  ASSERT_TRUE(s.hasKey("test1000"));
  ASSERT_TRUE(s.hasKey("test1023"));
  ASSERT_FALSE(s.hasKey("test0"));
  ASSERT_FALSE(s.hasKey("test1"));
  ASSERT_FALSE(s.hasKey("test2"));
  ASSERT_FALSE(s.hasKey("test3"));
  ASSERT_FALSE(s.hasKey("test1024"));
}

TEST(LookupTest, HasKeySubattributes) {
  std::string const value(
      "{\"foo\":{\"bar\":1,\"bark\":[],\"baz\":{\"qux\":{\"qurz\":null}}}}");

  Parser parser;
  parser.parse(value);
  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  ASSERT_VELOCYPACK_EXCEPTION(s.hasKey(std::vector<std::string>()),
                              Exception::InvalidAttributePath);
  ASSERT_TRUE(s.hasKey(std::vector<std::string>({"foo"})));
  ASSERT_TRUE(s.hasKey(std::vector<std::string>({"foo", "bar"})));
  ASSERT_FALSE(s.hasKey(std::vector<std::string>({"boo"})));
  ASSERT_FALSE(s.hasKey(std::vector<std::string>({"boo", "far"})));
  ASSERT_TRUE(s.hasKey(std::vector<std::string>({"foo", "bark"})));
  ASSERT_FALSE(s.hasKey(std::vector<std::string>({"foo", "bark", "baz"})));
  ASSERT_TRUE(s.hasKey(std::vector<std::string>({"foo", "baz"})));
  ASSERT_TRUE(s.hasKey(std::vector<std::string>({"foo", "baz", "qux"})));
  ASSERT_TRUE(
      s.hasKey(std::vector<std::string>({"foo", "baz", "qux", "qurz"})));
  ASSERT_FALSE(
      s.hasKey(std::vector<std::string>({"foo", "baz", "qux", "qurk"})));
  ASSERT_FALSE(s.hasKey(
      std::vector<std::string>({"foo", "baz", "qux", "qurz", "p0rk"})));
}

TEST(LookupTest, HasKeySubattributesCompact) {
  std::string const value(
      "{\"foo\":{\"bar\":1,\"bark\":[],\"baz\":{\"qux\":{\"qurz\":null}}}}");

  Options options;
  options.buildUnindexedObjects = true;

  Parser parser(&options);
  parser.parse(value);
  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  ASSERT_EQ(0x14, s.head());

  ASSERT_VELOCYPACK_EXCEPTION(s.hasKey(std::vector<std::string>()),
                              Exception::InvalidAttributePath);
  ASSERT_TRUE(s.hasKey(std::vector<std::string>({"foo"})));
  ASSERT_EQ(0x14, s.get(std::vector<std::string>({"foo"})).head());
  ASSERT_TRUE(s.hasKey(std::vector<std::string>({"foo", "bar"})));
  ASSERT_FALSE(s.hasKey(std::vector<std::string>({"boo"})));
  ASSERT_FALSE(s.hasKey(std::vector<std::string>({"boo", "far"})));
  ASSERT_TRUE(s.hasKey(std::vector<std::string>({"foo", "bark"})));
  ASSERT_FALSE(s.hasKey(std::vector<std::string>({"foo", "bark", "baz"})));
  ASSERT_TRUE(s.hasKey(std::vector<std::string>({"foo", "baz"})));
  ASSERT_TRUE(s.hasKey(std::vector<std::string>({"foo", "baz", "qux"})));
  ASSERT_TRUE(
      s.hasKey(std::vector<std::string>({"foo", "baz", "qux", "qurz"})));
  ASSERT_FALSE(
      s.hasKey(std::vector<std::string>({"foo", "baz", "qux", "qurk"})));
  ASSERT_FALSE(s.hasKey(
      std::vector<std::string>({"foo", "baz", "qux", "qurz", "p0rk"})));
}

TEST(LookupTest, EmptyObject) {
  std::string const value("{}");

  Parser parser;
  parser.parse(value);
  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  Slice v;
  v = s.get("foo");
  ASSERT_TRUE(v.isNone());

  v = s.get("bar");
  ASSERT_TRUE(v.isNone());

  v = s.get("baz");
  ASSERT_TRUE(v.isNone());
}

TEST(LookupTest, AlmostEmptyObject) {
  std::string const value("{\"foo\":1}");

  Parser parser;
  parser.parse(value);
  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  Slice v;
  v = s.get("foo");
  ASSERT_TRUE(v.isInteger());
  ASSERT_EQ(1UL, v.getUInt());

  v = s.get("bar");
  ASSERT_TRUE(v.isNone());

  v = s.get("baz");
  ASSERT_TRUE(v.isNone());
}

TEST(LookupTest, LookupShortObject) {
  std::string const value(
      "{\"foo\":null,\"bar\":true,\"baz\":13.53,\"qux\":[1],\"quz\":{}}");

  Parser parser;
  parser.parse(value);
  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  Slice v;
  v = s.get("foo");
  ASSERT_TRUE(v.isNull());

  v = s.get("bar");
  ASSERT_TRUE(v.isBool());
  ASSERT_EQ(true, v.getBool());

  v = s.get("baz");
  ASSERT_TRUE(v.isDouble());
  ASSERT_DOUBLE_EQ(13.53, v.getDouble());

  v = s.get("qux");
  ASSERT_TRUE(v.isArray());
  ASSERT_TRUE(v.isType(ValueType::Array));
  ASSERT_EQ(1ULL, v.length());

  v = s.get("quz");
  ASSERT_TRUE(v.isObject());
  ASSERT_TRUE(v.isType(ValueType::Object));
  ASSERT_EQ(0ULL, v.length());

  // non-present attributes
  v = s.get("nada");
  ASSERT_TRUE(v.isNone());

  v = s.get(std::string("foo\0", 4));
  ASSERT_TRUE(v.isNone());

  v = s.get("Foo");
  ASSERT_TRUE(v.isNone());

  v = s.get("food");
  ASSERT_TRUE(v.isNone());

  v = s.get("");
  ASSERT_TRUE(v.isNone());
}

TEST(LookupTest, LookupShortObjectCompact) {
  std::string const value(
      "{\"foo\":null,\"bar\":true,\"baz\":13.53,\"qux\":[1],\"quz\":{}}");

  Options options;
  options.buildUnindexedObjects = true;

  Parser parser(&options);
  parser.parse(value);
  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  ASSERT_EQ(0x14, s.head());

  Slice v;
  v = s.get("foo");
  ASSERT_TRUE(v.isNull());

  v = s.get("bar");
  ASSERT_TRUE(v.isBool());
  ASSERT_EQ(true, v.getBool());

  v = s.get("baz");
  ASSERT_TRUE(v.isDouble());
  ASSERT_DOUBLE_EQ(13.53, v.getDouble());

  v = s.get("qux");
  ASSERT_TRUE(v.isArray());
  ASSERT_TRUE(v.isType(ValueType::Array));
  ASSERT_EQ(1ULL, v.length());

  v = s.get("quz");
  ASSERT_TRUE(v.isObject());
  ASSERT_TRUE(v.isType(ValueType::Object));
  ASSERT_EQ(0ULL, v.length());

  // non-present attributes
  v = s.get("nada");
  ASSERT_TRUE(v.isNone());

  v = s.get(std::string("foo\0", 4));
  ASSERT_TRUE(v.isNone());

  v = s.get("Foo");
  ASSERT_TRUE(v.isNone());

  v = s.get("food");
  ASSERT_TRUE(v.isNone());

  v = s.get("");
  ASSERT_TRUE(v.isNone());
}

TEST(LookupTest, LookupSubattributes) {
  std::string const value(
      "{\"foo\":{\"bar\":1,\"bark\":[],\"baz\":{\"qux\":{\"qurz\":null}}}}");

  Parser parser;
  parser.parse(value);
  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  ASSERT_VELOCYPACK_EXCEPTION(s.get(std::vector<std::string>()),
                              Exception::InvalidAttributePath);

  Slice v;
  v = s.get(std::vector<std::string>({"foo"}));
  ASSERT_TRUE(v.isObject());

  v = s.get(std::vector<std::string>({"foo", "bar"}));
  ASSERT_TRUE(v.isNumber());
  ASSERT_EQ(1ULL, v.getUInt());

  v = s.get(std::vector<std::string>({"boo"}));
  ASSERT_TRUE(v.isNone());

  v = s.get(std::vector<std::string>({"boo", "far"}));
  ASSERT_TRUE(v.isNone());

  v = s.get(std::vector<std::string>({"foo", "bark"}));
  ASSERT_TRUE(v.isArray());

  v = s.get(std::vector<std::string>({"foo", "bark", "baz"}));
  ASSERT_TRUE(v.isNone());

  v = s.get(std::vector<std::string>({"foo", "baz"}));
  ASSERT_TRUE(v.isObject());

  v = s.get(std::vector<std::string>({"foo", "baz", "qux"}));
  ASSERT_TRUE(v.isObject());

  v = s.get(std::vector<std::string>({"foo", "baz", "qux", "qurz"}));
  ASSERT_TRUE(v.isNull());

  v = s.get(std::vector<std::string>({"foo", "baz", "qux", "qurk"}));
  ASSERT_TRUE(v.isNone());

  v = s.get(std::vector<std::string>({"foo", "baz", "qux", "qurz", "p0rk"}));
  ASSERT_TRUE(v.isNone());
}

TEST(LookupTest, LookupSubattributesCompact) {
  std::string const value(
      "{\"foo\":{\"bar\":1,\"bark\":[],\"baz\":{\"qux\":{\"qurz\":null}}}}");

  Options options;
  options.buildUnindexedObjects = true;

  Parser parser(&options);
  parser.parse(value);
  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  ASSERT_EQ(0x14, s.head());

  ASSERT_VELOCYPACK_EXCEPTION(s.get(std::vector<std::string>()),
                              Exception::InvalidAttributePath);

  Slice v;
  v = s.get(std::vector<std::string>({"foo"}));
  ASSERT_TRUE(v.isObject());
  ASSERT_EQ(0x14, v.head());

  v = s.get(std::vector<std::string>({"foo", "bar"}));
  ASSERT_TRUE(v.isNumber());
  ASSERT_EQ(1ULL, v.getUInt());

  v = s.get(std::vector<std::string>({"boo"}));
  ASSERT_TRUE(v.isNone());

  v = s.get(std::vector<std::string>({"boo", "far"}));
  ASSERT_TRUE(v.isNone());

  v = s.get(std::vector<std::string>({"foo", "bark"}));
  ASSERT_TRUE(v.isArray());

  v = s.get(std::vector<std::string>({"foo", "bark", "baz"}));
  ASSERT_TRUE(v.isNone());

  v = s.get(std::vector<std::string>({"foo", "baz"}));
  ASSERT_TRUE(v.isObject());

  v = s.get(std::vector<std::string>({"foo", "baz", "qux"}));
  ASSERT_TRUE(v.isObject());

  v = s.get(std::vector<std::string>({"foo", "baz", "qux", "qurz"}));
  ASSERT_TRUE(v.isNull());

  v = s.get(std::vector<std::string>({"foo", "baz", "qux", "qurk"}));
  ASSERT_TRUE(v.isNone());

  v = s.get(std::vector<std::string>({"foo", "baz", "qux", "qurz", "p0rk"}));
  ASSERT_TRUE(v.isNone());
}

TEST(LookupTest, LookupLongObject) {
  std::string value("{");
  for (std::size_t i = 4; i < 1024; ++i) {
    if (i > 4) {
      value.append(",");
    }
    value.append("\"test");
    value.append(std::to_string(i));
    value.append("\":");
    value.append(std::to_string(i));
  }
  value.append("}");

  Parser parser;
  parser.parse(value);
  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  Slice v;
  v = s.get("test4");
  ASSERT_TRUE(v.isNumber());
  ASSERT_EQ(4ULL, v.getUInt());

  v = s.get("test10");
  ASSERT_TRUE(v.isNumber());
  ASSERT_EQ(10ULL, v.getUInt());

  v = s.get("test42");
  ASSERT_TRUE(v.isNumber());
  ASSERT_EQ(42ULL, v.getUInt());

  v = s.get("test100");
  ASSERT_TRUE(v.isNumber());
  ASSERT_EQ(100ULL, v.getUInt());

  v = s.get("test932");
  ASSERT_TRUE(v.isNumber());
  ASSERT_EQ(932ULL, v.getUInt());

  v = s.get("test1000");
  ASSERT_TRUE(v.isNumber());
  ASSERT_EQ(1000ULL, v.getUInt());

  v = s.get("test1023");
  ASSERT_TRUE(v.isNumber());
  ASSERT_EQ(1023ULL, v.getUInt());

  // none existing
  v = s.get("test0");
  ASSERT_TRUE(v.isNone());

  v = s.get("test1");
  ASSERT_TRUE(v.isNone());

  v = s.get("test1024");
  ASSERT_TRUE(v.isNone());
}

TEST(LookupTest, LookupLongObjectCompact) {
  std::string value("{");
  for (std::size_t i = 4; i < 1024; ++i) {
    if (i > 4) {
      value.append(",");
    }
    value.append("\"test");
    value.append(std::to_string(i));
    value.append("\":");
    value.append(std::to_string(i));
  }
  value.append("}");

  Options options;
  options.buildUnindexedObjects = true;

  Parser parser(&options);
  parser.parse(value);
  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  ASSERT_EQ(0x14, s.head());

  Slice v;
  v = s.get("test4");
  ASSERT_TRUE(v.isNumber());
  ASSERT_EQ(4ULL, v.getUInt());

  v = s.get("test10");
  ASSERT_TRUE(v.isNumber());
  ASSERT_EQ(10ULL, v.getUInt());

  v = s.get("test42");
  ASSERT_TRUE(v.isNumber());
  ASSERT_EQ(42ULL, v.getUInt());

  v = s.get("test100");
  ASSERT_TRUE(v.isNumber());
  ASSERT_EQ(100ULL, v.getUInt());

  v = s.get("test932");
  ASSERT_TRUE(v.isNumber());
  ASSERT_EQ(932ULL, v.getUInt());

  v = s.get("test1000");
  ASSERT_TRUE(v.isNumber());
  ASSERT_EQ(1000ULL, v.getUInt());

  v = s.get("test1023");
  ASSERT_TRUE(v.isNumber());
  ASSERT_EQ(1023ULL, v.getUInt());

  // none existing
  v = s.get("test0");
  ASSERT_TRUE(v.isNone());

  v = s.get("test1");
  ASSERT_TRUE(v.isNone());

  v = s.get("test1024");
  ASSERT_TRUE(v.isNone());
}

TEST(LookupTest, LookupLongObjectUnsorted) {
  std::string value("{");
  for (std::size_t i = 4; i < 1024; ++i) {
    if (i > 4) {
      value.append(",");
    }
    value.append("\"test");
    value.append(std::to_string(i));
    value.append("\":");
    value.append(std::to_string(i));
  }
  value.append("}");

  Parser parser;
  parser.parse(value);
  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  Slice v;
  v = s.get("test4");
  ASSERT_TRUE(v.isNumber());
  ASSERT_EQ(4ULL, v.getUInt());

  v = s.get("test10");
  ASSERT_TRUE(v.isNumber());
  ASSERT_EQ(10ULL, v.getUInt());

  v = s.get("test42");
  ASSERT_TRUE(v.isNumber());
  ASSERT_EQ(42ULL, v.getUInt());

  v = s.get("test100");
  ASSERT_TRUE(v.isNumber());
  ASSERT_EQ(100ULL, v.getUInt());

  v = s.get("test932");
  ASSERT_TRUE(v.isNumber());
  ASSERT_EQ(932ULL, v.getUInt());

  v = s.get("test1000");
  ASSERT_TRUE(v.isNumber());
  ASSERT_EQ(1000ULL, v.getUInt());

  v = s.get("test1023");
  ASSERT_TRUE(v.isNumber());
  ASSERT_EQ(1023ULL, v.getUInt());

  // none existing
  v = s.get("test0");
  ASSERT_TRUE(v.isNone());

  v = s.get("test1");
  ASSERT_TRUE(v.isNone());

  v = s.get("test1024");
  ASSERT_TRUE(v.isNone());
}

TEST(LookupTest, LookupLinear) {
  std::string value("{");
  for (std::size_t i = 0; i < 4; ++i) {
    if (i > 0) {
      value.append(",");
    }
    value.append("\"test");
    value.append(std::to_string(i));
    value.append("\":");
    value.append(std::to_string(i));
  }
  value.append("}");

  Parser parser;
  parser.parse(value);
  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  Slice v;
  v = s.get("test0");
  ASSERT_TRUE(v.isNumber());
  ASSERT_EQ(0ULL, v.getUInt());

  v = s.get("test1");
  ASSERT_TRUE(v.isNumber());
  ASSERT_EQ(1ULL, v.getUInt());

  v = s.get("test2");
  ASSERT_TRUE(v.isNumber());
  ASSERT_EQ(2ULL, v.getUInt());

  v = s.get("test3");
  ASSERT_TRUE(v.isNumber());
  ASSERT_EQ(3ULL, v.getUInt());
}

TEST(LookupTest, LookupBinary) {
  std::string value("{");
  for (std::size_t i = 0; i < 128; ++i) {
    if (i > 0) {
      value.append(",");
    }
    value.append("\"test");
    value.append(std::to_string(i));
    value.append("\":");
    value.append(std::to_string(i));
  }
  value.append("}");

  Parser parser;
  parser.parse(value);
  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  for (std::size_t i = 0; i < 128; ++i) {
    std::string key = "test";
    key.append(std::to_string(i));
    Slice v = s.get(key);

    ASSERT_TRUE(v.isNumber());
    ASSERT_EQ(i, v.getUInt());
  }
}

TEST(LookupTest, LookupBinaryCompact) {
  std::string value("{");
  for (std::size_t i = 0; i < 128; ++i) {
    if (i > 0) {
      value.append(",");
    }
    value.append("\"test");
    value.append(std::to_string(i));
    value.append("\":");
    value.append(std::to_string(i));
  }
  value.append("}");

  Options options;
  options.buildUnindexedObjects = true;

  Parser parser(&options);
  parser.parse(value);
  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  ASSERT_EQ(0x14, s.head());

  for (std::size_t i = 0; i < 128; ++i) {
    std::string key = "test";
    key.append(std::to_string(i));
    Slice v = s.get(key);

    ASSERT_TRUE(v.isNumber());
    ASSERT_EQ(i, v.getUInt());
  }
}

TEST(LookupTest, LookupBinarySamePrefix) {
  std::string value("{");
  for (std::size_t i = 0; i < 128; ++i) {
    if (i > 0) {
      value.append(",");
    }
    value.append("\"test");
    for (std::size_t j = 0; j < i; ++j) {
      value.append("x");
    }
    value.append("\":");
    value.append(std::to_string(i));
  }
  value.append("}");

  Parser parser;
  parser.parse(value);
  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  for (std::size_t i = 0; i < 128; ++i) {
    std::string key = "test";
    for (std::size_t j = 0; j < i; ++j) {
      key.append("x");
    }
    Slice v = s.get(key);

    ASSERT_TRUE(v.isNumber());
    ASSERT_EQ(i, v.getUInt());
  }
}

TEST(LookupTest, LookupBinaryLongObject) {
  std::string value("{");
  for (std::size_t i = 0; i < 1127; ++i) {
    if (i > 0) {
      value.append(",");
    }
    value.append("\"test");
    value.append(std::to_string(i));
    value.append("\":");
    value.append(std::to_string(i));
  }
  value.append("}");

  Parser parser;
  parser.parse(value);
  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  for (std::size_t i = 0; i < 1127; ++i) {
    std::string key = "test";
    key.append(std::to_string(i));
    Slice v = s.get(key);

    ASSERT_TRUE(v.isNumber());
    ASSERT_EQ(i, v.getUInt());
  }
}

TEST(LookupTest, LookupInvalidTypeNull) {
  std::string const value("null");

  Parser parser;
  parser.parse(value);
  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  ASSERT_VELOCYPACK_EXCEPTION(s.get("test"), Exception::InvalidValueType);
}

TEST(LookupTest, LookupInvalidTypeArray) {
  std::string const value("[]");

  Parser parser;
  parser.parse(value);
  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  ASSERT_VELOCYPACK_EXCEPTION(s.get("test"), Exception::InvalidValueType);
}

TEST(LookupTest, AtNull) {
  std::string const value("null");

  Parser parser;
  parser.parse(value);
  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  ASSERT_VELOCYPACK_EXCEPTION(s.at(0), Exception::InvalidValueType);
}

TEST(LookupTest, AtObject) {
  std::string const value("{}");

  Parser parser;
  parser.parse(value);
  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  ASSERT_VELOCYPACK_EXCEPTION(s.at(0), Exception::InvalidValueType);
}

TEST(LookupTest, AtArray) {
  std::string const value("[1,2,3,4]");

  Parser parser;
  parser.parse(value);
  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  ASSERT_EQ(1, s.at(0).getInt());
  ASSERT_EQ(2, s.at(1).getInt());
  ASSERT_EQ(3, s.at(2).getInt());
  ASSERT_EQ(4, s.at(3).getInt());

  ASSERT_VELOCYPACK_EXCEPTION(s.at(4), Exception::IndexOutOfBounds);
}

TEST(LookupTest, AtArrayCompact) {
  std::string const value("[1,2,3,4]");

  Options options;
  options.buildUnindexedArrays = true;

  Parser parser(&options);
  parser.parse(value);
  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  ASSERT_EQ(0x13, s.head());

  ASSERT_EQ(1, s.at(0).getInt());
  ASSERT_EQ(2, s.at(1).getInt());
  ASSERT_EQ(3, s.at(2).getInt());
  ASSERT_EQ(4, s.at(3).getInt());

  ASSERT_VELOCYPACK_EXCEPTION(s.at(4), Exception::IndexOutOfBounds);
}

TEST(LookupTest, AtArrayEmpty) {
  std::string const value("[]");

  Parser parser;
  parser.parse(value);
  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  ASSERT_VELOCYPACK_EXCEPTION(s.at(0), Exception::IndexOutOfBounds);
  ASSERT_VELOCYPACK_EXCEPTION(s.at(1), Exception::IndexOutOfBounds);
}

TEST(LookupTest, KeyAtArray) {
  std::string const value("[]");

  Parser parser;
  parser.parse(value);
  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  ASSERT_VELOCYPACK_EXCEPTION(s.keyAt(0), Exception::InvalidValueType);
}

TEST(LookupTest, KeyAtNull) {
  std::string const value("null");

  Parser parser;
  parser.parse(value);
  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  ASSERT_VELOCYPACK_EXCEPTION(s.keyAt(0), Exception::InvalidValueType);
}

TEST(LookupTest, KeyAtObject) {
  std::string const value("{\"foo\":1,\"bar\":2,\"baz\":3,\"qux\":4}");

  Parser parser;
  parser.parse(value);
  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  ASSERT_EQ("bar", s.keyAt(0).copyString());
  ASSERT_EQ("baz", s.keyAt(1).copyString());
  ASSERT_EQ("foo", s.keyAt(2).copyString());
  ASSERT_EQ("qux", s.keyAt(3).copyString());

  ASSERT_VELOCYPACK_EXCEPTION(s.keyAt(4), Exception::IndexOutOfBounds);
}

TEST(LookupTest, KeyAtObjectSorted) {
  std::string const value("{\"foo\":1,\"bar\":2,\"baz\":3,\"qux\":4}");

  Parser parser;
  parser.parse(value);
  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  ASSERT_EQ("bar", s.keyAt(0).copyString());
  ASSERT_EQ("baz", s.keyAt(1).copyString());
  ASSERT_EQ("foo", s.keyAt(2).copyString());
  ASSERT_EQ("qux", s.keyAt(3).copyString());

  ASSERT_VELOCYPACK_EXCEPTION(s.keyAt(4), Exception::IndexOutOfBounds);
}

TEST(LookupTest, KeyAtObjectCompact) {
  std::string const value("{\"foo\":1,\"bar\":2,\"baz\":3,\"qux\":4}");

  Options options;
  options.buildUnindexedObjects = true;

  Parser parser(&options);
  parser.parse(value);
  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  ASSERT_EQ(0x14, s.head());

  ASSERT_EQ("foo", s.keyAt(0).copyString());
  ASSERT_EQ("bar", s.keyAt(1).copyString());
  ASSERT_EQ("baz", s.keyAt(2).copyString());
  ASSERT_EQ("qux", s.keyAt(3).copyString());

  ASSERT_VELOCYPACK_EXCEPTION(s.keyAt(4), Exception::IndexOutOfBounds);
}

TEST(LookupTest, KeyAtObjectEmpty) {
  std::string const value("{}");

  Parser parser;
  parser.parse(value);
  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  ASSERT_VELOCYPACK_EXCEPTION(s.keyAt(0), Exception::IndexOutOfBounds);
  ASSERT_VELOCYPACK_EXCEPTION(s.keyAt(1), Exception::IndexOutOfBounds);
}

TEST(LookupTest, ValueAtArray) {
  std::string const value("[]");

  Parser parser;
  parser.parse(value);
  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  ASSERT_VELOCYPACK_EXCEPTION(s.valueAt(0), Exception::InvalidValueType);
}

TEST(LookupTest, ValueAtNull) {
  std::string const value("null");

  Parser parser;
  parser.parse(value);
  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  ASSERT_VELOCYPACK_EXCEPTION(s.valueAt(0), Exception::InvalidValueType);
}

TEST(LookupTest, ValueAtObject) {
  std::string const value("{\"foo\":1,\"bar\":2,\"baz\":3,\"qux\":4}");

  Parser parser;
  parser.parse(value);
  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  ASSERT_EQ(2, s.valueAt(0).getInt());
  ASSERT_EQ(3, s.valueAt(1).getInt());
  ASSERT_EQ(1, s.valueAt(2).getInt());
  ASSERT_EQ(4, s.valueAt(3).getInt());

  ASSERT_VELOCYPACK_EXCEPTION(s.valueAt(4), Exception::IndexOutOfBounds);
}

TEST(LookupTest, ValueAtObjectSorted) {
  std::string const value("{\"foo\":1,\"bar\":2,\"baz\":3,\"qux\":4}");

  Parser parser;
  parser.parse(value);
  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  ASSERT_EQ(2, s.valueAt(0).getInt());
  ASSERT_EQ(3, s.valueAt(1).getInt());
  ASSERT_EQ(1, s.valueAt(2).getInt());
  ASSERT_EQ(4, s.valueAt(3).getInt());

  ASSERT_VELOCYPACK_EXCEPTION(s.valueAt(4), Exception::IndexOutOfBounds);
}

TEST(LookupTest, ValueAtObjectCompact) {
  std::string const value("{\"foo\":1,\"bar\":2,\"baz\":3,\"qux\":4}");

  Options options;
  options.buildUnindexedObjects = true;

  Parser parser(&options);
  parser.parse(value);
  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  ASSERT_EQ(0x14, s.head());

  ASSERT_EQ(1, s.valueAt(0).getInt());
  ASSERT_EQ(2, s.valueAt(1).getInt());
  ASSERT_EQ(3, s.valueAt(2).getInt());
  ASSERT_EQ(4, s.valueAt(3).getInt());

  ASSERT_VELOCYPACK_EXCEPTION(s.valueAt(4), Exception::IndexOutOfBounds);
}

TEST(LookupTest, ValueAtObjectEmpty) {
  std::string const value("{}");

  Parser parser;
  parser.parse(value);
  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  ASSERT_VELOCYPACK_EXCEPTION(s.valueAt(0), Exception::IndexOutOfBounds);
  ASSERT_VELOCYPACK_EXCEPTION(s.valueAt(1), Exception::IndexOutOfBounds);
}

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
